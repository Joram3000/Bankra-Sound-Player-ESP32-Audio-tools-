// we hebben nu een audio player, scope en 2 samples
// in de toekomst moeten dat 4 samples worden en effecten er bij
// de code moet zo clean mogelijk blijven zodat het goed begrijpelijk is wat er gebeurd


#include <SPI.h>                    // SPI communicatie voor SD-kaart
#include <SD.h>                     // SD-kaart functionaliteit
#include <Adafruit_SSD1306.h>       // OLED display driver
#include <Wire.h>                   // I2C communicatie voor display
#include <Adafruit_GFX.h>           // Grafische functies voor display
#include <AudioTools.h>             // Audio verwerking bibliotheek
#include "AudioTools/Disk/AudioSourceSD.h"      // SD-kaart audio bron
#include "AudioTools/AudioCodecs/CodecWAV.h" // WAV decoder
#include <ScopeI2SStream.h>         // Custom I2S stream met scope functionaliteit
#include <ScopeDisplay.h>           // OLED scope display manager

const char *startFilePath="/";
const char* ext="wav";
AudioSourceSD source(startFilePath, ext);
I2SStream i2s;
WAVDecoder wavDecoder;
AudioPlayer player(source, i2s, wavDecoder);

// Chip Select pin voor de SD-kaart module en bedieningsknoppen
const int SD_CS = 5;
const int PLAY_BUTTON_PIN = 13;
const int AUX_BUTTON_PIN = 4; // gereserveerd voor toekomstige functies
// array van buttonPins
const int BUTTON_PINS[] = {13, 4};
const uint32_t BUTTON_DEBOUNCE_MS = 20;
const uint32_t BUTTON_RETRIGGER_GUARD_MS = 80; // minimale tijd tussen herstarts
const uint32_t BUTTON_FADE_MS = 25; // gewenst fade in/out venster

// Analoge volumeregeling
const int VOLUME_POT_PIN = 34;             // ESP32 ADC-pin voor potmeter (AJUSTEER indien nodig)
const uint32_t VOLUME_READ_INTERVAL_MS = 30; // hoe vaak we de pot meten
const float VOLUME_DEADBAND = 0.02f;         // vermijd jitter bij kleine veranderingen

// Display en waveform setup
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int16_t waveformBuffer[WAVEFORM_SAMPLES];
int waveformIndex = 0;

// Scope display manager
ScopeDisplay scopeDisplay(&display, waveformBuffer, &waveformIndex);

// Scope I2S stream (gebruikt display mutex voor thread-safety)
ScopeI2SStream scopeI2s(waveformBuffer, &waveformIndex, scopeDisplay.getMutex());

struct ButtonState {
  int pin;
  const char* samplePath;
  bool rawState;
  bool debouncedState;
  bool latched;
  uint32_t lastDebounceTime;
  uint32_t lastTriggerTime;
};

ButtonState buttonStates[] = {
  {PLAY_BUTTON_PIN, "/1.wav", false, false, false, 0, 0},
  {AUX_BUTTON_PIN,  "/2.wav", false, false, false, 0, 0}
};

const size_t BUTTON_COUNT = sizeof(buttonStates) / sizeof(buttonStates[0]);
int activeButtonIndex = -1;
String currentSamplePath = "";
float lastVolumeLevel = -1.0f;
uint32_t lastVolumeSampleTime = 0;

String makeAbsolutePath(const char* path) {
  if (!path) {
    return "";
  }
  String fullPath = path;
  if (fullPath.startsWith("/")) {
    return fullPath;
  }
  String basePath = startFilePath ? String(startFilePath) : "/";
  if (basePath.length() == 0) {
    basePath = "/";
  }
  if (!basePath.startsWith("/")) {
    basePath = "/" + basePath;
  }
  if (!basePath.endsWith("/")) {
    basePath += '/';
  }
  return basePath + fullPath;
}

float normalizeVolumeFromAdc(int raw) {
  const float adcMax = 4095.0f; // ESP32 ADC 12-bit
  float volume = static_cast<float>(raw) / adcMax;
  if (volume < 0.0f) volume = 0.0f;
  if (volume > 1.0f) volume = 1.0f;
  return volume;
}

void updateVolumeFromPot(uint32_t now) {
  if ((now - lastVolumeSampleTime) < VOLUME_READ_INTERVAL_MS) {
    return;
  }
  lastVolumeSampleTime = now;

  int raw = analogRead(VOLUME_POT_PIN);
  float targetVolume = normalizeVolumeFromAdc(raw);

  if (lastVolumeLevel < 0.0f) {
    lastVolumeLevel = targetVolume;
    player.setVolume(targetVolume);
    return;
  }

  float diff = targetVolume - lastVolumeLevel;
  if (diff < 0.0f) diff = -diff;
  if (diff >= VOLUME_DEADBAND) {
    lastVolumeLevel = targetVolume;
    player.setVolume(targetVolume);
  }
}

