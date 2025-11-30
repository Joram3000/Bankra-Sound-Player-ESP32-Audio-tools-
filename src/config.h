// Project-wide configuration constants
#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

constexpr std::array<int, 4> BUTTON_PINS = {13, 4, 16, 17};
constexpr size_t BUTTON_COUNT = BUTTON_PINS.size();
constexpr int SWITCH_PIN = 27;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 20;
constexpr uint32_t BUTTON_RETRIGGER_GUARD_MS = 20;
constexpr uint32_t BUTTON_FADE_MS = 12;
constexpr uint32_t EFFECT_TOGGLE_FADE_MS = 6;
constexpr uint32_t SAMPLE_ATTACK_FADE_MS = 10;
constexpr int VOLUME_POT_PIN = 34;
constexpr uint32_t VOLUME_READ_INTERVAL_MS = 30;
constexpr float VOLUME_DEADBAND = 0.12f;
