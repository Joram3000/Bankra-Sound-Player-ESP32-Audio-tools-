// Minimal header to hold DryWetMixerStream moved out of bankrasampler.cpp
#pragma once

#include <AudioTools.h>
#include <vector>
#include <memory>
#include <algorithm>
#include "AudioTools/CoreAudio/AudioEffects/AudioEffects.h"
#include "AudioTools/CoreAudio/AudioFilter/Filter.h"
#include <Arduino.h> // voor Serial debug
#include "config.h"

class DryWetMixerStream : public ModifyingStream {
public:
  // Backwards-compatible begin() delegates to ModifyingStream-style setters
  void begin(I2SStream& outStream, Delay& effect) {
    setOutput(outStream);
    setEffect(&effect);
  }

  void setMix(float dry, float wet) {
    dryMix = dry;
    wetMixActive = wet;
    targetWetMix = effectEnabled ? wetMixActive : 0.0f;
    currentWetMix = targetWetMix;
    wetRampFramesRemaining = 0;

  }

  void configureMasterLowPass(float cutoffHz, float q = 0.7071f,
                              bool enabled = true) {
    inputFilterCutoff = cutoffHz;
    inputFilterQ = q;
    inputFilterEnabled = enabled;
    refreshInputFilterState();

  }

  void configureMasterCompressor(uint16_t attackMs, uint16_t releaseMs,
                                 uint16_t holdMs, uint8_t thresholdPercent,
                                 float compressionRatio,
                                 bool enabled = true) {
    compAttackMs = attackMs;
    compReleaseMs = releaseMs;
    compHoldMs = holdMs;
    compThresholdPercent = thresholdPercent;
    compRatio = compressionRatio;
    masterCompressorEnabled = enabled;
    refreshMasterCompressor();

  }

  void setMasterCompressorEnabled(bool enabled) {
    masterCompressorEnabled = enabled;
    if (masterCompressor) {
      masterCompressor->setActive(enabled);
    }
  }

  void setInputLowPassCutoff(float cutoffHz) {
    inputFilterCutoff = cutoffHz;
    if (!inputFilterEnabled || !inputFilterInitialized) {
      return;
    }
    if (inputLowPassFilters.empty() || sampleRate == 0) {
      return;
    }
    for (auto &filter : inputLowPassFilters) {
      if (filter) {
        filter->begin(inputFilterCutoff, static_cast<float>(sampleRate),
                      inputFilterQ);
      }
    }
  }

  void setAudioInfo(AudioInfo newInfo) override {
    AudioStream::setAudioInfo(newInfo);
    if (dryOutput) dryOutput->setAudioInfo(newInfo);
    // propagate to internal callback stream as well
    cbStream.setAudioInfo(newInfo);
    sampleBytes = std::max<int>(1, newInfo.bits_per_sample / 8);
    channels = std::max<int>(1, newInfo.channels);
    frameBytes = sampleBytes * channels;
    pendingLen = 0;
    pendingBuffer.clear();
    sampleRate = newInfo.sample_rate > 0 ? newInfo.sample_rate : 44100;
    fadeFrames = std::max<uint32_t>(1, (sampleRate * EFFECT_TOGGLE_FADE_MS) / 1000);
    attackFrames = std::max<uint32_t>(1, (sampleRate * SAMPLE_ATTACK_FADE_MS) / 1000);
    targetWetMix = effectEnabled ? wetMixActive : 0.0f;
    currentWetMix = targetWetMix;
    wetRampFramesRemaining = 0;
    attackFramesRemaining = 0;
    const size_t reserveFrames = 256; // tweak if needed
    mixBuffer.clear();
    mixBuffer.reserve(reserveFrames * channels);
    refreshInputFilterState();
  refreshMasterCompressor();

  }

  // ModifyingStream API: allow this mixer to be used like other AudioTools
  // components. We keep internal pointers to the provided stream/print
  // objects but still prefer typed I2SStream for the dry output.
  void setStream(Stream &in) override {
  p_in = &in;

  cbStream.setStream(in);
  s_instance = this;
  cbStream.setUpdateCallback(staticUpdate);
  }

