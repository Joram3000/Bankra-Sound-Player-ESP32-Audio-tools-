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

struct ButtonState {
  int pin;
  const char* samplePath;
  bool rawState;
  bool debouncedState;
  bool latched;
  uint32_t lastDebounceTime;
  uint32_t lastTriggerTime;
};

ButtonState buttonStates[] = {
  {PLAY_BUTTON_PIN, "/1.wav", false, false, false, 0, 0},
  {AUX_BUTTON_PIN,  "/2.wav", false, false, false, 0, 0}
};

const size_t BUTTON_COUNT = sizeof(buttonStates) / sizeof(buttonStates[0]);
int activeButtonIndex = -1;
String currentSamplePath = "";

String makeAbsolutePath(const char* path) {
  if (!path) {
    return "";
  }
  String fullPath = path;
  if (fullPath.startsWith("/")) {
    return fullPath;
  }
  String basePath = startFilePath ? String(startFilePath) : "/";
  if (basePath.length() == 0) {
    basePath = "/";
  }
  if (!basePath.startsWith("/")) {
    basePath = "/" + basePath;
  }
  if (!basePath.endsWith("/")) {
    basePath += '/';
  }
  return basePath + fullPath;
}

bool playSampleForButton(int buttonIndex) {
  if (buttonIndex < 0 || buttonIndex >= (int)BUTTON_COUNT) {
    return false;
  }

  ButtonState &btn = buttonStates[buttonIndex];
  String fullPath = makeAbsolutePath(btn.samplePath);
  if (fullPath.isEmpty()) {
    Serial.println("Geen geldig pad om af te spelen");
    return false;
  }

  if (!player.setPath(fullPath.c_str())) {
    Serial.printf("Kon bestand %s niet openen\n", fullPath.c_str());
    return false;
  }

  currentSamplePath = fullPath;
  player.play();
  activeButtonIndex = buttonIndex;
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
  uint32_t now = millis();
  for (size_t i = 0; i < BUTTON_COUNT; ++i) {
    ButtonState &btn = buttonStates[i];
    bool rawState = digitalRead(btn.pin) == LOW;
    if (rawState != btn.rawState) {
      btn.lastDebounceTime = now;
      btn.rawState = rawState;
    }

    if ((now - btn.lastDebounceTime) > BUTTON_DEBOUNCE_MS && rawState != btn.debouncedState) {
      btn.debouncedState = rawState;
      if (btn.debouncedState) {
        if (!btn.latched && (now - btn.lastTriggerTime) > BUTTON_RETRIGGER_GUARD_MS) {
          if (playSampleForButton((int)i)) {
            btn.latched = true;
            btn.lastTriggerTime = now;
          }
        }
      } else {
        btn.latched = false;
        if (activeButtonIndex == (int)i) {
          player.stop();
          activeButtonIndex = -1;
        }
      }
    }
  }

  // Audio playback op core 1
  player.copy();
  
  // Update playing status EN bestandsnaam
  static bool lastPlayingState = false;
  static String lastFileName = "";
  
  bool currentPlayingState = player.isActive();
  String currentFileName = currentSamplePath;
  if (currentFileName.isEmpty()) {
    const char* fallback = source.toStr();
    if (fallback != nullptr) {
      currentFileName = fallback;
    }
  }
  
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
