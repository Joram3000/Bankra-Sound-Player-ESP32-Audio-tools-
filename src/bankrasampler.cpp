// ik heb nu audio mp3 player
// en het schermpje op een aparte thread
// met het idee dat deze file zo ligt mogelijk blijft en er liever extra files komen dan dat deze code onleesbaar wordt.

// het volgende wat zou kunnen gebeuren is dat er óf wav functie wordt toegevoegd
// of dat er buttons worden toegevoegd waar de muziek op moet reageren/


#include <SPI.h>                    // SPI communicatie voor SD-kaart
#include <SD.h>                     // SD-kaart functionaliteit
#include <Adafruit_SSD1306.h>       // OLED display driver
#include <Wire.h>                   // I2C communicatie voor display
#include <Adafruit_GFX.h>           // Grafische functies voor display
#include <AudioTools.h>             // Audio verwerking bibliotheek
#include "AudioTools/Disk/AudioSourceSD.h"      // SD-kaart audio bron
#include "AudioTools/AudioCodecs/CodecWAV.h" // WAV decoder
#include <ScopeI2SStream.h>         // Custom I2S stream met scope functionaliteit
#include <ScopeDisplay.h>           // OLED scope display manager

const char *startFilePath="/";
const char* ext="wav";
AudioSourceSD source(startFilePath, ext);
I2SStream i2s;
WAVDecoder wavDecoder;
AudioPlayer player(source, i2s, wavDecoder);

// Chip Select pin voor de SD-kaart module en bedieningsknoppen
const int SD_CS = 5;
const int PLAY_BUTTON_PIN = 13;
const int AUX_BUTTON_PIN = 4; // gereserveerd voor toekomstige functies
const uint32_t BUTTON_DEBOUNCE_MS = 20;
const uint32_t BUTTON_RETRIGGER_GUARD_MS = 80; // minimale tijd tussen herstarts

// Display en waveform setup
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int16_t waveformBuffer[WAVEFORM_SAMPLES];
int waveformIndex = 0;

// Scope display manager
ScopeDisplay scopeDisplay(&display, waveformBuffer, &waveformIndex);

// Scope I2S stream (gebruikt display mutex voor thread-safety)
ScopeI2SStream scopeI2s(waveformBuffer, &waveformIndex, scopeDisplay.getMutex());

bool restartCurrentTrack() {
  const char* currentPath = source.toStr();
  if (!currentPath || currentPath[0] == '\0') {
    Serial.println("Huidige bestandsnaam onbekend, kan niet herstarten");
    return false;
  }
  String fullPath = currentPath;
  if (!fullPath.startsWith("/")) {
    fullPath = String(startFilePath ? startFilePath : "/");
    if (fullPath.length() == 0) {
      fullPath = "/";
    }
    if (!fullPath.endsWith("/")) {
      fullPath += '/';
    }
    fullPath += currentPath;
  }

  if (!player.setPath(fullPath.c_str())) {
    Serial.printf("Kon bestand %s niet opnieuw openen\n", fullPath.c_str());
    return false;
  }
  player.play();
  return true;
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
  
  // Update display titel via ScopeDisplay
  if(type == Title) {
    scopeDisplay.setFilename(String(buf));
  }
}

void setup() {
  pinMode(PLAY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(AUX_BUTTON_PIN, INPUT_PULLUP);
  
  Serial.begin(115200);
  // Houd het logniveau laag zodat Serial I/O de audio niet onderbreekt
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

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
  player.setSilenceOnInactive(true); // houd uitgang stil wanneer niet actief
  player.setAutoNext(false);        // blijf op dezelfde sample
  player.setDelayIfOutputFull(0);   // voorkom 100ms pauzes bij volle I2S buffer
  player.begin();
  player.stop(); // start stil totdat de knop wordt ingedrukt
  
  Serial.println("Setup complete");
}

void loop() {
  // Lees play-knop met eenvoudige debounce
  static bool lastRawPlayState = false;
  static bool debouncedPlayState = false;
  static uint32_t lastDebounceTime = 0;
  static bool buttonLatched = false;
  static uint32_t lastTriggerTime = 0;
  bool rawPlayState = digitalRead(PLAY_BUTTON_PIN) == LOW; // actief laag
  uint32_t now = millis();

  if (rawPlayState != lastRawPlayState) {
    lastDebounceTime = now;
    lastRawPlayState = rawPlayState;
  }

  if ((now - lastDebounceTime) > BUTTON_DEBOUNCE_MS && rawPlayState != debouncedPlayState) {
    debouncedPlayState = rawPlayState;
    if (debouncedPlayState) {
      if (!buttonLatched && (now - lastTriggerTime) > BUTTON_RETRIGGER_GUARD_MS) {
        if (restartCurrentTrack()) {
          buttonLatched = true;
          lastTriggerTime = now;
        }
      }
    } else if (buttonLatched) {
      player.stop();
      buttonLatched = false;
    }
  }

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
      // Verwijder extensie (mp3, wav, etc.)
      int lastDot = displayName.lastIndexOf('.');
      if(lastDot > 0) {
        displayName = displayName.substring(0, lastDot);
      }
      
      scopeDisplay.setFilename(displayName);
    }
    
    lastPlayingState = currentPlayingState;
    lastFileName = currentFileName;
  }
}