  void setOutput(Print &out) override {
    // Try to treat out as an I2SStream if possible. This cast is safe when
    // callers pass the same I2SStream instance used previously.
    dryOutput = reinterpret_cast<I2SStream*>(&out);

  p_out = &out;
  cbStream.setOutput(out);
  s_instance = this;
  cbStream.setUpdateCallback(staticUpdate);
  }

  // Configure the Delay target to use. The Delay object is not disabled by
  // this mixer – we ensure it stays active so internal buffers keep running.
  void setEffect(Delay* d) {
    delay = d;
    if (delay) delay->setActive(true);
    s_instance = this;
    cbStream.setUpdateCallback(staticUpdate);
  }

  void setEffectActive(bool active) {
  // Do not disable the Delay object itself here. We want the delay line to
  // keep running so echoes / feedback continue even when the wet mix is
  // turned off. The effectEnabled flag only controls audibility (wet mix).
  effectEnabled = active;
  targetWetMix = effectEnabled ? wetMixActive : 0.0f;
  scheduleWetRamp();
  }

  void updateEffectSampleRate(uint32_t sampleRate) {
    if (delay && sampleRate > 0) {
      delay->setSampleRate(sampleRate);
    }

  }

  // When true we actually feed the incoming audio into the delay. When false
  // we still call delay->process(0) so the delay's internal buffer advances
  // and the effect tail keeps playing without new input.
  void setSendActive(bool send) {
    sendActive = send;

  }
  // Delegate write to internal CallbackStream which will call our
  // update callback to mix the buffer before sending it to the real output.
  size_t write(const uint8_t* data, size_t len) override {
    return cbStream.write(data, len);
  }

  // When there is no active source we can pump silence through the mixer to
  // advance internal effects (e.g., delay buffer) so tails continue to decay.
  // frames: number of audio frames (samples per channel) to push.
  void pumpSilenceFrames(size_t frames) {
    if (frames == 0) return;
    size_t sampleCount = frames * std::max<int>(1, channels);
    size_t byteCount = sampleCount * static_cast<size_t>(sampleBytes);
    // allocate a temporary zero buffer on the heap to avoid stack pressure
    std::vector<uint8_t> zeros(byteCount);
    // write will call the CallbackStream which will call our updateCallback
    // and thus call delay->process(0) for each frame.
    write(zeros.data(), byteCount);
  }

private:
  I2SStream* dryOutput = nullptr;
  Delay* delay = nullptr;
  float dryMix = 1.0f;
  float wetMixActive = 0.35f;
  float currentWetMix = 0.0f;
  float targetWetMix = 0.0f;
  float wetRampDelta = 0.0f;
  int sampleBytes = sizeof(int16_t);
  int channels = 2;
  std::vector<int16_t> mixBuffer;
  std::vector<int16_t> convertedInput;
  std::vector<int32_t> expandedOutput;
  std::vector<uint8_t> pendingBuffer;
  size_t pendingLen = 0;
  size_t frameBytes = sizeof(int16_t) * 2;
  uint32_t sampleRate = 44100;
  uint32_t fadeFrames = 1;
  uint32_t wetRampFramesRemaining = 0;
  bool effectEnabled = false;
  // When false we do not feed the incoming audio into the delay; the delay
  // still runs and is called with silence (0) so its buffer advances.
  bool sendActive = false;
  uint32_t attackFrames = 1;
  uint32_t attackFramesRemaining = 0;

  // Input filter state (applied before wet send and dry output)
  std::vector<std::unique_ptr<LowPassFilter<float>>> inputLowPassFilters;
  bool inputFilterEnabled = false;
  bool inputFilterInitialized = false;
  float inputFilterCutoff = 0.0f;
  float inputFilterQ = 0.7071f;
  std::vector<float> filteredDryScratch;
  std::unique_ptr<Compressor> masterCompressor;
  bool masterCompressorEnabled = false;
  uint16_t compAttackMs = 10;
  uint16_t compReleaseMs = 120;
  uint16_t compHoldMs = 10;
  uint8_t compThresholdPercent = 15;
  float compRatio = 0.5f;

