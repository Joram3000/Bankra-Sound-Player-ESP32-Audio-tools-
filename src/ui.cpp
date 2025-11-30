// ui.cpp - display + scope implementation moved out of bankrasampler.cpp
#include "ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AudioTools.h>

// Create display and scope objects here
Adafruit_SSD1306 display(128, 64, &Wire, -1);
static int16_t waveformBuffer[WAVEFORM_SAMPLES];
static int waveformIndex = 0;
static ScopeDisplay scopeDisplay(&display, waveformBuffer, &waveformIndex);
ScopeI2SStream scopeI2s(waveformBuffer, &waveformIndex, scopeDisplay.getMutex());

bool initUi() {
  // initialize the SSD1306-backed scopeDisplay
  if (!scopeDisplay.begin(0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    return false;
  }
  return true;
}

void updateUi(bool playing, const String& filename) {
  static bool lastPlayingState = false;
  static String lastFileName = "";
  String fn = filename;
  if (fn.isEmpty()) fn = "-";
  if (playing != lastPlayingState || fn != lastFileName) {
    lastPlayingState = playing;
    lastFileName = fn;
    // TODO: actual drawing code can be added here. For now we keep state tracking.
  }
}
