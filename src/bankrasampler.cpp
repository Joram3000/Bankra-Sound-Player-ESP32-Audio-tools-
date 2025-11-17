#include <SPI.h>                    // SPI communicatie voor SD-kaart
#include <SD.h>                     // SD-kaart functionaliteit
#include <Adafruit_SSD1306.h>       // OLED display driver
#include <Wire.h>                   // I2C communicatie voor display
#include <Adafruit_GFX.h>           // Grafische functies voor display
#include <AudioTools.h>             // Audio verwerking bibliotheek
#include "AudioTools/Disk/AudioSourceSD.h"      // SD-kaart audio bron
#include "AudioTools/AudioCodecs/CodecMP3Helix.h" // MP3 decoder

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSD source(startFilePath, ext);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);

// Maak een OLED display object aan (128x64 pixels, I2C, geen reset pin)
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Chip Select pin voor de SD-kaart module
const int SD_CS = 5;

// Task handle voor display thread
TaskHandle_t displayTaskHandle = NULL;

// Gedeelde variabelen voor display (thread-safe met mutex)
SemaphoreHandle_t displayMutex;
String currentFile = "";
bool isPlaying = false;

/**
 * Display update task - draait in aparte thread
 * Houdt het display up-to-date zonder audio te verstoren
 */
void displayTask(void * parameter) {
  for(;;) {
    // Neem mutex om veilig gedeelde data te lezen
    if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
      display.clearDisplay();
      display.setCursor(0, 0);
      
      // Toon status
      display.print("Status: ");
      display.println(isPlaying ? "Playing" : "Stopped");
      
      // Toon huidig bestand
      display.setCursor(0, 16);
      display.print("File: ");
      display.println(currentFile);
      
      display.display();
      
      xSemaphoreGive(displayMutex);
    }
    
    // Update display elke 100ms
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void printMetaData(MetaDataType type, const char* str, int len){
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");

  if (!str || len <= 0) {
    Serial.println();
    return;
  }

  const int MAX_MD = 128;
  int n = len;
  if (n > MAX_MD) n = MAX_MD;
  
  static char buf[MAX_MD + 1];
  memcpy(buf, str, n);
  buf[n] = '\0';
  Serial.println(buf);
  
  // Update display info (thread-safe)
  if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
    if(type == Title) {
      currentFile = String(buf);
    }
    xSemaphoreGive(displayMutex);
  }
}

void setup() {
  pinMode(13, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);

  SPI.begin();
  if (!SD.begin(SD_CS, SPI, 80000000UL)) {
    Serial.println("Card failed, or not present");
    while (1);
  }

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  // Configureer I2S audio output
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = 14;
  cfg.pin_ws  = 15;
  cfg.pin_data = 32;
  i2s.begin(cfg);
  
  // Metadata callback registreren
  player.setMetadataCallback(printMetaData);
  player.begin();

  // Creëer mutex voor thread-safe display updates
  displayMutex = xSemaphoreCreateMutex();
  
  // Start display task op core 0 (audio blijft op core 1)
  xTaskCreatePinnedToCore(
    displayTask,          // Task functie
    "DisplayTask",        // Task naam
    4096,                 // Stack size
    NULL,                 // Parameters
    1,                    // Priority (lager dan audio)
    &displayTaskHandle,   // Task handle
    0                     // Core 0 (audio op core 1)
  );
  
  Serial.println("Setup complete");
}

void loop() {
  // Audio playback op core 1
  player.copy();
  
  // Update playing status (thread-safe)
  static bool lastPlayingState = false;
  bool currentPlayingState = player.isActive();
  if(currentPlayingState != lastPlayingState) {
    if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
      isPlaying = currentPlayingState;
      xSemaphoreGive(displayMutex);
    }
    lastPlayingState = currentPlayingState;
  }
}
