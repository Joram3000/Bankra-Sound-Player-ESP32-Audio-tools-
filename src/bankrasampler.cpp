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
#include <algorithm>
#include <cstring>
#include <vector>
#include "AudioTools/CoreAudio/AudioEffects/AudioEffects.h"

// Constants & globals (converted to clearer names / constexpr)
// constexpr const char* "/" = "/";
// constexpr const char* EXT_WAV = "wav";
// constexpr int SD_CS_PIN = 5;
constexpr int BUTTON_PINS[] = {13, 4, 16, 17};
constexpr size_t BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
constexpr int SWITCH_PIN = 27;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_RETRIGGER_GUARD_MS = 20;
constexpr uint32_t BUTTON_FADE_MS = 12;
constexpr uint32_t EFFECT_TOGGLE_FADE_MS = 6;
constexpr uint32_t SAMPLE_ATTACK_FADE_MS = 10;
constexpr int VOLUME_POT_PIN = 34;
constexpr uint32_t VOLUME_READ_INTERVAL_MS = 30;
constexpr float VOLUME_DEADBAND = 0.12f;

// Audio stack helpers
class DryWetMixerStream : public AudioStream {
public:
  void begin(I2SStream& outStream, Delay& effect) {
    dryOutput = &outStream;
    delay = &effect;
  }

  void setMix(float dry, float wet) {
    dryMix = dry;
    wetMixActive = wet;
    targetWetMix = effectEnabled ? wetMixActive : 0.0f;
    currentWetMix = targetWetMix;
    wetRampFramesRemaining = 0;
  }

  void setAudioInfo(AudioInfo newInfo) override {
    AudioStream::setAudioInfo(newInfo);
    if (dryOutput) dryOutput->setAudioInfo(newInfo);
    sampleBytes = std::max<int>(1, newInfo.bits_per_sample / 8);
    channels = std::max<int>(1, newInfo.channels);
    frameBytes = sampleBytes * channels;
    pendingLen = 0;
    pendingBuffer.clear();
    sampleRate = newInfo.sample_rate > 0 ? newInfo.sample_rate : 44100;
    fadeFrames = std::max<uint32_t>(1, (sampleRate * EFFECT_TOGGLE_FADE_MS) / 1000);
    attackFrames = std::max<uint32_t>(1, (sampleRate * SAMPLE_ATTACK_FADE_MS) / 1000);
    targetWetMix = effectEnabled ? wetMixActive : 0.0f;
    currentWetMix = targetWetMix;
    wetRampFramesRemaining = 0;
    attackFramesRemaining = 0;
    const size_t reserveFrames = 256; // tweak if needed
    mixBuffer.clear();
    mixBuffer.reserve(reserveFrames * channels);
  }

  void setEffectActive(bool active) {
    if (delay) delay->setActive(active);
    effectEnabled = active;
    targetWetMix = effectEnabled ? wetMixActive : 0.0f;
    scheduleWetRamp();
  }

  void updateEffectSampleRate(uint32_t sampleRate) {
    if (delay && sampleRate > 0) {
      delay->setSampleRate(sampleRate);
    }
  }

  void triggerAttackFade() {
    attackFramesRemaining = attackFrames;
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!dryOutput || !delay || len == 0) return 0;
    if (sampleBytes != sizeof(int16_t)) {
      // Unsupported format; just pass through dry
      return dryOutput->write(data, len);
    }

    size_t processed = 0;
    while (len > 0) {
      if (pendingLen > 0 || len < frameBytes) {
        size_t needed = frameBytes - pendingLen;
        size_t copyLen = std::min(needed, len);
        if (pendingBuffer.size() < frameBytes) pendingBuffer.resize(frameBytes);
        memcpy(pendingBuffer.data() + pendingLen, data, copyLen);
        pendingLen += copyLen;
        data += copyLen;
        len -= copyLen;
        if (pendingLen < frameBytes) break;
        mixAndWrite(pendingBuffer.data(), frameBytes);
        pendingLen = 0;
        processed += frameBytes;
        continue;
      }

      size_t chunkLen = (len / frameBytes) * frameBytes;
      if (chunkLen == 0) break;
      mixAndWrite(data, chunkLen);
      data += chunkLen;
      len -= chunkLen;
      processed += chunkLen;
    }

