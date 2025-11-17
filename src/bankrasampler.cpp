#include <SPI.h>                    // SPI communicatie voor SD-kaart
#include <SD.h>                     // SD-kaart functionaliteit
#include <Adafruit_SSD1306.h>       // OLED display driver
#include <Wire.h>                   // I2C communicatie voor display
#include <Adafruit_GFX.h>           // Grafische functies voor display
#include <AudioTools.h>             // Audio verwerking bibliotheek
#include "AudioTools/Disk/AudioSourceSD.h"      // SD-kaart audio bron
#include "AudioTools/AudioCodecs/CodecMP3Helix.h" // MP3 decoder

// Audio output stream via I2S (Inter-IC Sound protocol)
I2SStream i2s;
// MP3 decoder object voor het decoderen van MP3 bestanden
MP3DecoderHelix decoder;


/**
 * Callback functie voor het afdrukken van metadata van audio bestanden
 * 
 * @param type Het type metadata (bijv. titel, artiest, album)
 * @param str Pointer naar de metadata string
 * @param len Lengte van de metadata string
 */
void printMetaData(MetaDataType type, const char* str, int len){
  // Print metadata type indicator
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");

  // Controleer of de string geldig is
  if (!str || len <= 0) {
    Serial.println();
    return;
  }

  // Maximale lengte voor metadata buffer
  const int MAX_MD = 128;
  int n = len;
  if (n > MAX_MD) n = MAX_MD;
  
  // Tijdelijke buffer voor metadata string
  static char buf[MAX_MD + 1];
  memcpy(buf, str, n);
  buf[n] = '\0';  // Null-terminator toevoegen
  Serial.println(buf);
}


// Maak een OLED display object aan (128x64 pixels, I2C, geen reset pin)
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Chip Select pin voor de SD-kaart module
const int SD_CS = 5;

/**
 * Setup functie - wordt één keer uitgevoerd bij opstarten
 * Initialiseert alle hardware componenten en periferie
 */
void setup() {
  // Configureer GPIO pinnen als input met pull-up weerstanden
  pinMode(13, INPUT_PULLUP);  // Knop 1 (voor bediening)
  pinMode(4, INPUT_PULLUP);   // Knop 2 (voor bediening)
  
  // Start seriële communicatie voor debugging
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  // Initialiseer SPI bus en SD-kaart met hogere kloksnelheid (80 MHz) voor snellere toegang
  SPI.begin(); // Gebruikt standaard pinnen op ESP32
  if (!SD.begin(SD_CS, SPI, 80000000UL)) { // 80 MHz klokfrequentie
    Serial.println("Card failed, or not present");
    while (1); // Blijf hangen als SD-kaart niet gevonden wordt
  }

  // Initialiseer het OLED display op I2C adres 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Blijf hangen als display initialisatie mislukt
  }

  // Configureer display instellingen
  display.clearDisplay();              // Wis het display buffer
  display.setTextSize(1);              // Tekst grootte 1 (6x8 pixels per karakter)
  display.setTextColor(SSD1306_WHITE); // Witte tekst op zwarte achtergrond
  display.setCursor(0, 0);             // Start cursor linksboven

  // Lees en toon bestanden van de SD-kaart op het display
  File root = SD.open("/");
  int y = 0;  // Y-positie teller voor regels
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;  // Stop als er geen bestanden meer zijn
    
    display.setCursor(0, y * 8);  // Zet cursor (8 pixels per regel)
    display.println(entry.name()); // Toon bestandsnaam
    entry.close();
    y++;
    if (y > 7) break;  // Maximaal 8 regels (64 pixels / 8 pixels per regel)
  }
  display.display();  // Verstuur buffer naar het display

  // Configureer I2S audio output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = 14;   // Bit Clock (BCLK) pin
  cfg.pin_ws  = 15;   // Word Select / Left-Right Clock (LRCK) pin
  cfg.pin_data = 32;  // Data In (DIN) pin
  i2s.begin(cfg);     // Start I2S interface

}

/**
 * Loop functie - wordt continu herhaald na setup()
 * Hier komt de hoofdlogica voor het afspelen van samples
 */
void loop() {
  // TODO: Implementeer sample playback logica
  // - Lees knopinput (GPIO 13 en 4)
  // - Speel corresponderende MP3 samples af
  // - Update display met huidige status
}