  // debug counters
  uint32_t debugFrameCounter = 0;
  const uint32_t debugFrameInterval = 100; // print every N frames
  // ModifyingStream targets
  Stream* p_in = nullptr;
  Print* p_out = nullptr;
  // internal CallbackStream used to hook into AudioTools processing
  CallbackStream cbStream;

  // static instance pointer for callback forwarding (assumes single mixer)
  static DryWetMixerStream* s_instance;

  // static callback invoked by CallbackStream; forwards to instance method
  static size_t staticUpdate(uint8_t* data, size_t len) {
    if (s_instance) return s_instance->updateCallback(data, len);
    return len;
  }

  void mixAndWrite(const uint8_t* chunk, size_t chunkLen) {
    // Provide an update-style API which rewrites the incoming buffer in-place
    // with mixed output. This method is also used as the core implementation
    // for the CallbackStream update callback.
  }

  // Called by staticUpdate to perform the mixing and write results back into
  // the provided byte buffer. Returns number of bytes written (len) or 0 on
  // error.
  size_t updateCallback(uint8_t* chunk, size_t chunkLen) {
    size_t frames = chunkLen / frameBytes;
    if (!dryOutput || !delay || frames == 0) return 0;
    size_t sampleCount = frames * channels;
    mixBuffer.resize(sampleCount);

    const int16_t* input = nullptr;
    if (sampleBytes == sizeof(int16_t)) {
      input = reinterpret_cast<const int16_t*>(chunk);
    } else if (sampleBytes == sizeof(int32_t)) {
      convertedInput.resize(sampleCount);
      const int32_t* input32 = reinterpret_cast<const int32_t*>(chunk);
      for (size_t i = 0; i < sampleCount; ++i) {
        int32_t value = input32[i] >> 16;
        if (value > 32767) value = 32767;
        if (value < -32768) value = -32768;
        convertedInput[i] = static_cast<int16_t>(value);
      }
      input = convertedInput.data();
    } else {
      return 0;
    }

    int16_t* mixed = mixBuffer.data();
    if (filteredDryScratch.size() < static_cast<size_t>(channels)) {
      filteredDryScratch.resize(static_cast<size_t>(channels), 0.0f);
    }

    for (size_t frame = 0; frame < frames; ++frame) {
      float monoSum = 0.0f;
      for (int ch = 0; ch < channels; ++ch) {
        float sampleValue = static_cast<float>(input[frame * channels + ch]);
        float filtered = processInputLowPass(sampleValue, ch);
        if (filtered > 32767.0f) filtered = 32767.0f;
        if (filtered < -32768.0f) filtered = -32768.0f;
        filteredDryScratch[ch] = filtered;
        monoSum += filtered;
      }
      float filteredMono = (channels > 0) ? (monoSum / static_cast<float>(channels)) : monoSum;
      effect_t wetInput = sendActive ? static_cast<effect_t>(filteredMono) : 0;
      // Always run the delay process so its internal buffer advances.
      // When send is muted we still feed silence so the delay tail keeps moving.
      effect_t wetSample = delay->process(wetInput);
      float wetLevel = advanceWetMix();
      float attackGain = advanceAttackGain();

      // debug: print a sample occasionally to observe wet sample and levels


      for (int ch = 0; ch < channels; ++ch) {
        int32_t dryVal = static_cast<int32_t>(filteredDryScratch[ch]);
        int32_t mixedVal = static_cast<int32_t>(dryMix * dryVal + wetLevel * wetSample);
        if (attackGain < 0.999f) {
          mixedVal = static_cast<int32_t>(mixedVal * attackGain);
        }
        if (mixedVal > 32767) mixedVal = 32767;
        if (mixedVal < -32768) mixedVal = -32768;
        int16_t outputSample = static_cast<int16_t>(mixedVal);
        if (masterCompressor) {
          outputSample = masterCompressor->process(static_cast<effect_t>(outputSample));
        }
        mixed[frame * channels + ch] = outputSample;
      }
    }

    // Write mixed samples back into chunk
    if (sampleBytes == sizeof(int16_t)) {
      memcpy(chunk, mixed, sampleCount * sizeof(int16_t));
      return sampleCount * sizeof(int16_t);
    }

    if (sampleBytes == sizeof(int32_t)) {
      expandedOutput.resize(sampleCount);
      for (size_t i = 0; i < sampleCount; ++i) {
        expandedOutput[i] = static_cast<int32_t>(mixed[i]) << 16;
      }
      memcpy(chunk, expandedOutput.data(), sampleCount * sizeof(int32_t));
      return sampleCount * sizeof(int32_t);
    }

    return 0;
  }

