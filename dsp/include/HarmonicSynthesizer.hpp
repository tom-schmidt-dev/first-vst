#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <array>

enum class WaveformType {
    Sine = 0,
    Saw,
    Square,
    Triangle
};

enum class SynthesisMixMode {
    Add = 0,
    RingMod,
    PhaseMod
};

class HarmonicSynthesizer {
public:
    HarmonicSynthesizer(size_t numHarmonics = 60, float sampleRate = 44100.0f)
        : mNumHarmonics(numHarmonics),
          mSampleRate(sampleRate),
          mPhases(7, std::vector<float>(numHarmonics, 0.0f)),
          mCoreBufferL(128, 0.0f),
          mCoreBufferR(128, 0.0f),
          mHarmBufferL(128, 0.0f),
          mHarmBufferR(128, 0.0f) {}

    void setSampleRate(float sampleRate) noexcept {
        mSampleRate = sampleRate;
    }

    void processBlock(float prevF0, float targetF0,
                      const float* prevAmps, const float* targetAmps,
                      float* outL, float* outR, size_t numSamples,
                      float detuneCents, float stereoSpread,
                      float subGain, float high1Gain, float high2Gain, float high3Gain,
                      float bodyCutoffHz,
                      float subPitchMult, float high1PitchMult, float high2PitchMult, float high3PitchMult,
                      const std::array<WaveformType, 4>& harmonyWaves,
                      float harmonyBalance,
                      SynthesisMixMode mixMode) noexcept {
        if (numSamples == 0) return;

        if (mCoreBufferL.size() < numSamples) {
            mCoreBufferL.resize(numSamples, 0.0f);
            mCoreBufferR.resize(numSamples, 0.0f);
            mHarmBufferL.resize(numSamples, 0.0f);
            mHarmBufferR.resize(numSamples, 0.0f);
        }

        std::fill_n(mCoreBufferL.data(), numSamples, 0.0f);
        std::fill_n(mCoreBufferR.data(), numSamples, 0.0f);
        std::fill_n(mHarmBufferL.data(), numSamples, 0.0f);
        std::fill_n(mHarmBufferR.data(), numSamples, 0.0f);
        std::fill_n(outL, numSamples, 0.0f);
        std::fill_n(outR, numSamples, 0.0f);

        if (prevF0 < 10.0f && targetF0 < 10.0f) return;

        const float twoPi = 6.28318530717958647692f;
        const float invSampleRate = 1.0f / mSampleRate;
        const float nyquist = mSampleRate * 0.5f;

        const float detuneFactor = std::pow(2.0f, detuneCents / 1200.0f);
        const std::array<float, 7> voicePitchMult = {
            1.0f, 1.0f / detuneFactor, detuneFactor,
            subPitchMult, high1PitchMult, high2PitchMult, high3PitchMult
        };

        const std::array<float, 7> voiceGains = {
            1.0f, 0.7f, 0.7f,
            subGain, high1Gain, high2Gain, high3Gain
        };

        const float spread = std::clamp(stereoSpread, 0.0f, 1.0f);
        const std::array<float, 7> panL = {
            0.5f, 0.5f * (1.0f + spread), 0.5f * (1.0f - spread),
            0.5f, 0.5f, 0.5f * (1.0f + 0.6f * spread), 0.5f * (1.0f - 0.6f * spread)
        };
        const std::array<float, 7> panR = {
            0.5f, 0.5f * (1.0f - spread), 0.5f * (1.0f + spread),
            0.5f, 0.5f, 0.5f * (1.0f - 0.6f * spread), 0.5f * (1.0f + 0.6f * spread)
        };

        const std::array<float, 7> voiceDamping = { 1.0f, 1.0f, 1.0f, 1.0f, 0.70f, 0.50f, 0.35f };
        const float safeCutoff = std::max(1000.0f, bodyCutoffHz);

        // 1. Harmoniestimmen rendern (Voices 3-6)
        for (size_t v = 3; v < 7; ++v) {
            const float vGain = voiceGains[v];
            if (vGain < 0.0001f) continue;

            const float vMult = voicePitchMult[v];
            const float vPanL = panL[v] * vGain;
            const float vPanR = panR[v] * vGain;
            const float voiceDampingScale = voiceDamping[v];
            const WaveformType wave = harmonyWaves[v - 3];

            for (size_t h = 0; h < mNumHarmonics; ++h) {
                const float startAmp = prevAmps ? prevAmps[h] : 0.0f;
                const float endAmp   = targetAmps ? targetAmps[h] : 0.0f;

                if (startAmp < 0.0001f && endAmp < 0.0001f) continue;

                float waveScale = 1.0f;
                if (wave == WaveformType::Saw) {
                    waveScale = 1.0f / static_cast<float>(h + 1);
                } else if (wave == WaveformType::Square) {
                    if ((h % 2) != 0) continue;
                    waveScale = 1.0f / static_cast<float>(h + 1);
                } else if (wave == WaveformType::Triangle) {
                    if ((h % 2) != 0) continue;
                    waveScale = 1.0f / std::pow(static_cast<float>(h + 1), 2.0f);
                }

                const float harmonicMul = static_cast<float>(h + 1) * vMult;
                float phase = mPhases[v][h];

                for (size_t i = 0; i < numSamples; ++i) {
                    const float frac = static_cast<float>(i) / static_cast<float>(numSamples);
                    const float currentF0 = prevF0 + frac * (targetF0 - prevF0);
                    const float currentAmp = (startAmp + frac * (endAmp - startAmp)) * waveScale;
                    const float harmonicFreq = currentF0 * harmonicMul;

                    if (harmonicFreq < nyquist && currentF0 >= 10.0f) {
                        const float freqRatio = harmonicFreq / safeCutoff;
                        const float bodyDamping = (1.0f / (1.0f + freqRatio * freqRatio)) * voiceDampingScale;
                        const float sample = currentAmp * bodyDamping * std::sin(phase);

                        mHarmBufferL[i] += sample * vPanL;
                        mHarmBufferR[i] += sample * vPanR;
                    }

                    phase += twoPi * harmonicFreq * invSampleRate;
                    if (phase >= twoPi) phase -= twoPi;
                }
                mPhases[v][h] = phase;
            }
        }

        // 2. Core Ensemble rendern (Voices 0-2)
        const bool isPM = (mixMode == SynthesisMixMode::PhaseMod);
        const float pmDepth = 1.5f * harmonyBalance;

        for (size_t v = 0; v < 3; ++v) {
            const float vGain = voiceGains[v];
            if (vGain < 0.0001f) continue;

            const float vMult = voicePitchMult[v];
            const float vPanL = panL[v] * vGain;
            const float vPanR = panR[v] * vGain;

            for (size_t h = 0; h < mNumHarmonics; ++h) {
                const float startAmp = prevAmps ? prevAmps[h] : 0.0f;
                const float endAmp   = targetAmps ? targetAmps[h] : 0.0f;

                if (startAmp < 0.0001f && endAmp < 0.0001f) continue;

                const float harmonicMul = static_cast<float>(h + 1) * vMult;
                float phase = mPhases[v][h];

                for (size_t i = 0; i < numSamples; ++i) {
                    const float frac = static_cast<float>(i) / static_cast<float>(numSamples);
                    const float currentF0 = prevF0 + frac * (targetF0 - prevF0);
                    const float currentAmp = startAmp + frac * (endAmp - startAmp);
                    const float harmonicFreq = currentF0 * harmonicMul;

                    if (harmonicFreq < nyquist && currentF0 >= 10.0f) {
                        const float freqRatio = harmonicFreq / safeCutoff;
                        const float bodyDamping = (1.0f / (1.0f + freqRatio * freqRatio));

                        float modulatedPhase = phase;
                        if (isPM) {
                            modulatedPhase += pmDepth * (mHarmBufferL[i] + mHarmBufferR[i]) * 0.5f;
                        }

                        const float sample = currentAmp * bodyDamping * std::sin(modulatedPhase);
                        mCoreBufferL[i] += sample * vPanL;
                        mCoreBufferR[i] += sample * vPanR;
                    }

                    phase += twoPi * harmonicFreq * invSampleRate;
                    if (phase >= twoPi) phase -= twoPi;
                }
                mPhases[v][h] = phase;
            }
        }

        // 3. Verknüpfung und Balance
        const float b = std::clamp(harmonyBalance, 0.0f, 1.0f);
        for (size_t i = 0; i < numSamples; ++i) {
            if (mixMode == SynthesisMixMode::Add || mixMode == SynthesisMixMode::PhaseMod) {
                outL[i] = (1.0f - b) * mCoreBufferL[i] + b * mHarmBufferL[i];
                outR[i] = (1.0f - b) * mCoreBufferR[i] + b * mHarmBufferR[i];
            } else if (mixMode == SynthesisMixMode::RingMod) {
                const float rmSignalL = mCoreBufferL[i] * mHarmBufferL[i] * 4.0f;
                const float rmSignalR = mCoreBufferR[i] * mHarmBufferR[i] * 4.0f;
                outL[i] = (1.0f - b) * mCoreBufferL[i] + b * rmSignalL;
                outR[i] = (1.0f - b) * mCoreBufferR[i] + b * rmSignalR;
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
    std::vector<float> mCoreBufferL;
    std::vector<float> mCoreBufferR;
    std::vector<float> mHarmBufferL;
    std::vector<float> mHarmBufferR;
};