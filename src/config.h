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
constexpr int NUM_WAVEFORM_SAMPLES = DISPLAY_WIDTH * 2;
constexpr uint8_t DISPLAY_I2C_ADDRESS = 0x3C;
constexpr bool DISPLAY_INVERT_COLORS = false;

#if DISPLAY_DRIVER == DISPLAY_DRIVER_U8G2_SSD1306
	// Pas deze macro's aan als je een andere U8g2 constructor nodig hebt
	#ifndef DISPLAY_U8G2_CLASS
		// Gebruik de SH1106 constructor voor DIYUSER 1.3 (SH1106) modules.
		// Als je een echte SSD1306-module hebt, zet deze terug naar
		// U8G2_SSD1306_128X64_NONAME_F_HW_I2C
		#define DISPLAY_U8G2_CLASS U8G2_SH1106_128X64_NONAME_F_HW_I2C
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
constexpr int SWITCH_PIN_SETTINGS_MODE = 35; // new pin for entering settings mode on boot
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_RETRIGGER_GUARD_MS = 20;
 constexpr uint32_t BUTTON_FADE_MS = 12;
constexpr uint32_t EFFECT_TOGGLE_FADE_MS = 6;
constexpr uint32_t SAMPLE_ATTACK_FADE_MS = 10;
constexpr int POT_PIN = 34;
constexpr uint32_t VOLUME_READ_INTERVAL_MS = 30;
constexpr float VOLUME_DEADBAND = 0.12f;

// -----------------------------------------------------------------------------
// Audio mixer / delay defaults exposed to UI and storage
// -----------------------------------------------------------------------------
constexpr float DEFAULT_DELAY_TIME_MS    = 420.0f;
constexpr float DEFAULT_DELAY_DEPTH      = 0.40f;
constexpr float DEFAULT_DELAY_FEEDBACK   = 0.45f;

constexpr float DELAY_TIME_MIN_MS        = 50.0f;
constexpr float DELAY_TIME_MAX_MS        = 2000.0f;
constexpr float DELAY_TIME_STEP_MS       = 10.0f;

constexpr float DELAY_DEPTH_MIN          = 0.0f;
constexpr float DELAY_DEPTH_MAX          = 1.0f;
constexpr float DELAY_DEPTH_STEP         = 0.02f;

constexpr float DELAY_FEEDBACK_MIN       = 0.0f;
constexpr float DELAY_FEEDBACK_MAX       = 0.95f;
constexpr float DELAY_FEEDBACK_STEP      = 0.02f;

constexpr float MIXER_DRY_MIN            = 0.0f;
constexpr float MIXER_DRY_MAX            = 1.0f;
constexpr float MIXER_DRY_STEP           = 0.02f;
constexpr float MIXER_DEFAULT_DRY_LEVEL  = 1.0f;

constexpr float MIXER_WET_MIN            = 0.0f;
constexpr float MIXER_WET_MAX            = 1.0f;
constexpr float MIXER_WET_STEP           = 0.02f;
constexpr float MIXER_DEFAULT_WET_LEVEL  = 0.75f;

// FILTER SETTINGS
constexpr float LOW_PASS_CUTOFF_HZ = 500.0f;
constexpr float LOW_PASS_Q         = 0.8071f;
constexpr float LOW_PASS_MIN_HZ    = 300.0f;
constexpr float LOW_PASS_MAX_HZ    = 4500.0f;
constexpr float LOW_PASS_STEP_HZ   = 25.0f;
constexpr float LOW_PASS_CUTOFF_SMOOTH_ALPHA = 0.48f; // 0..1
constexpr float LOW_PASS_CUTOFF_DEADBAND_HZ  = 4.0f;
constexpr float LOW_PASS_Q_MIN      = 0.2f;
constexpr float LOW_PASS_Q_MAX      = 2.5f;
constexpr float LOW_PASS_Q_STEP     = 0.05f;

constexpr float FILTER_SLEW_MIN_HZ_PER_SEC      = 100.0f;
constexpr float FILTER_SLEW_MAX_HZ_PER_SEC      = 20000.0f;
constexpr float FILTER_SLEW_STEP_HZ_PER_SEC     = 100.0f;
constexpr float FILTER_SLEW_DEFAULT_HZ_PER_SEC  = 8000.0f;

// Master bus compression (gentle glue on final output)
constexpr bool     MASTER_COMPRESSOR_ENABLED          = true;
constexpr uint16_t MASTER_COMPRESSOR_ATTACK_MS        = 12;
constexpr uint16_t MASTER_COMPRESSOR_RELEASE_MS       = 70;
constexpr uint16_t MASTER_COMPRESSOR_HOLD_MS          = 12;
constexpr uint8_t  MASTER_COMPRESSOR_THRESHOLD_PERCENT= 18;  // relative to full-scale
constexpr float    MASTER_COMPRESSOR_RATIO            = 0.75f; // 0..1 (lower = stronger)

constexpr uint16_t MASTER_COMPRESSOR_ATTACK_MIN_MS    = 1;
constexpr uint16_t MASTER_COMPRESSOR_ATTACK_MAX_MS    = 100;
constexpr uint16_t MASTER_COMPRESSOR_ATTACK_STEP_MS   = 1;

constexpr uint16_t MASTER_COMPRESSOR_RELEASE_MIN_MS   = 10;
constexpr uint16_t MASTER_COMPRESSOR_RELEASE_MAX_MS   = 500;
constexpr uint16_t MASTER_COMPRESSOR_RELEASE_STEP_MS  = 5;

constexpr uint16_t MASTER_COMPRESSOR_HOLD_MIN_MS      = 0;
constexpr uint16_t MASTER_COMPRESSOR_HOLD_MAX_MS      = 100;
constexpr uint16_t MASTER_COMPRESSOR_HOLD_STEP_MS     = 1;

constexpr float    MASTER_COMPRESSOR_THRESHOLD_MIN    = 0.0f;
constexpr float    MASTER_COMPRESSOR_THRESHOLD_MAX    = 100.0f;
constexpr float    MASTER_COMPRESSOR_THRESHOLD_STEP   = 1.0f;

constexpr float    MASTER_COMPRESSOR_RATIO_MIN        = 0.1f;
constexpr float    MASTER_COMPRESSOR_RATIO_MAX        = 1.0f;
constexpr float    MASTER_COMPRESSOR_RATIO_STEP       = 0.05f;

// zoom screen defaults
constexpr float DEFAULT_HORIZ_ZOOM = 8.0f; //>1 = inzoomen (minder samples weergegeven), <1 = uitzoomen
constexpr float DEFAULT_VERT_SCALE = 2.0f; // amplitude schaal factor
constexpr float ZOOM_MIN = 0.5f;
constexpr float ZOOM_MAX = 40.0f;
constexpr float ZOOM_STEP = 0.1f;
constexpr float ZOOM_BIG_STEP = 0.5f;

// Settings menu rendering
constexpr uint8_t SETTINGS_VISIBLE_MENU_ITEMS = 6;


// --- Additional hardware pins for new features ---
constexpr int SD_CS_PIN    = 5;  // already in use by SD
constexpr int SPI_MOSI_PIN = 23; // MOSI (shared)
constexpr int SPI_SCK_PIN  = 18; // SCLK (shared)
constexpr int SPI_MISO_PIN = 19; // MISO (shared)
constexpr int I2S_PIN_BCK  = 14;
constexpr int I2S_PIN_WS   = 15;
constexpr int I2S_PIN_DATA = 32;

