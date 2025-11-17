#include <SPI.h>                    // SPI communicatie voor SD-kaart
#include <SD.h>                     // SD-kaart functionaliteit
#include <Adafruit_SSD1306.h>       // OLED display driver
#include <Wire.h>                   // I2C communicatie voor display
#include <Adafruit_GFX.h>           // Grafische functies voor display
#include <AudioTools.h>             // Audio verwerking bibliotheek
#include "AudioTools/Disk/AudioSourceSD.h"      // SD-kaart audio bron
#include "AudioTools/AudioCodecs/CodecMP3Helix.h" // MP3 decoder
#include <ScopeI2SStream.h>         // Custom I2S stream met scope functionaliteit
#include <ScopeDisplay.h>           // OLED scope display manager

const char *startFilePath="/";
const char* ext="mp3";
AudioSourceSD source(startFilePath, ext);
I2SStream i2s;
MP3DecoderHelix decoder;
AudioPlayer player(source, i2s, decoder);

// Chip Select pin voor de SD-kaart module
const int SD_CS = 5;

// Display en waveform setup
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int16_t waveformBuffer[WAVEFORM_SAMPLES];
int waveformIndex = 0;

// Scope display manager
ScopeDisplay scopeDisplay(&display, waveformBuffer, &waveformIndex);

// Scope I2S stream (gebruikt display mutex voor thread-safety)
ScopeI2SStream scopeI2s(waveformBuffer, &waveformIndex, scopeDisplay.getMutex());

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
  
  // Update display titel via ScopeDisplay
  if(type == Title) {
    scopeDisplay.setFilename(String(buf));
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

  // Initialiseer scope display
  if(!scopeDisplay.begin(0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

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
  
  Serial.println("Setup complete");
}

void loop() {
  // Audio playback op core 1
  player.copy();
  
  // Update playing status EN bestandsnaam
  static bool lastPlayingState = false;
  static String lastFileName = "";
  
  bool currentPlayingState = player.isActive();
  String currentFileName = source.toStr();  // Haal huidige bestandsnaam op
  
  if(currentPlayingState != lastPlayingState || currentFileName != lastFileName) {
    // Update playing status
    if(currentPlayingState != lastPlayingState) {
      scopeDisplay.setPlaying(currentPlayingState);
    }
    
    // Update bestandsnaam (verwijder pad, houd alleen filename)
    if(currentFileName != lastFileName) {
      int lastSlash = currentFileName.lastIndexOf('/');
      String displayName;
      if(lastSlash >= 0) {
        displayName = currentFileName.substring(lastSlash + 1);
      } else {
        displayName = currentFileName;
      }
      // Verwijder .mp3 extensie
      displayName.replace(".mp3", "");
      
      scopeDisplay.setFilename(displayName);
    }
    
    lastPlayingState = currentPlayingState;
    lastFileName = currentFileName;
  }
}
