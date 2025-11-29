#ifndef SCOPEI2SSTREAM_H
#define SCOPEI2SSTREAM_H

#include <AudioTools.h>

// Waveform buffer voor scope display
#define WAVEFORM_SAMPLES 128  // Aantal samples (= display breedte)

/**
 * Custom output stream die samples captured voor waveform display
 * Intercepteert audio data op weg naar I2S voor visualisatie
 */
class ScopeI2SStream : public I2SStream {
  private:
    int16_t* waveformBuffer;
    int* waveformIndex;
    SemaphoreHandle_t* mutex;
    int downsampleRate;
     float amplitudeGamma = 0.5f; // Schaalfactor voor amplitude (wortel)
    
  public:
    /**
     * Constructor
     * @param buffer Pointer naar waveform buffer array
     * @param index Pointer naar buffer index
     * @param displayMutex Pointer naar mutex voor thread-safe access
     * @param downsample Neem 1 van elke N samples (default: 16)
     */
    ScopeI2SStream(int16_t* buffer, int* index, SemaphoreHandle_t* displayMutex, int downsample = 16) 
      : waveformBuffer(buffer), 
        waveformIndex(index), 
        mutex(displayMutex),
        downsampleRate(downsample) {
    }
    
    /**
     * Override write() om samples te capturen voor scope display
     */
    size_t write(const uint8_t *data, size_t len) override {
      // Capture samples voor waveform (downsample voor display)
      static int sampleCounter = 0;
      const int16_t* samples = (const int16_t*)data;
      int numSamples = len / sizeof(int16_t);
      
      for(int i = 0; i < numSamples; i += 2) {  // Skip rechter kanaal (stereo)
        if(sampleCounter++ % downsampleRate == 0) {  // Downsample
          if(xSemaphoreTake(*mutex, 0)) {  // Non-blocking mutex
            // Normaliseer sample naar [-1, 1]
            int16_t s = samples[i];
            float norm = (float)s / 32768.0f;
            // Pas niet-lineaire schaal toe
            float scaled = powf(fabsf(norm), amplitudeGamma);
            if(norm < 0) scaled = -scaled;
            // Schaal terug naar int16_t bereik
            int16_t out = (int16_t)(scaled * 32767.0f);
            waveformBuffer[*waveformIndex] = out;
            *waveformIndex = (*waveformIndex + 1) % WAVEFORM_SAMPLES;
            xSemaphoreGive(*mutex);
          }
        }
      }
      
      // Schrijf data door naar I2S hardware
      return I2SStream::write(data, len);
    }
};

#endif // SCOPEI2SSTREAM_H
