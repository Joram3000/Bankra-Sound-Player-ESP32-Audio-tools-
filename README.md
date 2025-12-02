# ESP32 Sample Player

Een compacte, polyfone **sample player** gebouwd rond een **ESP32**, met een **I2S DAC (UDA1334A)**, een **SD‑kaartmodule**, vier knoppen en een optioneel **128×64 OLED‑display**.

Projectdoel:

* samples triggeren via vier knoppen.
* **Press = play**, **release = immediate stop**.
* Focus op **lage latency**, stabiele I2S‑output.

---

## Hardware

| Component           | Functie                         |
| ------------------- | ------------------------------- |
| ESP32               | Hoofdcontroller, I2S audio      |
| UDA1334A            | I2S DAC audio‑output            |
| SD‑kaartmodule      | Leest WAV‑samples vanaf microSD |
| GME12864‑41 OLED    | Statusdisplay (I2C)             |
| 4 momentary knoppen | Triggering (INPUT_PULLUP)       |

---

## Bestandsstructuur

De SD‑kaart bevat:

```
sample1.wav
sample2.wav
sample3.wav
sample4.wav
```

Alle bestanden moeten 16‑bit PCM WAV zijn.

---

## Benodigde libraries

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <AudioTools.h>
#include <AudioLibs/AudioKit.h>
#include <AudioLibs/AudioPlayer.h>
// Optioneel display
#include <U8g2lib.h>   // of Adafruit_SSD1306
```

---

## Softwaregedrag

### Startup

* Initialiseer SD, DAC (I2S), en OLED.
* Toon een korte statusmelding (“Sample Player Ready”).

### Knoppen

* Config: `INPUT_PULLUP`, LOW = ingedrukt.
* Detecteer **edge events** (pressed/released).

### Audio‐afhandeling

* Elke knop triggert een eigen sample.
* Structuur:

```cpp
struct SampleSlot {
    const char* filename;
    int buttonPin;
    bool isPlaying;
};
```

* Bij indrukken → sample starten.
* Bij loslaten → sample onmiddellijk stoppen.
* Polyfonie ondersteund door meerdere onafhankelijke spelers.

### OLED output

OP het scherm hebben we een oscillosope weergave avn de audio

Het scherm wordt alleen geüpdatet bij statuswijzigingen.

---

## Performance

* Geoptimaliseerde I2S‑bufferinstellingen voor minimale latency.
* Gebruik van `AudioPlayer` of `AudioGeneratorWAV` uit AudioTools.
* Foutmeldingen via Serial (`SD init fail`, `missing file`, etc.).

---
