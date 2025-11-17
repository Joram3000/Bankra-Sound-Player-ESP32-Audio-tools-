#ifndef SCOPEDISPLAY_H
#define SCOPEDISPLAY_H

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Arduino.h>

// Waveform buffer configuratie
#define WAVEFORM_SAMPLES 128  // Aantal samples (= display breedte)

/**
 * ScopeDisplay - Beheert OLED display met oscilloscope visualisatie
 * 
 * Features:
 * - Real-time waveform display (oscilloscope)
 * - Status indicator (playing/paused)
 * - Bestandsnaam weergave
 * - Thread-safe updates via mutex
 * - Eigen FreeRTOS task op Core 0
 */
class ScopeDisplay {
  private:
    Adafruit_SSD1306* display;
    TaskHandle_t displayTaskHandle;
    SemaphoreHandle_t displayMutex;
    
    // Waveform data (gedeeld met ScopeI2SStream)
    int16_t* waveformBuffer;
    int* waveformIndex;
    
    // Status info
    String currentFile;
    bool isPlaying;
    
    // Display layout configuratie
    static const int SCREEN_WIDTH = 128;
    static const int SCREEN_HEIGHT = 64;
    static const int TITLE_HEIGHT = 11;
    static const int SCOPE_TOP = 12;
    static const int SCOPE_HEIGHT = 52;
    
    /**
     * Display update task - draait in aparte thread
     */
    static void displayTaskImpl(void* parameter) {
      ScopeDisplay* self = (ScopeDisplay*)parameter;
      self->displayLoop();
    }
    
    /**
     * Main display loop - rendert scope en status
     */
    void displayLoop() {
      for(;;) {
        if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
          display->clearDisplay();
          
          // Render status en titel
          renderTitle();
          
          // Render waveform scope
          renderWaveform();
          
          display->display();
          xSemaphoreGive(displayMutex);
        }
        
        // Update display elke 50ms voor vloeiende scope (20 FPS)
        vTaskDelay(50 / portTICK_PERIOD_MS);
      }
    }
    
    /**
     * Render titel sectie met status en bestandsnaam
     */
    void renderTitle() {
      display->setTextSize(1);
      display->setCursor(0, 0);
      
      // Status icoon
      display->print(isPlaying ? ">" : "||");
      display->print(" ");
      
      // Bestandsnaam (verkort als te lang)
      String shortName = currentFile;
      if(shortName.length() > 18) {
        shortName = shortName.substring(0, 15) + "...";
      }
      display->println(shortName);
      
      // Horizontale scheidingslijn
      display->drawLine(0, TITLE_HEIGHT, SCREEN_WIDTH - 1, TITLE_HEIGHT, SSD1306_WHITE);
    }
    
    /**
     * Render oscilloscope waveform
     */
    void renderWaveform() {
      const int scopeCenter = SCOPE_TOP + SCOPE_HEIGHT / 2;
      
      // Teken middenlijn (0V referentie - gestippeld)
      for(int x = 0; x < SCREEN_WIDTH; x += 4) {
        display->drawPixel(x, scopeCenter, SSD1306_WHITE);
      }
      
      // Teken waveform (roteer buffer zodat nieuwste sample rechts is)
      for(int x = 0; x < WAVEFORM_SAMPLES - 1; x++) {
        // Bereken buffer indices (oudste naar nieuwste)
        int bufIdx1 = (*waveformIndex + x) % WAVEFORM_SAMPLES;
        int bufIdx2 = (*waveformIndex + x + 1) % WAVEFORM_SAMPLES;
        
        // Schaal 16-bit audio sample naar display pixels
        // -32768 to +32767 -> scopeHeight/2 pixels
        int y1 = scopeCenter - (waveformBuffer[bufIdx1] * SCOPE_HEIGHT / 2 / 32768);
        int y2 = scopeCenter - (waveformBuffer[bufIdx2] * SCOPE_HEIGHT / 2 / 32768);
        
        // Begrens binnen display
        y1 = constrain(y1, SCOPE_TOP, SCOPE_TOP + SCOPE_HEIGHT - 1);
        y2 = constrain(y2, SCOPE_TOP, SCOPE_TOP + SCOPE_HEIGHT - 1);
        
        // Teken lijn tussen samples
        display->drawLine(x, y1, x + 1, y2, SSD1306_WHITE);
      }
    }
    
  public:
    /**
     * Constructor
     * @param disp Pointer naar Adafruit_SSD1306 display object
     * @param waveBuffer Pointer naar waveform buffer array
     * @param waveIdx Pointer naar buffer index
     */
    ScopeDisplay(Adafruit_SSD1306* disp, int16_t* waveBuffer, int* waveIdx) 
      : display(disp),
        displayTaskHandle(NULL),
        waveformBuffer(waveBuffer),
        waveformIndex(waveIdx),
        currentFile(""),
        isPlaying(false) {
      
      // Creëer mutex voor thread-safe updates
      displayMutex = xSemaphoreCreateMutex();
    }
    
    /**
     * Initialiseer display en start update task
     * @param i2cAddress I2C adres van display (default: 0x3C)
     * @return true als succesvol
     */
    bool begin(uint8_t i2cAddress = 0x3C) {
      // Initialiseer display hardware
      if(!display->begin(SSD1306_SWITCHCAPVCC, i2cAddress)) {
        return false;
      }
      
      // Toon startup bericht
      display->clearDisplay();
      display->setTextSize(1);
      display->setTextColor(SSD1306_WHITE);
      display->setCursor(0, 0);
      display->println("Initializing...");
      display->display();
      
      // Start display task op core 0 (audio blijft op core 1)
      xTaskCreatePinnedToCore(
        displayTaskImpl,      // Task functie
        "ScopeDisplay",       // Task naam
        4096,                 // Stack size
        this,                 // Parameter (this pointer)
        1,                    // Priority (lager dan audio)
        &displayTaskHandle,   // Task handle
        0                     // Core 0
      );
      
      return true;
    }
    
    /**
     * Update status en bestandsnaam (thread-safe)
     * @param playing Is audio aan het spelen?
     * @param filename Huidige bestandsnaam
     */
    void updateStatus(bool playing, const String& filename) {
      if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        isPlaying = playing;
        currentFile = filename;
        xSemaphoreGive(displayMutex);
      }
    }
    
    /**
     * Update alleen playing status (thread-safe)
     * @param playing Is audio aan het spelen?
     */
    void setPlaying(bool playing) {
      if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        isPlaying = playing;
        xSemaphoreGive(displayMutex);
      }
    }
    
    /**
     * Update alleen bestandsnaam (thread-safe)
     * @param filename Huidige bestandsnaam
     */
    void setFilename(const String& filename) {
      if(xSemaphoreTake(displayMutex, portMAX_DELAY)) {
        currentFile = filename;
        xSemaphoreGive(displayMutex);
      }
    }
    
    /**
     * Krijg mutex handle voor gebruik door ScopeI2SStream
     * @return Pointer naar display mutex
     */
    SemaphoreHandle_t* getMutex() {
      return &displayMutex;
    }
};

#endif // SCOPEDISPLAY_H
