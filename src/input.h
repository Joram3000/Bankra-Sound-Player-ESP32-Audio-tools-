// input.h - Button and VolumeManager declarations
#pragma once

#include <Arduino.h>

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
private:
  int adcPin;
  uint32_t lastSampleTime = 0;
  float lastVolume = -1.0f;
  float rampVolume = -1.0f;
};
