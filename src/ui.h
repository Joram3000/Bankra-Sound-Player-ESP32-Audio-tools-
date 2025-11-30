// ui.h — display and oscilloscope wrapper
#pragma once

#include <Adafruit_SSD1306.h>
#include <ScopeI2SStream.h>
#include <ScopeDisplay.h>

// Expose the scoped I2S stream so initAudio() can configure it.
extern ScopeI2SStream scopeI2s;

// Initialize display and scope. Returns true on success.
bool initUi();

// Update UI state (called from main loop)
void updateUi(bool playing, const String& filename);
