// Project-wide configuration constants
#pragma once

#include <cstddef>
#include <cstdint>
#include <array>


// make screen configurable here, wether it is u8g2 or Adafruit_SSD1306

constexpr std::array<int, 4> BUTTON_PINS = {13, 4, 16, 17, };
constexpr size_t BUTTON_COUNT = BUTTON_PINS.size();
constexpr bool BUTTONS_ACTIVE_LOW = true;
constexpr int SWITCH_PIN = 27;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_RETRIGGER_GUARD_MS = 20;
constexpr uint32_t BUTTON_FADE_MS = 12;
constexpr uint32_t EFFECT_TOGGLE_FADE_MS = 6;
constexpr uint32_t SAMPLE_ATTACK_FADE_MS = 10;
constexpr int VOLUME_POT_PIN = 34;
constexpr uint32_t VOLUME_READ_INTERVAL_MS = 30;
constexpr float VOLUME_DEADBAND = 0.12f;

// --- Additional hardware pins for new features ---

constexpr int SD_CS_PIN    = 5;  // already in use by SD
// SPI bus (shared with SD)
// Note: SCK/MOSI/MISO are the VSPI defaults used by SD.begin()
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

// Encoder (A/B + switch) - using input-only pins: external pull-ups required
constexpr int ENC_PIN_A = 35;
constexpr int ENC_PIN_B = 36;
constexpr int ENC_PIN_SW = 39;

// Extra inputs: 2 extra buttons + 1 extra switch
constexpr std::array<int, 2> EXTRA_BUTTON_PINS = {37, 38};
constexpr int EXTRA_SWITCH_PIN = 22;

// Notes:
// - GPIO34..39 are input-only (geen OUTPUT, geen interne pull-ups). Gebruik externe pull-ups.
// - Avoid using pins 6..11 (flash), and be cautious with 0,2,12 (boot straps).
