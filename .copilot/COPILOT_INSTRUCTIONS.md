# 🤖 Copilot Instructies — ESP32 Sample Player

## 🎯 Projectdoel
Ontwikkel een **sound sample player** op een **ESP32** met 4 drukknoppen, een **I2S DAC (UDA1334A)**, en een **SD-kaartmodule**.  
Bij indrukken van een knop speelt de bijbehorende sample af, en bij loslaten stopt deze onmiddellijk.  
Systeem moet **polyfoon** zijn en **minimale latency** hebben.

---

## ⚙️ Hardware

| Component | Beschrijving |
|------------|---------------|
| ESP32 | Hoofdcontroller met I2S-ondersteuning |
| GME12864-41 | 128x64 OLED-display (I2C) |
| SD-kaartmodule | MicroSD via SPI |
| UDA1334A | I2S DAC audio-uitgang |
| 4 drukknoppen | Momentary switches met interne pull-up |

---

## 📦 Libraries

Gebruik de volgende Arduino libraries:

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <AudioTools.h>
#include <AudioLibs/AudioKit.h>
#include <AudioLibs/AudioPlayer.h>
// Optioneel display:
#include <U8g2lib.h>  // of <Adafruit_SSD1306.h>

Gedragsspecificatie

Startup:

Initialiseer SD, DAC, OLED.

Toon "Sample Player Ready" op het scherm.

Knoppen:

Gebruik 4 digitale inputs met INPUT_PULLUP.

Detecteer pressed (LOW) en released (HIGH) events.

Audio:

Elk samplebestand heet sample1.wav, sample2.wav, sample3.wav, sample4.wav.

Bij indrukken: start afspelen van sample.

Bij loslaten: stop sample.

Meerdere samples tegelijk toegestaan (polyfoon).

Weergave:

Toon op OLED welke samples actief zijn.

Bijv.:

Playing:
[1] ON
[2] OFF
[3] ON
[4] OFF


Performance:

Optimaliseer buffers en I2S-config voor lage latency.

Gebruik AudioPlayer of AudioGeneratorWAV uit AudioTools.

🧠 Verwachting van Copilot

Copilot moet code genereren die:

Een robuuste AudioPlayer setup maakt met I2SStream.

Een array van 4 samples beheert, elk met:

struct SampleSlot {
    const char* filename;
    int buttonPin;
    bool isPlaying;
};


loop() gebruikt om knoppen te scannen en play/stop aan te roepen.

playSample(int index) en stopSample(int index) implementeert.

OLED bijwerkt bij statusverandering.

Foutmeldingen logt via Serial.

🚀 Uitbreidingsopties

Later uitbreiden met:

Volume per sample (DAC gain).

Loop-modus (toggle).

VU-meter of waveform op OLED.

MIDI-triggering via USB of DIN.