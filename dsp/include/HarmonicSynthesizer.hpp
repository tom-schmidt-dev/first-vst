#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <array>

class HarmonicSynthesizer {
public:
    HarmonicSynthesizer(size_t numHarmonics = 60, float sampleRate = 44100.0f)
    : mNumHarmonics(numHarmonics),
    mSampleRate(sampleRate),
    mPhases(5, std::vector<float>(numHarmonics, 0.0f)) {}

    void setSampleRate(float sampleRate) noexcept {
        mSampleRate = sampleRate;
    }

    void processBlock(float prevF0, float targetF0,
                      const float* prevAmps, const float* targetAmps,
                      float* outL, float* outR, size_t numSamples,
                      float detuneCents, float stereoSpread,
                      float subGain, float highGain) noexcept {
                          if (numSamples == 0) return;

                          std::fill_n(outL, numSamples, 0.0f);
                          std::fill_n(outR, numSamples, 0.0f);

                          if (prevF0 < 10.0f && targetF0 < 10.0f) {
                              return;
                          }

                          const float twoPi = 6.28318530717958647692f;
                          const float invSampleRate = 1.0f / mSampleRate;
                          const float nyquist = mSampleRate * 0.5f;

                          // Frequenzmultiplikatoren für die 5 Stimmen
                          const float detuneFactor = std::pow(2.0f, detuneCents / 1200.0f);
                          const std::array<float, 5> voicePitchMult = {
                              1.0f,                   // Voice 0: Center (f0)
                              1.0f / detuneFactor,    // Voice 1: Detune Down
                              detuneFactor,           // Voice 2: Detune Up
                              0.5f,                   // Voice 3: Sub-Oktave (-12 st)
                              2.0f                    // Voice 4: High-Oktave (+12 st)
                          };

                          // Lautstärkeskalierung pro Stimme
                          const std::array<float, 5> voiceGains = {
                              1.0f,
                              0.7f,
                              0.7f,
                              subGain,
                              highGain
                          };

                          // Panning-Gewichte (Left/Right)
                          const float spread = std::clamp(stereoSpread, 0.0f, 1.0f);
                          const std::array<float, 5> panL = {
                              0.5f,                                  // Center
                              0.5f * (1.0f + spread),                // Detune Down -> Links
                              0.5f * (1.0f - spread),                // Detune Up   -> Rechts
                              0.5f,                                  // Sub-Oktave
                              0.5f                                   // High-Oktave
                          };
                          const std::array<float, 5> panR = {
                              0.5f,                                  // Center
                              0.5f * (1.0f - spread),                // Detune Down
                              0.5f * (1.0f + spread),                // Detune Up
                              0.5f,                                  // Sub-Oktave
                              0.5f                                   // High-Oktave
                          };

                          // Synthese über alle 5 Stimmen
                          for (size_t v = 0; v < 5; ++v) {
                              const float vMult = voicePitchMult[v];
                              const float vGain = voiceGains[v];
                              const float vPanL = panL[v] * vGain;
                              const float vPanR = panR[v] * vGain;

                              for (size_t h = 0; h < mNumHarmonics; ++h) {
                                  const float startAmp = prevAmps ? prevAmps[h] : 0.0f;
                                  const float endAmp = targetAmps ? targetAmps[h] : 0.0f;

                                  // Inaktive Harmonische überspringen
                                  if (startAmp < 0.0001f && endAmp < 0.0001f) {
                                      continue;
                                  }

                                  const float harmonicMul = static_cast<float>(h + 1) * vMult;
                                  float phase = mPhases[v][h];

                                  for (size_t i = 0; i < numSamples; ++i) {
                                      const float frac = static_cast<float>(i) / static_cast<float>(numSamples);
                                      const float currentF0 = prevF0 + frac * (targetF0 - prevF0);
                                      const float currentAmp = startAmp + frac * (endAmp - startAmp);

                                      const float harmonicFreq = currentF0 * harmonicMul;

                                      if (harmonicFreq < nyquist && currentF0 >= 10.0f) {
                                          const float sample = currentAmp * std::sin(phase);
                                          outL[i] += sample * vPanL;
                                          outR[i] += sample * vPanR;
                                      }

                                      phase += twoPi * harmonicFreq * invSampleRate;
                                      if (phase >= twoPi) {
                                          phase -= twoPi;
                                      }
                                  }

                                  mPhases[v][h] = phase;
                              }
                          }
                      }

                      void reset() noexcept {
                          for (auto& voicePhases : mPhases) {
                              std::fill(voicePhases.begin(), voicePhases.end(), 0.0f);
                          }
                      }

private:
    size_t mNumHarmonics;
    float mSampleRate;
    std::vector<std::vector<float>> mPhases;
};
