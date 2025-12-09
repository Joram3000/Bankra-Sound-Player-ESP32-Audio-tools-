// ui.h — display and oscilloscope wrapper
#pragma once

#include <ScopeI2SStream.h>

// Expose the scoped I2S stream so initAudio() can configure it.
extern ScopeI2SStream scopeI2s;

// Initialize display and scope. Returns true on success.
bool initUi();

// Update UI state (called from main loop)
void updateUi(bool playing, const String& filename);

// When using the U8G2 driver, expose the underlying U8G2 instance so other
// modules (e.g. settings screens) can draw to the display using the same
// hardware object. Returns nullptr when no U8G2 display is in use.
class U8G2; // forward
U8G2* getU8g2Display();

// Expose the display mutex used by the scope display task so callers can
// safely take the mutex before drawing directly. Returns nullptr when not
// available.
void* getDisplayMutex();

// Adjust scope drawing parameters from other modules
void setScopeHorizZoom(float z);

// Temporarily pause/resume the scope task when drawing custom overlays.
void setScopeDisplaySuspended(bool suspended);
