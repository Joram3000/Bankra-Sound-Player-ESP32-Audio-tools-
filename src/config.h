// Project-wide configuration constants
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// -----------------------------------------------------------------------------
// Display selection & geometry
// -----------------------------------------------------------------------------
#define DISPLAY_DRIVER_ADAFRUIT_SSD1306 0
#define DISPLAY_DRIVER_U8G2_SSD1306     1

// Pick which display backend to compile (see ui.cpp for usage)
#define DISPLAY_DRIVER DISPLAY_DRIVER_U8G2_SSD1306
constexpr int DISPLAY_WIDTH  = 128;
constexpr int DISPLAY_HEIGHT = 64;
constexpr int NUM_WAVEFORM_SAMPLES = DISPLAY_WIDTH / 2;
constexpr uint8_t DISPLAY_I2C_ADDRESS = 0x3C;
constexpr bool DISPLAY_INVERT_COLORS = false;

#if DISPLAY_DRIVER == DISPLAY_DRIVER_U8G2_SSD1306
	// Pas deze macro's aan als je een andere U8g2 constructor nodig hebt
	#ifndef DISPLAY_U8G2_CLASS
		#define DISPLAY_U8G2_CLASS U8G2_SSD1306_128X64_NONAME_F_HW_I2C
	#endif
	#ifndef DISPLAY_U8G2_CTOR_ARGS
		#define DISPLAY_U8G2_CTOR_ARGS U8G2_R0, U8X8_PIN_NONE
	#endif
#endif

// -----------------------------------------------------------------------------
// User controls
// -----------------------------------------------------------------------------
constexpr std::array<int, 6> BUTTON_PINS = {13, 4, 16, 17, 12, 25};
constexpr size_t BUTTON_COUNT = BUTTON_PINS.size();
constexpr bool BUTTONS_ACTIVE_LOW = true;
constexpr int SWITCH_PIN_DELAY_SEND = 27;
constexpr int SWITCH_PIN_ENABLE_FILTER = 26;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_RETRIGGER_GUARD_MS = 20;
constexpr uint32_t BUTTON_FADE_MS = 12;
constexpr uint32_t EFFECT_TOGGLE_FADE_MS = 6;
constexpr uint32_t SAMPLE_ATTACK_FADE_MS = 10;
constexpr int POT_PIN = 34;
constexpr uint32_t VOLUME_READ_INTERVAL_MS = 30;
constexpr float VOLUME_DEADBAND = 0.12f;

// Master output filtering
constexpr bool  MASTER_LOW_PASS_ENABLED   = true;
constexpr float MASTER_LOW_PASS_CUTOFF_HZ = 500.0f;
constexpr float MASTER_LOW_PASS_Q         = 0.4071f;
constexpr float MASTER_LOW_PASS_MIN_HZ    = 20.0f;
constexpr float MASTER_LOW_PASS_MAX_HZ    = 6000.0f;
constexpr float MASTER_LOW_PASS_CUTOFF_SMOOTH_ALPHA = 0.48f; // 0..1
constexpr float MASTER_LOW_PASS_CUTOFF_DEADBAND_HZ  = 4.0f;

// Master bus compression (gentle glue on final output)
constexpr bool     MASTER_COMPRESSOR_ENABLED          = true;
constexpr uint16_t MASTER_COMPRESSOR_ATTACK_MS        = 12;
constexpr uint16_t MASTER_COMPRESSOR_RELEASE_MS       = 140;
constexpr uint16_t MASTER_COMPRESSOR_HOLD_MS          = 16;
constexpr uint8_t  MASTER_COMPRESSOR_THRESHOLD_PERCENT= 18;  // relative to full-scale
constexpr float    MASTER_COMPRESSOR_RATIO            = 0.45f; // 0..1 (lower = stronger)

// --- Additional hardware pins for new features ---
constexpr int SD_CS_PIN    = 5;  // already in use by SD
constexpr int SPI_SCK_PIN  = 18; // SCLK (shared)
constexpr int SPI_MOSI_PIN = 23; // MOSI (shared)
constexpr int SPI_MISO_PIN = 19; // MISO (shared)

