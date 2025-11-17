#include <SPI.h>                    // SPI communicatie voor SD-kaart
#include <SD.h>                     // SD-kaart functionaliteit
#include <Adafruit_SSD1306.h>       // OLED display driver
#include <Wire.h>                   // I2C communicatie voor display
#include <Adafruit_GFX.h>           // Grafische functies voor display
#include <AudioTools.h>             // Audio verwerking bibliotheek
#include "AudioTools/Disk/AudioSourceSD.h"      // SD-kaart audio bron
#include "AudioTools/AudioCodecs/CodecMP3Helix.h" // MP3 decoder
#include <ScopeI2SStream.h>         // Custom I2S stream met scope functionaliteit

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


// Waveform buffer voor scope display
int16_t waveformBuffer[WAVEFORM_SAMPLES];
int waveformIndex = 0;

// Maak ScopeI2SStream met verwijzingen naar buffer en mutex
ScopeI2SStream scopeI2s(waveformBuffer, &waveformIndex, &displayMutex);


/**
 * Display update task - draait in aparte thread
 * Toont waveform als oscilloscope
 */
void displayTask(void * parameter) {
  for(;;) {
    // Neem mutex om veilig gedeelde data te lezen
    if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
      display.clearDisplay();
      
      // Titel sectie (bovenste 12 pixels)
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print(isPlaying ? ">" : "||");
      display.print(" ");
      
      // Toon bestandsnaam (verkort als te lang)
      String shortName = currentFile;
      if(shortName.length() > 18) {
        shortName = shortName.substring(0, 15) + "...";
      }
      display.println(shortName);
      
      // Horizontale lijn
      display.drawLine(0, 11, 127, 11, SSD1306_WHITE);
      
      // Waveform scope (rest van display: 12-63 = 52 pixels hoog)
      const int scopeTop = 12;
      const int scopeHeight = 52;
      const int scopeCenter = scopeTop + scopeHeight / 2;
      
      // Teken middenlijn (0V referentie)
      for(int x = 0; x < 128; x += 4) {
        display.drawPixel(x, scopeCenter, SSD1306_WHITE);
      }
      
      // Teken waveform
      for(int x = 0; x < WAVEFORM_SAMPLES - 1; x++) {
        // Schaal 16-bit audio sample naar display pixels
        // -32768 to +32767 -> -26 to +26 pixels
        int y1 = scopeCenter - (waveformBuffer[x] * scopeHeight / 2 / 32768);
        int y2 = scopeCenter - (waveformBuffer[x+1] * scopeHeight / 2 / 32768);
        
        // Begrens binnen display
        y1 = constrain(y1, scopeTop, scopeTop + scopeHeight - 1);
        y2 = constrain(y2, scopeTop, scopeTop + scopeHeight - 1);
        
        // Teken lijn tussen samples
        display.drawLine(x, y1, x+1, y2, SSD1306_WHITE);
      }
      
      display.display();
      
      xSemaphoreGive(displayMutex);
    }
    
    // Update display elke 50ms voor vloeiende scope
    vTaskDelay(50 / portTICK_PERIOD_MS);
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

  // Creëer mutex VOOR we audio starten
  displayMutex = xSemaphoreCreateMutex();

  // Configureer I2S audio output met scope stream
  auto cfg = scopeI2s.defaultConfig(TX_MODE);
  cfg.pin_bck = 14;
  cfg.pin_ws  = 15;
  cfg.pin_data = 32;
  scopeI2s.begin(cfg);
  
  // Gebruik scopeI2s in plaats van i2s
  player.setOutput(scopeI2s);
  
  // Metadata callback registreren
  player.setMetadataCallback(printMetaData);
  player.begin();
  
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
  
  // Update playing status EN bestandsnaam (thread-safe)
  static bool lastPlayingState = false;
  static String lastFileName = "";
  
  bool currentPlayingState = player.isActive();
  String currentFileName = source.toStr();  // Haal huidige bestandsnaam op
  
  if(currentPlayingState != lastPlayingState || currentFileName != lastFileName) {
    if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
      isPlaying = currentPlayingState;
      
      // Update bestandsnaam (verwijder pad, houd alleen filename)
      if(currentFileName != lastFileName) {
        int lastSlash = currentFileName.lastIndexOf('/');
        if(lastSlash >= 0) {
          currentFile = currentFileName.substring(lastSlash + 1);
        } else {
          currentFile = currentFileName;
        }
        // Verwijder .mp3 extensie
        currentFile.replace(".mp3", "");
      }
      
      xSemaphoreGive(displayMutex);
    }
    lastPlayingState = currentPlayingState;
    lastFileName = currentFileName;
  }
}