  void writeMixedFrames(const int16_t* mixed, size_t frames) {
    size_t sampleCount = frames * channels;
    if (sampleBytes == sizeof(int16_t)) {
      dryOutput->write(reinterpret_cast<const uint8_t*>(mixed), sampleCount * sizeof(int16_t));
    } else if (sampleBytes == sizeof(int32_t)) {
      expandedOutput.resize(sampleCount);
      for (size_t i = 0; i < sampleCount; ++i) {
        expandedOutput[i] = static_cast<int32_t>(mixed[i]) << 16;
      }
      dryOutput->write(reinterpret_cast<const uint8_t*>(expandedOutput.data()), sampleCount * sizeof(int32_t));
    }
  }

  void scheduleWetRamp() {
    if (fadeFrames <= 1) {
      currentWetMix = targetWetMix;
      wetRampFramesRemaining = 0;
      wetRampDelta = 0.0f;
      return;
    }
    wetRampFramesRemaining = fadeFrames;
    wetRampDelta = (targetWetMix - currentWetMix) / static_cast<float>(fadeFrames);
  }

  float advanceWetMix() {
    if (wetRampFramesRemaining > 0) {
      currentWetMix += wetRampDelta;
      --wetRampFramesRemaining;
      if ((wetRampDelta > 0.0f && currentWetMix > targetWetMix) ||
          (wetRampDelta < 0.0f && currentWetMix < targetWetMix)) {
        currentWetMix = targetWetMix;
        wetRampFramesRemaining = 0;
        wetRampDelta = 0.0f;
      }
    } else {
      currentWetMix = targetWetMix;
    }
    return currentWetMix;
  }

  float advanceAttackGain() {
    if (attackFramesRemaining == 0) return 1.0f;
    float gain = 1.0f - (static_cast<float>(attackFramesRemaining) / static_cast<float>(attackFrames));
    --attackFramesRemaining;
    if (attackFramesRemaining == 0) return 1.0f;
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    return gain;
  }

  void refreshInputFilterState() {
    inputFilterInitialized = false;
    if (channels <= 0) {
      inputLowPassFilters.clear();
      filteredDryScratch.clear();
      return;
    }

    if (!inputFilterEnabled || sampleRate == 0) {
      filteredDryScratch.assign(static_cast<size_t>(channels), 0.0f);
      return;
    }

    if (inputLowPassFilters.size() < static_cast<size_t>(channels)) {
      inputLowPassFilters.resize(static_cast<size_t>(channels));
    }

    for (int ch = 0; ch < channels; ++ch) {
      if (!inputLowPassFilters[ch]) {
        inputLowPassFilters[ch].reset(new LowPassFilter<float>());
      }
      inputLowPassFilters[ch]->begin(inputFilterCutoff,
                                     static_cast<float>(sampleRate),
                                     inputFilterQ);
    }
    filteredDryScratch.assign(static_cast<size_t>(channels), 0.0f);
    inputFilterInitialized = true;
  }

  float processInputLowPass(float sample, int channelIndex) {
    if (!inputFilterEnabled || !inputFilterInitialized) {
      return sample;
    }
    if (channelIndex < 0 ||
        channelIndex >= static_cast<int>(inputLowPassFilters.size())) {
      return sample;
    }
    LowPassFilter<float>* filter = inputLowPassFilters[channelIndex].get();
    if (filter == nullptr) return sample;
    return filter->process(sample);
  }

  void refreshMasterCompressor() {
    if (sampleRate == 0) {
      masterCompressor.reset();
      return;
    }
  masterCompressor.reset(new Compressor(sampleRate, compAttackMs,
                      compReleaseMs, compHoldMs,
                      compThresholdPercent, compRatio));
    masterCompressor->setActive(masterCompressorEnabled);
  }
};

