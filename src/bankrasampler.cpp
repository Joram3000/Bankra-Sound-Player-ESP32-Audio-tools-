#include <SPI.h>
#include <SD.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <AudioTools.h>
#include "AudioTools/Disk/AudioSourceSD.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

const int BUTTON_PIN13 = 13;
bool wasPressed13 = false;

const int BUTTON_PIN4 = 4;
bool wasPressed4 = false;


const char* ext = "mp3";
I2SStream i2s;
MP3DecoderHelix decoder;

AudioPlayer* playerPtr = nullptr; // Gebruik een pointer
AudioSourceSD* currentSourcePtr = nullptr; // hou de actieve bron bij

int currentTrack = -1; // -1 means no track selected

unsigned long lastDisplayUpdate = 0;
unsigned long playStartTime = 0;

void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");

  if (!str || len <= 0) {
    Serial.println();
    return;
  }

  // veilig kopiëren en null-termineren om crashes te voorkomen
  const int MAX_MD = 128;
  int n = len;
  if (n > MAX_MD) n = MAX_MD;
  static char buf[MAX_MD + 1];
  memcpy(buf, str, n);
  buf[n] = '\0';
  Serial.println(buf);
}

// Maak een tweede seriële poort aan voor het schermpje
Adafruit_SSD1306 display(128, 64, &Wire, -1);

const int SD_CS = 5; // Chip select pin voor de SD-kaart

void setup() {
  pinMode(BUTTON_PIN13, INPUT_PULLUP);
  pinMode(BUTTON_PIN4, INPUT_PULLUP);
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // faster SD: init SPI and use higher clock to speed up file opens/reads (ESP32)
  SPI.begin(); // default pins op ESP32; verandert niet voor andere boards
  if (!SD.begin(SD_CS, SPI, 80000000UL)) { // 80 MHz
    Serial.println("Card failed, or not present");
    while (1);
  }

  // initialiseer het scherm - DIT MOET VÓÓR display.clearDisplay()!
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  File root = SD.open("/");
  int y = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    display.setCursor(0, y * 8);
    display.println(entry.name());
    entry.close();
    y++;
    if (y > 7) break; // maximaal 8 regels
  }
  display.display();

  // setup output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = 14;   // BCLK
  cfg.pin_ws  = 15;   // WSEL/LRCK
  cfg.pin_data = 32;  // DIN
  i2s.begin(cfg);

  // setup player - maak 1 AudioSourceSD voor de hele SD en 1 AudioPlayer die die source gebruikt
  // (maak deze pas nadat SD.begin en i2s.begin zijn gedaan)
  if (!currentSourcePtr) {
    currentSourcePtr = new AudioSourceSD("/", ext);    // doorzoekt root naar .mp3 bestanden
  }
  if (!playerPtr) {
    playerPtr = new AudioPlayer(*currentSourcePtr, i2s, decoder);
    playerPtr->setMetadataCallback(nullptr); // disable metadata callback to avoid crashes
  }

  // --- PRIMING: initialise decoder/I2S en warm SD-cache zodat later starten sneller gaat ---
  // korte begin() -> end() initialiseert intern geheugen/buffers van de player/decoder
  playerPtr->begin();
  delay(200);          // korte tijd geven om buffers op te bouwen
  playerPtr->end();

  // optioneel: open mp3-bestanden kort om SD-filesystems cache warm te maken
  File f = SD.open("/chelsey.mp3", FILE_READ);
  if (f) f.close();
  f = SD.open("/VINTAGE95.mp3", FILE_READ);
  if (f) f.close();
}

void loop() {
  if (playerPtr) playerPtr->copy();

  bool pressed13 = digitalRead(BUTTON_PIN13) == LOW;
  bool pressed4 = digitalRead(BUTTON_PIN4) == LOW;

  // --- knop 13 (chelsey) ---
  if (pressed13 && !wasPressed13) {
    playerPtr->end(); // Stop direct, ook als andere sample speelt
    currentSourcePtr->setFileFilter("chelsey.mp3");
    playerPtr->begin();
    currentTrack = 0;
    playStartTime = millis();
  }
  if (!pressed13 && wasPressed13) {
    playerPtr->end();
    currentSourcePtr->setFileFilter("");
    playStartTime = 0;
    currentTrack = -1;
  }
  wasPressed13 = pressed13;

  // --- knop 4 (VINTAGE95) ---
  if (pressed4 && !wasPressed4) {
    playerPtr->end();
    currentSourcePtr->setFileFilter("VINTAGE95.mp3");
    playerPtr->begin();
    currentTrack = 1;
    playStartTime = millis();
  }
  if (!pressed4 && wasPressed4) {
    playerPtr->end();
    currentSourcePtr->setFileFilter("");
    playStartTime = 0;
    currentTrack = -1;
  }
  wasPressed4 = pressed4;

  // Display update (ongewijzigd)
  if (millis() - lastDisplayUpdate > 1000) {
    display.clearDisplay();
    display.setCursor(0, 0);

    if (playerPtr && playerPtr->isActive()) {
      display.println("Speelt af...");
      display.print("Track: ");
      display.println(currentTrack == 0 ? "chelsey.mp3" : currentTrack == 1 ? "VINTAGE95.mp3" : "-");
      unsigned long elapsed = (millis() - playStartTime) / 1000;
      display.print("Tijd: ");
      display.print(elapsed);
      display.println("s");
    } else {
      display.println("Gestopt");
      display.println("Tijd: 0s");
      display.print("Track: -");
    }

    display.display();
    lastDisplayUpdate = millis();
  }
}
