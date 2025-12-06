// Minimal header to hold DryWetMixerStream moved out of bankrasampler.cpp
#pragma once

#include <AudioTools.h>
#include <vector>
#include <algorithm>
#include "AudioTools/CoreAudio/AudioEffects/AudioEffects.h"
#include <Arduino.h> // voor Serial debug

// Enable/disable debug prints (0 = uit, 1 = aan)
#ifndef DEBUG_MIXER
#define DEBUG_MIXER 0
#endif

class DryWetMixerStream : public AudioStream {
public:
  void begin(I2SStream& outStream, Delay& effect) {
    dryOutput = &outStream;
    delay = &effect;
#if DEBUG_MIXER
    Serial.println("[DryWetMixer] begin()");
#endif
  }

  void setMix(float dry, float wet) {
    dryMix = dry;
    wetMixActive = wet;
    targetWetMix = effectEnabled ? wetMixActive : 0.0f;
    currentWetMix = targetWetMix;
    wetRampFramesRemaining = 0;
#if DEBUG_MIXER
    Serial.print("[DryWetMixer] setMix dry=");
    Serial.print(dryMix, 4);
    Serial.print(" wetActive=");
    Serial.print(wetMixActive, 4);
    Serial.print(" targetWet=");
    Serial.println(targetWetMix, 4);
#endif
  }

  void setAudioInfo(AudioInfo newInfo) override {
    AudioStream::setAudioInfo(newInfo);
    if (dryOutput) dryOutput->setAudioInfo(newInfo);
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
#if DEBUG_MIXER
    Serial.print("[DryWetMixer] setAudioInfo sr=");
    Serial.print(sampleRate);
    Serial.print(" bits=");
    Serial.print(newInfo.bits_per_sample);
    Serial.print(" ch=");
    Serial.println(channels);
#endif
  }

  void setEffectActive(bool active) {
    if (delay) delay->setActive(active);
    effectEnabled = active;
    targetWetMix = effectEnabled ? wetMixActive : 0.0f;
    scheduleWetRamp();
#if DEBUG_MIXER
    Serial.print("[DryWetMixer] setEffectActive -> ");
    Serial.print(active ? "ON" : "OFF");
    Serial.print(" targetWet=");
    Serial.println(targetWetMix, 4);
#endif
  }

  void updateEffectSampleRate(uint32_t sampleRate) {
    if (delay && sampleRate > 0) {
      delay->setSampleRate(sampleRate);
    }
#if DEBUG_MIXER
    Serial.print("[DryWetMixer] updateEffectSampleRate ");
    Serial.println(sampleRate);
#endif
  }

  void triggerAttackFade() {
    attackFramesRemaining = attackFrames;
#if DEBUG_MIXER
    Serial.print("[DryWetMixer] triggerAttackFade frames=");
    Serial.println(attackFrames);
#endif
  }

  size_t write(const uint8_t* data, size_t len) override {
    if (!dryOutput || !delay || len == 0) return 0;
    if (sampleBytes != sizeof(int16_t) && sampleBytes != sizeof(int32_t)) {
      // Unsupported format; just pass through dry
#if DEBUG_MIXER
      Serial.println("[DryWetMixer] Unsupported sample size, passing through dry");
#endif
      return dryOutput->write(data, len);
    }

    size_t processed = 0;
    while (len > 0) {
      if (pendingLen > 0 || len < frameBytes) {
        size_t needed = frameBytes - pendingLen;
        size_t copyLen = std::min(needed, len);
        if (pendingBuffer.size() < frameBytes) pendingBuffer.resize(frameBytes);
        memcpy(pendingBuffer.data() + pendingLen, data, copyLen);
        pendingLen += copyLen;
        data += copyLen;
        len -= copyLen;
        if (pendingLen < frameBytes) break;
        mixAndWrite(pendingBuffer.data(), frameBytes);
        pendingLen = 0;
        processed += frameBytes;
        continue;
      }

      size_t chunkLen = (len / frameBytes) * frameBytes;
      if (chunkLen == 0) break;
      mixAndWrite(data, chunkLen);
      data += chunkLen;
      len -= chunkLen;
      processed += chunkLen;
    }

#if DEBUG_MIXER
    // report how many bytes were processed this call
    Serial.print("[DryWetMixer] write processed_bytes=");
    Serial.println(processed);
#endif

    return processed;
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
  uint32_t attackFrames = 1;
  uint32_t attackFramesRemaining = 0;

  // debug counters
  uint32_t debugFrameCounter = 0;
  const uint32_t debugFrameInterval = 100; // print every N frames

  void mixAndWrite(const uint8_t* chunk, size_t chunkLen) {
    size_t frames = chunkLen / frameBytes;
    if (frames == 0) return;
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
      return;
    }

    int16_t* mixed = mixBuffer.data();

    for (size_t frame = 0; frame < frames; ++frame) {
      int32_t mono = 0;
      for (int ch = 0; ch < channels; ++ch) {
        mono += input[frame * channels + ch];
      }
      mono /= channels;

      effect_t wetSample = delay->process(static_cast<effect_t>(mono));
      float wetLevel = advanceWetMix();
      float attackGain = advanceAttackGain();

      // debug: print a sample occasionally to observe wet sample and levels
#if DEBUG_MIXER
      if ((debugFrameCounter++ % debugFrameInterval) == 0) {
        Serial.print("[DryWetMixer] frameSample mono=");
        Serial.print(mono);
        Serial.print(" wetSample=");
        Serial.print((int)wetSample);
        Serial.print(" wetLevel=");
        Serial.print(wetLevel, 4);
        Serial.print(" dryMix=");
        Serial.print(dryMix, 4);
        Serial.print(" effectEnabled=");
        Serial.println(effectEnabled ? "1" : "0");
      }
#endif

      for (int ch = 0; ch < channels; ++ch) {
        int32_t dryVal = input[frame * channels + ch];
        int32_t mixedVal = static_cast<int32_t>(dryMix * dryVal + wetLevel * wetSample);
        if (attackGain < 0.999f) {
          mixedVal = static_cast<int32_t>(mixedVal * attackGain);
        }
        if (mixedVal > 32767) mixedVal = 32767;
        if (mixedVal < -32768) mixedVal = -32768;
        mixed[frame * channels + ch] = static_cast<int16_t>(mixedVal);
      }
    }
    writeMixedFrames(mixed, frames);
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
};