    return processed;
  }

private:
  I2SStream* dryOutput = nullptr;
  Delay* delay = nullptr;
  float dryMix = 1.0f;
  float wetMixActive = 0.35f;
  float currentWetMix = 0.0f;
  float targetWetMix = 0.0f;
  float wetRampDelta = 0.0f;
  int sampleBytes = sizeof(int16_t);
  int channels = 2;
  std::vector<int16_t> mixBuffer;
  std::vector<uint8_t> pendingBuffer;
  size_t pendingLen = 0;
  size_t frameBytes = sizeof(int16_t) * 2;
  uint32_t sampleRate = 44100;
  uint32_t fadeFrames = 1;
  uint32_t wetRampFramesRemaining = 0;
  bool effectEnabled = false;
  uint32_t attackFrames = 1;
  uint32_t attackFramesRemaining = 0;

  void mixAndWrite(const uint8_t* chunk, size_t chunkLen) {
    size_t frames = chunkLen / frameBytes;
    if (frames == 0) return;
    mixBuffer.resize(frames * channels);
    const int16_t* input = reinterpret_cast<const int16_t*>(chunk);
    int16_t* mixed = mixBuffer.data();

    for (size_t frame = 0; frame < frames; ++frame) {
      int32_t mono = 0;
      for (int ch = 0; ch < channels; ++ch) {
        mono += input[frame * channels + ch];
      }
      mono /= channels;

      effect_t wetSample = delay->process(static_cast<effect_t>(mono));
      float wetLevel = advanceWetMix();
      float attackGain = advanceAttackGain();

      for (int ch = 0; ch < channels; ++ch) {
        int32_t dryVal = input[frame * channels + ch];
        int32_t mixedVal = static_cast<int32_t>(dryMix * dryVal + wetLevel * wetSample);
        if (attackGain < 0.999f) {
          mixedVal = static_cast<int32_t>(mixedVal * attackGain);
        }
        if (mixedVal > 32767) mixedVal = 32767;
        if (mixedVal < -32768) mixedVal = -32768;
        mixed[frame * channels + ch] = static_cast<int16_t>(mixedVal);
      }
    }

    dryOutput->write(reinterpret_cast<uint8_t*>(mixed), frames * frameBytes);
  }

  void scheduleWetRamp() {
    if (fadeFrames <= 1) {
      currentWetMix = targetWetMix;
      wetRampFramesRemaining = 0;
      wetRampDelta = 0.0f;
      return;
    }
    wetRampFramesRemaining = fadeFrames;
    wetRampDelta = (targetWetMix - currentWetMix) / static_cast<float>(fadeFrames);
  }

  float advanceWetMix() {
    if (wetRampFramesRemaining > 0) {
      currentWetMix += wetRampDelta;
      --wetRampFramesRemaining;
      if ((wetRampDelta > 0.0f && currentWetMix > targetWetMix) ||
          (wetRampDelta < 0.0f && currentWetMix < targetWetMix)) {
        currentWetMix = targetWetMix;
        wetRampFramesRemaining = 0;
        wetRampDelta = 0.0f;
      }
    } else {
      currentWetMix = targetWetMix;
    }
    return currentWetMix;
  }

  float advanceAttackGain() {
    if (attackFramesRemaining == 0) return 1.0f;
    float gain = 1.0f - (static_cast<float>(attackFramesRemaining) / static_cast<float>(attackFrames));
    --attackFramesRemaining;
    if (attackFramesRemaining == 0) return 1.0f;
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    return gain;
  }
};