bool playSampleForButton(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= (int)BUTTON_COUNT) {
    return false;
  }

  ButtonState &btn = buttonStates[buttonIndex];
  String fullPath = makeAbsolutePath(btn.samplePath);
  if (fullPath.isEmpty()) {
    Serial.println("Geen geldig pad om af te spelen");
    return false;
  }

  if (!player.setPath(fullPath.c_str())) {
    Serial.printf("Kon bestand %s niet openen\n", fullPath.c_str());
    return false;
  }

  currentSamplePath = fullPath;
  player.play();
  activeButtonIndex = buttonIndex;
  return true;
}

void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");

  if (!str || len <= 0) {
    Serial.println();
    return;
  }

  const int MAX_MD = 128;
  int n = len;
  if (n > MAX_MD) n = MAX_MD;
  
  static char buf[MAX_MD + 1];
  memcpy(buf, str, n);
  buf[n] = '\0';
  Serial.println(buf);
  
  // Update display titel via ScopeDisplay
  if(type == Title) {
    scopeDisplay.setFilename(String(buf));
  }
}

void setup() {
  pinMode(PLAY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(AUX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(VOLUME_POT_PIN, INPUT);
  
  Serial.begin(115200);
  // Houd het logniveau laag zodat Serial I/O de audio niet onderbreekt
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  SPI.begin();
  if (!SD.begin(SD_CS, SPI, 80000000UL)) {
    Serial.println("Card failed, or not present");
    while (1);
  }

  // Initialiseer scope display
  if(!scopeDisplay.begin(0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Configureer I2S audio output met scope stream
  auto cfg = scopeI2s.defaultConfig(TX_MODE);
  cfg.pin_bck = 14;
  cfg.pin_ws  = 15;
  cfg.pin_data = 32;
  scopeI2s.begin(cfg);
  
  // Gebruik scopeI2s in plaats van i2s
  player.setOutput(scopeI2s);
  
  // Metadata callback registreren
  player.setMetadataCallback(printMetaData);
  player.setSilenceOnInactive(true); // houd uitgang stil wanneer niet actief
  player.setAutoNext(false);        // blijf op dezelfde sample
  player.setDelayIfOutputFull(0);   // voorkom 100ms pauzes bij volle I2S buffer
  player.setFadeTime(BUTTON_FADE_MS); // zorg voor ~7ms fade in/out tegen klikken
  player.begin();
  player.stop(); // start stil totdat de knop wordt ingedrukt

  // Initialiseer volume volgens potmeter
  lastVolumeLevel = normalizeVolumeFromAdc(analogRead(VOLUME_POT_PIN));
  player.setVolume(lastVolumeLevel);
  
  Serial.println("Setup complete");
}

void loop() {
  uint32_t now = millis();
  updateVolumeFromPot(now);

  // --- BUTTON LOGIC HIER (debounce + triggers) ---
  for (size_t i = 0; i < BUTTON_COUNT; ++i) {
    ButtonState &btn = buttonStates[i];
    bool rawState = digitalRead(btn.pin) == LOW;
    if (rawState != btn.rawState) {
      btn.lastDebounceTime = now;
      btn.rawState = rawState;
    }

    if ((now - btn.lastDebounceTime) > BUTTON_DEBOUNCE_MS && rawState != btn.debouncedState) {
      btn.debouncedState = rawState;
      if (btn.debouncedState) {
        if (!btn.latched && (now - btn.lastTriggerTime) > BUTTON_RETRIGGER_GUARD_MS) {
          if (playSampleForButton((int)i)) {
            btn.latched = true;
            btn.lastTriggerTime = now;
          }
        }
      } else {
        btn.latched = false;
        if (activeButtonIndex == (int)i) {
          player.stop();
          activeButtonIndex = -1;
        }
      }
    }
  }

  // Audio playback op core 1
  player.copy();

  // Als het afspelen gestopt is, maak de knop meteen helemaal vrij
  if (!player.isActive() && activeButtonIndex >= 0) {
    ButtonState &btn = buttonStates[activeButtonIndex];
    btn.latched = false;
    // Zorg dat retrigger guard nooit meer blokkeert na natuurlijk einde:
    btn.lastTriggerTime = 0;  // of: now - BUTTON_RETRIGGER_GUARD_MS;
    activeButtonIndex = -1;
  }

  // Update playing status EN bestandsnaam
  static bool lastPlayingState = false;
  static String lastFileName = "";
  
  bool currentPlayingState = player.isActive();
  String currentFileName = currentSamplePath;
  if (currentFileName.isEmpty()) {
    currentFileName = "-";
  }
  
  if(currentPlayingState != lastPlayingState || currentFileName != lastFileName) {
    lastPlayingState = currentPlayingState;
    lastFileName = currentFileName;
    scopeDisplay.setPlaying(currentPlayingState);
    scopeDisplay.setFilename(currentFileName);
  }
}
