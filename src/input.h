// input.h - Button and VolumeManager declarations
#pragma once

#include <Arduino.h>
#include <functional>
#include "AudioTools/CoreAudio/VolumeControl.h"

class Button {
public:
  Button(int pin, const char* samplePath, bool activeLow = true);
  void begin();
  bool update(uint32_t now);
  void release();
  bool isLatched() const;
  const char* getPath() const;

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

class VolumeManager {
public:
  VolumeManager(int adcPin);
  void begin();
  void update(uint32_t now);
  void setFilterControlActive(bool active);
  using CutoffCallback = std::function<void(float)>;
  void setCutoffUpdateCallback(CutoffCallback cb);
  void forceImmediateSample();
private:
  enum class Mode { Volume, Cutoff };
  Mode currentMode = Mode::Volume;
  int adcPin;
  uint32_t lastSampleTime = 0;
  float lastVolume = -1.0f;
  float rampVolume = -1.0f;
  float lastCutoffHz = -1.0f;
  float smoothedCutoffHz = -1.0f;
  CutoffCallback cutoffCallback;
  audio_tools::ExponentialVolumeControl expoControl;
  audio_tools::CachedVolumeControl cachedVolumeControl;
  float applyVolumeCurve(float input);
  float mapNormalizedToCutoff(float normalized) const;
  void handleVolumeMode(float normalized);
  void handleCutoffMode(float normalized);
};
