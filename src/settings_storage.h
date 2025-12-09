#pragma once

class SettingsScreenU8g2;

// Loads persisted settings from the SD card into the provided settings screen.
void loadSettingsFromSd(SettingsScreenU8g2* settingsScreen);

// Saves the current settings from the provided settings screen to the SD card.
void saveSettingsToSd(const SettingsScreenU8g2* settingsScreen);
