#include <SPI.h>
#include <SD.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <AudioTools.h>
#include "AudioTools/Disk/AudioSourceSD.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include <ScopeI2SStream.h>
#include <ScopeDisplay.h>

// Constants & globals (converted to clearer names / constexpr)
constexpr const char* START_FILE_PATH = "/";
constexpr const char* EXT_WAV = "wav";
constexpr int SD_CS_PIN = 5;
constexpr int BUTTON_PINS[] = {13, 4, 16, 17};
constexpr size_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
constexpr int SWITCH_PIN = 27;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_RETRIGGER_GUARD_MS = 80;
constexpr uint32_t BUTTON_FADE_MS = 25;
constexpr int VOLUME_POT_PIN = 34;
constexpr uint32_t VOLUME_READ_INTERVAL_MS = 30;
constexpr float VOLUME_DEADBAND = 0.02f;

// Audio stack
AudioSourceSD source(START_FILE_PATH, EXT_WAV);
I2SStream i2s;
WAVDecoder wavDecoder;
AudioPlayer player(source, i2s, wavDecoder);

// Display & scope
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int16_t waveformBuffer[WAVEFORM_SAMPLES];
int waveformIndex = 0;
ScopeDisplay scopeDisplay(&display, waveformBuffer, &waveformIndex);
ScopeI2SStream scopeI2s(waveformBuffer, &waveformIndex, scopeDisplay.getMutex());

// State
int activeButtonIndex = -1;
String currentSamplePath = "";

// Switch state (pin 27, one side to GND -> use INPUT_PULLUP; LOW = ON)
bool switchRawState = false;
bool switchDebouncedState = false;
uint32_t switchLastDebounceTime = 0;

// Helpers
String makeAbsolutePath(const char* path) {
  if (!path) return "";
  String full = path;
  if (full.startsWith("/")) return full;
  String base = START_FILE_PATH ? String(START_FILE_PATH) : "/";
  if (!base.endsWith("/")) base += '/';
  return base + full;
}

float normalizeVolumeFromAdc(int raw) {
  const float adcMax = 4095.0f;
  float v = 1.0f - ((float)raw / adcMax);
  return constrain(v, 0.0f, 1.0f);
}

// Encapsulated Button class
class Button {
public:
  Button(int pin, const char* samplePath, bool activeLow = true)
    : pin(pin), samplePath(samplePath), activeLow(activeLow) {
    pinMode(pin, INPUT_PULLUP);
  }

  void begin() {
    rawState = debouncedState = false;
    lastDebounceTime = lastTriggerTime = 0;
    latched = false;
  }

  // returns true when this button successfully triggered playback
  bool update(uint32_t now) {
    bool raw = activeLow ? (digitalRead(pin) == LOW) : (digitalRead(pin) == HIGH);
    if (raw != rawState) {
      lastDebounceTime = now;
      rawState = raw;
    }
    if ((now - lastDebounceTime) > BUTTON_DEBOUNCE_MS && raw != debouncedState) {
      debouncedState = raw;
      if (debouncedState) {
        if (!latched && (now - lastTriggerTime) > BUTTON_RETRIGGER_GUARD_MS) {
          lastTriggerTime = now;
          latched = true;
          return true;
        }
      } else {
        latched = false;
      }
    }
    return false;
  }

  void release() { latched = false; lastTriggerTime = 0; }

  bool isLatched() const { return latched; }
  const char* getPath() const { return samplePath; }

private:
  int pin;
  const char* samplePath;
  bool activeLow = true;
  bool rawState = false;
  bool debouncedState = false;
  bool latched = false;
  uint32_t lastDebounceTime = 0;
  uint32_t lastTriggerTime = 0;
};

// Volume manager
class VolumeManager {
public:
  VolumeManager(int adcPin) : adcPin(adcPin) { pinMode(adcPin, INPUT); }

  void begin() {
    lastSampleTime = 0;
    lastVolume = normalizeVolumeFromAdc(analogRead(adcPin));
    player.setVolume(lastVolume);
  }

  void update(uint32_t now) {
    if ((now - lastSampleTime) < VOLUME_READ_INTERVAL_MS) return;
    lastSampleTime = now;
    float target = normalizeVolumeFromAdc(analogRead(adcPin));
    if (lastVolume < 0.0f || fabs(target - lastVolume) >= VOLUME_DEADBAND) {
      lastVolume = target;
      player.setVolume(target);
    }
  }

private:
  int adcPin;
  uint32_t lastSampleTime = 0;
  float lastVolume = -1.0f;
};