// Audio stack
AudioSourceSD source("/", "wav");
I2SStream i2s;
WAVDecoder wavDecoder;
AudioPlayer player(source, i2s, wavDecoder);
DryWetMixerStream mixerStream;
Delay delayEffect;

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
// (Inlined path handling where needed; `makeAbsolutePath` removed because
// the project always uses the same base path.)

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
    rampVolume = lastVolume;
    player.setVolume(lastVolume);
  }

  void update(uint32_t now) {
    if ((now - lastSampleTime) < VOLUME_READ_INTERVAL_MS) return;
    lastSampleTime = now;
    float target = normalizeVolumeFromAdc(analogRead(adcPin));
    if (lastVolume < 0.0f || fabs(target - lastVolume) >= VOLUME_DEADBAND) {
      lastVolume = target;
    }
    // Ramp volume towards target
    float rampStep = 0.8f; // Adjust for speed of ramp (smaller = slower)
    if (fabs(rampVolume - lastVolume) > rampStep) {
      if (rampVolume < lastVolume) rampVolume += rampStep;
      else rampVolume -= rampStep;
      player.setVolume(rampVolume);
    } else {
      rampVolume = lastVolume;
      player.setVolume(rampVolume);
    }
  }

private:
  int adcPin;
  uint32_t lastSampleTime = 0;
  float lastVolume = -1.0f;
  float rampVolume = -1.0f;
};

Button buttons[BUTTON_COUNT] = {
  Button(BUTTON_PINS[0], "/1.wav", false), // false = push-to-break (pressed reads HIGH)
  Button(BUTTON_PINS[1], "/2.wav", false),
  Button(BUTTON_PINS[2], "/3.wav", false),
  Button(BUTTON_PINS[3], "/4.wav", false),
};

VolumeManager volume(VOLUME_POT_PIN);

// Metadata callback
// void printMetaData(MetaDataType type, const char* str, int len){
//   Serial.print("==> ");
//   Serial.print(toStr(type));
//   Serial.print(": ");
//   if (!str || len <= 0) { Serial.println(); return; }
//   const int MAX_MD = 128;
//   int n = min(len, MAX_MD);
//   static char buf[MAX_MD + 1];
//   memcpy(buf, str, n);
//   buf[n] = '\0';
//   Serial.println(buf);
//   if(type == Title) scopeDisplay.setFilename(String(buf));
// }

// Audio/display init helpers
void initSd() {
  SPI.begin();
  if (!SD.begin(5, SPI, 80000000UL)) {
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
  mixerStream.begin(scopeI2s, delayEffect);
  uint32_t effectiveSampleRate = cfg.sample_rate > 0 ? cfg.sample_rate : 44100;
  AudioInfo mixInfo;
  mixInfo.sample_rate = effectiveSampleRate;
  mixInfo.channels = cfg.channels > 0 ? cfg.channels : 2;
  mixInfo.bits_per_sample = cfg.bits_per_sample > 0 ? cfg.bits_per_sample : 16;
  mixerStream.setAudioInfo(mixInfo);
  mixerStream.updateEffectSampleRate(effectiveSampleRate);
  mixerStream.setMix(1.0f, 0.75f);
  delayEffect.setDuration(420);      // milliseconds
  delayEffect.setDepth(0.40f);       // wet mix ratio handled in mixer
  delayEffect.setFeedback(0.45f);    // repeats
  player.setOutput(mixerStream);
  // player.setMetadataCallback(printMetaData);
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
  // Build full path inline. If `path` is absolute (starts with '/'), use it
  // directly; otherwise prefix with "/".
  String full;
  if (!path || strlen(path) == 0) {
    full = "";
  } else {
    full = String(path);
    if (!full.startsWith("/")) {
      String base = "/" ? String("/") : "/";
      if (!base.endsWith("/")) base += '/';
      full = base + full;
    }
  }
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
  mixerStream.triggerAttackFade();
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
  mixerStream.setEffectActive(switchDebouncedState);

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
  mixerStream.setEffectActive(switchDebouncedState);
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
