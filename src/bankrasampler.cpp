#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <AudioTools.h>
#include "AudioTools/Disk/AudioSourceSD.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include <ScopeI2SStream.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include "AudioTools/CoreAudio/AudioEffects/AudioEffects.h"
#include "config.h"
#include "audio_mixer.h"
#include "input.h"

// Audio stack
AudioSourceSD source("/", "wav");
I2SStream i2s;
WAVDecoder wavDecoder;
AudioPlayer player(source, i2s, wavDecoder);
DryWetMixerStream mixerStream;
Delay delayEffect;
DryWetMixerStream* DryWetMixerStream::s_instance = nullptr;

// Display & scope (moved to ui module)
#include "ui.h"

// State
int activeButtonIndex = -1;
String currentSamplePath = "";
float currentFilterCutoffHz = MASTER_LOW_PASS_CUTOFF_HZ;

// Switch state (pin 27, one side to GND -> use INPUT_PULLUP; LOW = ON)
bool switchRawState = false;
bool switchDebouncedState = false;
uint32_t switchLastDebounceTime = 0;

// Filter switch (pin 26) state
bool filterSwitchRawState = false;
bool filterSwitchDebouncedState = false;
uint32_t filterSwitchLastDebounceTime = 0;

float normalizeVolumeFromAdc(int raw) {
  const float adcMax = 4095.0f;
  float v = 1.0f - ((float)raw / adcMax);
  return constrain(v, 0.0f, 1.0f);
}

// Button and VolumeManager moved to input.h / input.cpp

Button buttons[BUTTON_COUNT] = {
  Button(BUTTON_PINS[0], "/1.wav", BUTTONS_ACTIVE_LOW),
  Button(BUTTON_PINS[1], "/2.wav", BUTTONS_ACTIVE_LOW),
  Button(BUTTON_PINS[2], "/3.wav", BUTTONS_ACTIVE_LOW),
  Button(BUTTON_PINS[3], "/4.wav", BUTTONS_ACTIVE_LOW),
  Button(BUTTON_PINS[4], "/5.wav", BUTTONS_ACTIVE_LOW),
  Button(BUTTON_PINS[5], "/6.wav", BUTTONS_ACTIVE_LOW),
};

VolumeManager volume(POT_PIN);


// Audio/display init helpers
void initSd() {
  SPI.begin();
  if (!SD.begin(5, SPI, 80000000UL)) {
    Serial.println("Card failed, or not present");
    while (1);
  }
}

void initDisplay() {
  if (!initUi()) {
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
  mixerStream.configureMasterCompressor(MASTER_COMPRESSOR_ATTACK_MS,
                                        MASTER_COMPRESSOR_RELEASE_MS,
                                        MASTER_COMPRESSOR_HOLD_MS,
                                        MASTER_COMPRESSOR_THRESHOLD_PERCENT,
                                        MASTER_COMPRESSOR_RATIO,
                                        MASTER_COMPRESSOR_ENABLED);
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

void applyFilterSwitchState(bool enabled) {
  mixerStream.configureMasterLowPass(currentFilterCutoffHz,
                                     MASTER_LOW_PASS_Q, enabled);
}

// play helper
bool playSampleForButton(size_t idx) {
  if (idx >= BUTTON_COUNT) return false;
  const char* path = buttons[idx].getPath();
  if (path == nullptr || path[0] == '\0') {
    Serial.println("Geen geldig pad om af te spelen");
    return false;
  }
  String full = String(path);
  if (full.charAt(0) != '/') full = String("/") + full;
  if (!player.setPath(full.c_str())) {
    Serial.printf("Kon bestand %s niet openen\n", full.c_str());
    return false;
  }
  currentSamplePath = full;
  player.play();
  // No per-play attack fade: the delay always runs and sending is controlled
  // by the hardware switch via setSendActive().
  activeButtonIndex = (int)idx;
  return true;
}

// Setup & loop (slim)
void setup() {
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  for (size_t i = 0; i < BUTTON_COUNT; ++i) buttons[i].begin();

  // init switch pin
  pinMode(SWITCH_PIN_DELAY_SEND, INPUT_PULLUP);
  bool init = (digitalRead(SWITCH_PIN_DELAY_SEND) == LOW);
  switchRawState = switchDebouncedState = init;

  pinMode(SWITCH_PIN_ENABLE_FILTER, INPUT_PULLUP);
  bool filterInit = (digitalRead(SWITCH_PIN_ENABLE_FILTER) == LOW);
  filterSwitchRawState = filterSwitchDebouncedState = filterInit;

  initSd();
  initDisplay();
  initAudio();
  volume.begin();
  volume.setCutoffUpdateCallback([](float cutoffHz) {
    currentFilterCutoffHz = cutoffHz;
    mixerStream.setInputLowPassCutoff(cutoffHz);
  });
  volume.setFilterControlActive(filterSwitchDebouncedState);
  volume.forceImmediateSample();
  // Keep the effect audible by default, but control whether we send audio
  // into the delay via the hardware switch (setSendActive).
  mixerStream.setEffectActive(true);
  mixerStream.setSendActive(switchDebouncedState);
  applyFilterSwitchState(filterSwitchDebouncedState);
}

void loop() {
  uint32_t now = millis();
  volume.update(now);

  // Read switch (debounced) - pin wired to GND on one side; LOW = ON
  bool raw = (digitalRead(SWITCH_PIN_DELAY_SEND) == LOW);
  if (raw != switchRawState) {
    switchLastDebounceTime = now;
    switchRawState = raw;
  }
  if ((now - switchLastDebounceTime) > BUTTON_DEBOUNCE_MS && raw != switchDebouncedState) {
    switchDebouncedState = raw;
    // Switch now controls whether we send audio into the delay line.
    mixerStream.setSendActive(switchDebouncedState);
  }

  bool filterRaw = (digitalRead(SWITCH_PIN_ENABLE_FILTER) == LOW);
  if (filterRaw != filterSwitchRawState) {
    filterSwitchLastDebounceTime = now;
    filterSwitchRawState = filterRaw;
  }
  if ((now - filterSwitchLastDebounceTime) > BUTTON_DEBOUNCE_MS &&
      filterRaw != filterSwitchDebouncedState) {
    filterSwitchDebouncedState = filterRaw;
    applyFilterSwitchState(filterSwitchDebouncedState);
    volume.setFilterControlActive(filterSwitchDebouncedState);
    volume.forceImmediateSample();
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
        // don't clear delay buffer here; allow tail to decay and stay visible
        buttons[i].release();
        activeButtonIndex = -1;
      }
    }
  }

  // Audio copy + housekeeping
  player.copy();
  // When player is inactive, pump a small amount of silence through the
  // mixer so delay/feedback buffers continue to advance and tails decay.
  if (!player.isActive()) {
    // push 64 frames (tweak if needed)
    mixerStream.pumpSilenceFrames(64);
  }
  // Als een sample klaar is: verbreek alleen de associatie met activeButtonIndex
  // maar release de latched state niet automatisch. Daardoor blijft een ingedrukte
  // knop latched en veroorzaakt geen her-trigger totdat je loslaat en opnieuw
  // indrukt. Dit zorgt dat "ingedrukt houden" geen onverwachte herstarten geeft.
  if (!player.isActive() && activeButtonIndex >= 0) {
    // sample finished: release latched state so next press works cleanly
    buttons[activeButtonIndex].release();
    activeButtonIndex = -1;
  }

  // Update display state (handled by UI module)
  bool currentPlayingState = player.isActive();
  updateUi(currentPlayingState, currentSamplePath);
}
