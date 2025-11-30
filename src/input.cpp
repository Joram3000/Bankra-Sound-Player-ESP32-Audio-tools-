#include "input.h"
#include <Arduino.h>
#include <cmath>
#include <AudioTools.h>
#include "config.h"

// Implementations for Button
Button::Button(int pin, const char* samplePath, bool activeLow)
  : pin(pin), samplePath(samplePath), activeLow(activeLow) {}

void Button::begin() {
  pinMode(pin, INPUT_PULLUP);
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
VolumeManager::VolumeManager(int adcPin) : adcPin(adcPin) {}

extern AudioPlayer player; // defined in bankrasampler.cpp

float normalizeVolumeFromAdc(int raw);

void VolumeManager::begin() {
  pinMode(adcPin, INPUT);
  lastSampleTime = 0;
  lastVolume = normalizeVolumeFromAdc(analogRead(adcPin));
  rampVolume = lastVolume;
  player.setVolume(lastVolume);
}

void VolumeManager::update(uint32_t now) {
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
