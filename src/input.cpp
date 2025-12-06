#include "input.h"
#include <Arduino.h>
#include <cmath>
#include <AudioTools.h>
#include "config.h"

// Implementations for Button
Button::Button(int pin, const char* samplePath, bool activeLow)
  : pin(pin), samplePath(samplePath), activeLow(activeLow) {}

void Button::begin() {
  // Configure internal pull resistor depending on activeLow.
  // - activeLow == true: pressed == LOW, enable INPUT_PULLUP
  // - activeLow == false: pressed == HIGH, enable INPUT_PULLDOWN (ESP32)
  #if defined(INPUT_PULLDOWN)
    pinMode(pin, activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
  #endif
  rawState = false;
  debouncedState = false;
  lastDebounceTime = 0;
  lastTriggerTime = 0;
  latched = false;
}

bool Button::update(uint32_t now) {
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

void Button::release() { latched = false; lastTriggerTime = 0; }
bool Button::isLatched() const { return latched; }
const char* Button::getPath() const { return samplePath; }

// VolumeManager implementations
VolumeManager::VolumeManager(int adcPin)
  : adcPin(adcPin), cachedVolumeControl(expoControl) {}

extern AudioPlayer player; // defined in bankrasampler.cpp

float normalizeVolumeFromAdc(int raw);

void VolumeManager::begin() {
  pinMode(adcPin, INPUT);
  lastSampleTime = 0;
  lastVolume = normalizeVolumeFromAdc(analogRead(adcPin));
  rampVolume = lastVolume;
  float curved = applyVolumeCurve(lastVolume);
  rampVolume = curved;
  player.setVolume(curved);
}

void VolumeManager::update(uint32_t now) {
  if ((now - lastSampleTime) < VOLUME_READ_INTERVAL_MS) return;
  lastSampleTime = now;
  float normalized = normalizeVolumeFromAdc(analogRead(adcPin));
  if (currentMode == Mode::Cutoff) {
    handleCutoffMode(normalized);
  } else {
    handleVolumeMode(normalized);
  }
}

void VolumeManager::setFilterControlActive(bool active) {
  Mode newMode = active ? Mode::Cutoff : Mode::Volume;
  if (currentMode == newMode) return;
  currentMode = newMode;
  if (currentMode == Mode::Cutoff) {
    smoothedCutoffHz = -1.0f;
    lastCutoffHz = -1.0f;
  } else {
    lastVolume = -1.0f;
  }
}

void VolumeManager::setCutoffUpdateCallback(CutoffCallback cb) {
  cutoffCallback = cb;
}

void VolumeManager::forceImmediateSample() { lastSampleTime = 0; }

float VolumeManager::applyVolumeCurve(float input) {
  return cachedVolumeControl.getVolumeFactor(constrain(input, 0.0f, 1.0f));
}

void VolumeManager::handleVolumeMode(float normalized) {
  float target = applyVolumeCurve(normalized);
  if (lastVolume < 0.0f || fabs(target - lastVolume) >= VOLUME_DEADBAND) {
    lastVolume = target;
  }
  float rampStep = 0.05f;
  if (fabs(rampVolume - lastVolume) > rampStep) {
    if (rampVolume < lastVolume) rampVolume += rampStep;
    else rampVolume -= rampStep;
    rampVolume = constrain(rampVolume, 0.0f, 1.0f);
    player.setVolume(rampVolume);
  } else {
    rampVolume = lastVolume;
    player.setVolume(rampVolume);
  }
}

void VolumeManager::handleCutoffMode(float normalized) {
  float target = mapNormalizedToCutoff(normalized);
  float alpha = constrain(MASTER_LOW_PASS_CUTOFF_SMOOTH_ALPHA, 0.0f, 1.0f);
  if (smoothedCutoffHz < 0.0f || alpha <= 0.0f) {
    smoothedCutoffHz = target;
  } else {
    smoothedCutoffHz += alpha * (target - smoothedCutoffHz);
  }
  if (!cutoffCallback) return;
  const float cutoffDeadband = MASTER_LOW_PASS_CUTOFF_DEADBAND_HZ;
  if (lastCutoffHz < 0.0f || fabs(smoothedCutoffHz - lastCutoffHz) >= cutoffDeadband) {
    lastCutoffHz = smoothedCutoffHz;
    cutoffCallback(smoothedCutoffHz);
  }
}

float VolumeManager::mapNormalizedToCutoff(float normalized) const {
  float minHz = MASTER_LOW_PASS_MIN_HZ;
  float maxHz = MASTER_LOW_PASS_MAX_HZ;
  float clamped = constrain(normalized, 0.0f, 1.0f);
  return minHz + (maxHz - minHz) * clamped;
}
