# ESP32 Sample Player

Een compacte, polyfone **sample player** gebouwd rond een **ESP32**, met een **I2S DAC (UDA1334A)**, een **SD‑kaartmodule**, vier knoppen en een optioneel **128×64 OLED‑display**.

Projectdoel:

- samples triggeren via vier knoppen.
- **Press = play**, **release = immediate stop**.
- Focus op **lage latency**, stabiele I2S‑output.

---

## Configuratie

Pas `src/config.h` aan om het displaytype en de resolutie te kiezen.

```cpp
#define DISPLAY_DRIVER_ADAFRUIT_SSD1306 0
#define DISPLAY_DRIVER_U8G2_SSD1306     1

constexpr int DISPLAY_DRIVER = DISPLAY_DRIVER_ADAFRUIT_SSD1306; // zet op U8G2 om te wisselen
constexpr int DISPLAY_WIDTH  = 128;
constexpr int DISPLAY_HEIGHT = 64;
constexpr bool DISPLAY_INVERT_COLORS = false;
```

Er bestaan nu twee ScopeDisplay-varianten:

- `lib/ScopeDisplay/ScopeDisplay.h` – gebruikt `Adafruit_SSD1306`/`Adafruit_GFX`.
- `lib/ScopeDisplay/ScopeDisplayU8g2.h` – gebruikt `U8g2`.

Door de `DISPLAY_DRIVER`-waarde te wisselen kies je welke header wordt geïnclude.
Gebruik je een ander U8g2-board (SH1106, flips, software I2C)? Zet dan bovenaan `config.h`
een override, bijvoorbeeld:

```cpp
#define DISPLAY_U8G2_CLASS U8G2_SH1106_128X64_NONAME_F_HW_I2C
#define DISPLAY_U8G2_CTOR_ARGS U8G2_R2, U8X8_PIN_NONE
```

De UI print bij opstarten welke backend actief is zodat je snel kunt checken.

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

- Initialiseer SD, DAC (I2S), en OLED.
- Toon een korte statusmelding (“Sample Player Ready”).

### Knoppen

- Config: `INPUT_PULLUP`, LOW = ingedrukt.
- Detecteer **edge events** (pressed/released).

### Audio‐afhandeling

- Elke knop triggert een eigen sample.
- Structuur:

```cpp
struct SampleSlot {
    const char* filename;
    int buttonPin;
    bool isPlaying;
};
```

- Bij indrukken → sample starten.
- Bij loslaten → sample onmiddellijk stoppen.
- Polyfonie ondersteund door meerdere onafhankelijke spelers.

### OLED output

OP het scherm hebben we een oscillosope weergave avn de audio

Het scherm wordt alleen geüpdatet bij statuswijzigingen.

---

## Performance

- Geoptimaliseerde I2S‑bufferinstellingen voor minimale latency.
- Gebruik van `AudioPlayer` of `AudioGeneratorWAV` uit AudioTools.
- Foutmeldingen via Serial (`SD init fail`, `missing file`, etc.).

---