Button buttons[BUTTON_COUNT] = {
  Button(BUTTON_PINS[0], "/1.wav", false), // false = push-to-break (pressed reads HIGH)
  Button(BUTTON_PINS[1], "/2.wav", false),
  Button(BUTTON_PINS[2], "/3.wav", false),
  Button(BUTTON_PINS[3], "/4.wav", false),
};

VolumeManager volume(VOLUME_POT_PIN);

// Metadata callback
void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  if (!str || len <= 0) { Serial.println(); return; }
  const int MAX_MD = 128;
  int n = min(len, MAX_MD);
  static char buf[MAX_MD + 1];
  memcpy(buf, str, n);
  buf[n] = '\0';
  Serial.println(buf);
  if(type == Title) scopeDisplay.setFilename(String(buf));
}

// Audio/display init helpers
void initSd() {
  SPI.begin();
  if (!SD.begin(SD_CS_PIN, SPI, 80000000UL)) {
    Serial.println("Card failed, or not present");
    while (1);
  }
}

void initDisplay() {
  if (!scopeDisplay.begin(0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) ;
  }
}

void initAudio() {
  auto cfg = scopeI2s.defaultConfig(TX_MODE);
  cfg.pin_bck = 14;
  cfg.pin_ws  = 15;
  cfg.pin_data = 32;
  scopeI2s.begin(cfg);
  player.setOutput(scopeI2s);
  player.setMetadataCallback(printMetaData);
  player.setSilenceOnInactive(true);
  player.setAutoNext(false);
  player.setDelayIfOutputFull(0);
  player.setFadeTime(BUTTON_FADE_MS);
  player.begin();
  player.stop();
}

// play helper
bool playSampleForButton(size_t idx) {
  if (idx >= BUTTON_COUNT) return false;
  const char* path = buttons[idx].getPath();
  String full = makeAbsolutePath(path);
  if (full.isEmpty()) {
    Serial.println("Geen geldig pad om af te spelen");
    return false;
  }
  if (!player.setPath(full.c_str())) {
    Serial.printf("Kon bestand %s niet openen\n", full.c_str());
    return false;
  }
  currentSamplePath = full;
  player.play();
  activeButtonIndex = (int)idx;
  return true;
}

// Setup & loop (slim)
void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  for (size_t i = 0; i < BUTTON_COUNT; ++i) buttons[i].begin();

  // init switch pin
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  bool init = (digitalRead(SWITCH_PIN) == LOW);
  switchRawState = switchDebouncedState = init;
  Serial.print("Switch initial: ");
  Serial.println(init ? "ON" : "OFF");

  initSd();
  initDisplay();
  initAudio();
  volume.begin();

  Serial.println("Setup complete");
}

void loop() {
  uint32_t now = millis();
  volume.update(now);

  // Read switch (debounced) - pin wired to GND on one side; LOW = ON
  bool raw = (digitalRead(SWITCH_PIN) == LOW);
  if (raw != switchRawState) {
    switchLastDebounceTime = now;
    switchRawState = raw;
  }
  if ((now - switchLastDebounceTime) > BUTTON_DEBOUNCE_MS && raw != switchDebouncedState) {
    switchDebouncedState = raw;
    Serial.print("Switch: ");
    Serial.println(switchDebouncedState ? "ON" : "OFF");
    // als je iets wil triggeren bij omschakelen, voeg het hier toe
  }

  // buttons
  for (size_t i = 0; i < BUTTON_COUNT; ++i) {
    if (buttons[i].update(now)) {
      // trigger
      if (playSampleForButton(i)) {
        // nothing else here
      }
    } else {
      // als knop is losgelaten en het was de actieve knop -> stop direct
      if (!buttons[i].isLatched() && activeButtonIndex == (int)i) {
        player.stop();                         // <-- stop playback bij loslaten
        buttons[i].release();
        activeButtonIndex = -1;
      }
    }
  }

  // Audio copy + housekeeping
  player.copy();
  if (!player.isActive() && activeButtonIndex >= 0) {
    buttons[activeButtonIndex].release();
    activeButtonIndex = -1;
  }

  // Update display state if changed
  static bool lastPlayingState = false;
  static String lastFileName = "";
  bool currentPlayingState = player.isActive();
  String fn = currentSamplePath;
  if (fn.isEmpty()) fn = "-";
  if (currentPlayingState != lastPlayingState || fn != lastFileName) {
    lastPlayingState = currentPlayingState;
    lastFileName = fn;
    scopeDisplay.setPlaying(currentPlayingState);
    scopeDisplay.setFilename(fn);
  }
}
