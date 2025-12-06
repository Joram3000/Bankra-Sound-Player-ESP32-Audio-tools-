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
constexpr int NUM_WAVEFORM_SAMPLES = DISPLAY_WIDTH;
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
constexpr std::array<int, 4> BUTTON_PINS = {13, 4, 16, 17};
constexpr size_t BUTTON_COUNT = BUTTON_PINS.size();
constexpr bool BUTTONS_ACTIVE_LOW = true;
constexpr int SWITCH_PIN_DELAY_SEND = 27;
constexpr int SWITCH_PIN_ENABLE_FILTER = 26;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_RETRIGGER_GUARD_MS = 20;
constexpr uint32_t BUTTON_FADE_MS = 12;
constexpr uint32_t EFFECT_TOGGLE_FADE_MS = 6;
constexpr uint32_t SAMPLE_ATTACK_FADE_MS = 10;
constexpr int VOLUME_POT_PIN = 34;
constexpr uint32_t VOLUME_READ_INTERVAL_MS = 30;
constexpr float VOLUME_DEADBAND = 0.12f;

// Master output filtering
constexpr bool  MASTER_LOW_PASS_ENABLED   = true;
constexpr float MASTER_LOW_PASS_CUTOFF_HZ = 1200.0f;
constexpr float MASTER_LOW_PASS_Q         = 0.9071f;

// --- Additional hardware pins for new features ---
constexpr int SD_CS_PIN    = 5;  // already in use by SD
constexpr int SPI_SCK_PIN  = 18; // SCLK (shared)
constexpr int SPI_MOSI_PIN = 23; // MOSI (shared)
constexpr int SPI_MISO_PIN = 19; // MISO (shared)

// Color TFT (SPI) - module pins: SCL(SCLK), SDA(MOSI), RES, DC, CS, BLK
constexpr int TFT_SCLK_PIN = SPI_SCK_PIN;
constexpr int TFT_MOSI_PIN = SPI_MOSI_PIN;
constexpr int TFT_CS_PIN   = 25; // chip select for TFT (choose free GPIO)
constexpr int TFT_DC_PIN   = 26; // data/command
constexpr int TFT_RST_PIN  = 33; // reset
constexpr int TFT_BLK_PIN  = 21; // backlight control (PWM optional)

// Extra inputs: 2 extra buttons + 1 extra switch
constexpr std::array<int, 2> EXTRA_BUTTON_PINS = {37, 38};

// Notes:
// - GPIO34..39 are input-only (geen OUTPUT, geen interne pull-ups). Gebruik externe pull-ups.
// - Avoid using pins 6..11 (flash), and be cautious with 0,2,12 (boot straps).
