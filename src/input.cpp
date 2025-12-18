#include "input.h"
#include <Arduino.h>
#include <cmath>
#include <AudioTools.h>
#include "config.h"

namespace {
class Sn74hc151Mux {
public:
  void begin();
  int readChannel(uint8_t channel);

private:
  void selectChannel(uint8_t channel);
  bool initialized = false;
  uint8_t lastChannel = 0xFF;
};

Sn74hc151Mux gInputMux;

void Sn74hc151Mux::begin() {
  if (initialized) return;
  pinMode(INPUT_MUX_PIN_A, OUTPUT);
  pinMode(INPUT_MUX_PIN_B, OUTPUT);
  pinMode(INPUT_MUX_PIN_C, OUTPUT);
  if (INPUT_MUX_PIN_EN >= 0) {
    pinMode(INPUT_MUX_PIN_EN, OUTPUT);
    digitalWrite(INPUT_MUX_PIN_EN, LOW);
  }
  pinMode(INPUT_MUX_PIN_Y, INPUT);
  selectChannel(0);
  initialized = true;
}

void Sn74hc151Mux::selectChannel(uint8_t channel) {
  channel &= 0x07;
  if (channel == lastChannel) return;
  digitalWrite(INPUT_MUX_PIN_A, channel & 0x01);
  digitalWrite(INPUT_MUX_PIN_B, (channel >> 1) & 0x01);
  digitalWrite(INPUT_MUX_PIN_C, (channel >> 2) & 0x01);
  lastChannel = channel;
}

int Sn74hc151Mux::readChannel(uint8_t channel) {
  if (!initialized) begin();
  selectChannel(channel);
  delayMicroseconds(INPUT_MUX_SETTLE_TIME_US);
  return digitalRead(INPUT_MUX_PIN_Y);
}
} // namespace

void initInputMux() {
  gInputMux.begin();
}

bool readMuxActiveState(uint8_t channel, bool activeLow) {
  int level = gInputMux.readChannel(channel);
  return activeLow ? (level == LOW) : (level == HIGH);
}

// Implementations for Button
Button::Button(int pinOrChannel, const char* samplePath, bool activeLow, bool useMultiplexer)
  : pin(pinOrChannel), samplePath(samplePath), activeLow(activeLow), useMultiplexer(useMultiplexer) {}

void Button::begin() {
  if (!useMultiplexer) {
    // Configure internal pull resistor depending on activeLow.
    // - activeLow == true: pressed == LOW, enable INPUT_PULLUP
    // - activeLow == false: pressed == HIGH, enable INPUT_PULLDOWN (ESP32)
    #if defined(INPUT_PULLDOWN)
      pinMode(pin, activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
    #endif
  }
  rawState = false;
  debouncedState = false;
  lastDebounceTime = 0;
  lastTriggerTime = 0;
  latched = false;
}

bool Button::update(uint32_t now) {
  bool raw = readPressedHardware();
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
        if (Serial) {
          Serial.print(F("Button pressed: "));
          Serial.println(samplePath ? samplePath : "<unnamed>");
        }
        return true;
      }
    } else {
      latched = false;
    }
  }
  return false;
}

void Button::release() { latched = false; lastTriggerTime = 0; }

void Button::sync(uint32_t now) {
  bool raw = readPressedHardware();
  rawState = raw;
  debouncedState = raw;
  latched = raw;
  lastDebounceTime = now;
  lastTriggerTime = raw ? now : 0;
}

bool Button::readRaw() const {
  return readPressedHardware();
}
bool Button::isLatched() const { return latched; }
const char* Button::getPath() const { return samplePath; }

bool Button::readPressedHardware() const {
  if (useMultiplexer) {
    return readMuxActiveState(static_cast<uint8_t>(pin), activeLow);
  }
  int level = digitalRead(pin);
  return activeLow ? (level == LOW) : (level == HIGH);
}

// VolumeManager implementations
VolumeManager::VolumeManager(int adcPin)
  : adcPin(adcPin), cachedVolumeControl(expoControl) {}

extern AudioPlayer player; // defined in bankrasampler.cpp

namespace {
float normalizeVolumeFromAdc(int raw) {
  const float adcMax = 4095.0f;
  float normalized = static_cast<float>(raw) / adcMax;
  if (POT_POLARITY_INVERTED) {
    normalized = 1.0f - normalized;
  }
  return constrain(normalized, 0.0f, 1.0f);
}
}

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
  float alpha = constrain(LOW_PASS_CUTOFF_SMOOTH_ALPHA, 0.0f, 1.0f);
  if (smoothedCutoffHz < 0.0f || alpha <= 0.0f) {
    smoothedCutoffHz = target;
  } else {
    smoothedCutoffHz += alpha * (target - smoothedCutoffHz);
  }
  if (!cutoffCallback) return;
  const float cutoffDeadband = LOW_PASS_CUTOFF_DEADBAND_HZ;
  if (lastCutoffHz < 0.0f || fabs(smoothedCutoffHz - lastCutoffHz) >= cutoffDeadband) {
    lastCutoffHz = smoothedCutoffHz;
    cutoffCallback(smoothedCutoffHz);
  }
}

float VolumeManager::mapNormalizedToCutoff(float normalized) const {
  float minHz = LOW_PASS_MIN_HZ;
  float maxHz = LOW_PASS_MAX_HZ;
  float clamped = constrain(normalized, 0.0f, 1.0f);
  return minHz + (maxHz - minHz) * clamped;
}
