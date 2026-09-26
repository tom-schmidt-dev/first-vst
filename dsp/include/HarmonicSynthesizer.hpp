#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <array>

enum class VoiceSourceType {
    DDSP = 0,
    Sine,
    Saw,
    Square,
    Triangle
};

enum class MatrixModMode {
    Off = 0,
    Add,
    RingMod,
    PhaseMod
};

struct MatrixCell {
    MatrixModMode mode = MatrixModMode::Off;
    float amount = 0.0f; // 0.0f bis 1.0f
};

struct VoiceConfig {
    float gain = 1.0f;                       // 0.0f bis 1.0f
    int octave = 0;                          // -5 bis +5 Oktaven
    int semitones = 0;                       // -12 bis +12 Halbtöne
    float cents = 0.0f;                      // -50.0f bis +50.0f Cents
    VoiceSourceType source = VoiceSourceType::DDSP;
    float pan = 0.0f;                        // -1.0f (Links) bis +1.0f (Rechts)
};

class HarmonicSynthesizer {
public:
    static constexpr size_t kNumVoices = 5;

    HarmonicSynthesizer(size_t numHarmonics = 60, float sampleRate = 44100.0f)
        : mNumHarmonics(numHarmonics),
          mSampleRate(sampleRate),
          mPhases(kNumVoices, std::vector<float>(numHarmonics, 0.0f)),
          mPrevVoiceOutputs{0.0f, 0.0f, 0.0f, 0.0f, 0.0f} {}

    void setSampleRate(float sampleRate) noexcept {
        mSampleRate = std::max(100.0f, sampleRate);
    }

    float getSampleRate() const noexcept {
        return mSampleRate;
    }

    size_t getNumHarmonics() const noexcept {
        return mNumHarmonics;
    }

    /**
     * Führt die Synthese für einen Audioblock durch.
     * Unterstützt 5 Stimmen (-5 bis +5 Oktaven), 5x5 All-to-All Cross-Modulationsmatrix
     * mit 1-Sample-Delay (z^-1) und tanh-Sättigung zur Feedback-Stabilisierung.
     */
    void processBlock(float prevF0, float targetF0,
                      const float* prevAmps, const float* targetAmps,
                      float* outL, float* outR, size_t numSamples,
                      const std::array<VoiceConfig, kNumVoices>& voices,
                      const std::array<std::array<MatrixCell, kNumVoices>, kNumVoices>& matrix,
                      float bodyCutoffHz = 5000.0f,
                      float startOverallAmp = 1.0f, float endOverallAmp = 1.0f) noexcept {
        if (numSamples == 0 || outL == nullptr || outR == nullptr) return;

        std::fill_n(outL, numSamples, 0.0f);
        std::fill_n(outR, numSamples, 0.0f);

        // Falls keine Tonhöhe anliegt, abbrechen
        if (prevF0 < 5.0f && targetF0 < 5.0f) {
            mPrevVoiceOutputs.fill(0.0f);
            return;
        }

        float startF0 = prevF0 >= 5.0f ? prevF0 : targetF0;
        float endF0   = targetF0 >= 5.0f ? targetF0 : startF0;

        const float twoPi = 6.28318530717958647692f;
        const float invSampleRate = 1.0f / mSampleRate;
        const float nyquist = mSampleRate * 0.5f;
        const float safeCutoff = std::max(1000.0f, bodyCutoffHz);

        // Tonhöhen-Multiplikatoren und Aktivitäts-Flag je Stimme
        std::array<float, kNumVoices> voicePitchMult;
        std::array<bool, kNumVoices> voiceActive;

        for (size_t v = 0; v < kNumVoices; ++v) {
            const int clampedOct  = std::clamp(voices[v].octave, -5, 5);
            const int clampedSemi = std::clamp(voices[v].semitones, -12, 12);
            const float clampedCents = std::clamp(voices[v].cents, -50.0f, 50.0f);

            const float totalSemitones = static_cast<float>(clampedOct * 12 + clampedSemi) + (clampedCents / 100.0f);
            voicePitchMult[v] = std::pow(2.0f, totalSemitones / 12.0f);

            // Eine Stimme ist aktiv, wenn ihr Gain > 0 ist oder sie als Modulationsquelle für eine aktive Stimme dient
            bool isModSource = false;
            for (size_t dst = 0; dst < kNumVoices; ++dst) {
                if (matrix[v][dst].mode != MatrixModMode::Off && matrix[v][dst].amount > 0.001f) {
                    isModSource = true;
                    break;
                }
            }
            voiceActive[v] = (voices[v].gain > 0.0001f) || isModSource;
        }

        // Sample-by-Sample Syntheseschleife für exakte z^-1 Modulation & Sättigung
        for (size_t i = 0; i < numSamples; ++i) {
            const float frac = (numSamples > 1) ? (static_cast<float>(i) / static_cast<float>(numSamples - 1)) : 1.0f;
            const float currentBaseF0 = startF0 + frac * (endF0 - startF0);
            const float currentOverallAmp = std::clamp(startOverallAmp + frac * (endOverallAmp - startOverallAmp), 0.0f, 1.0f);

            // 1. Phasenmodulations-Offsets (PM / FM) für alle Stimmen ermitteln
            std::array<float, kNumVoices> pmPhaseOffsets = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            for (size_t dst = 0; dst < kNumVoices; ++dst) {
                if (!voiceActive[dst]) continue;

                float pmSum = 0.0f;
                for (size_t src = 0; src < kNumVoices; ++src) {
                    if (matrix[src][dst].mode == MatrixModMode::PhaseMod && matrix[src][dst].amount > 0.001f) {
                        // Modulationstiefe k: Bis zu 4.0 Radiant Auslenkung
                        pmSum += (4.0f * matrix[src][dst].amount) * mPrevVoiceOutputs[src];
                    }
                }
                pmPhaseOffsets[dst] = pmSum;
            }

            // 2. Roh-Oszillatorsignale für aktive Stimmen berechnen
            std::array<float, kNumVoices> rawVoiceSamples = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

            for (size_t v = 0; v < kNumVoices; ++v) {
                if (!voiceActive[v]) continue;

                const float vF0 = currentBaseF0 * voicePitchMult[v];
                if (vF0 < 2.0f || vF0 >= nyquist) continue;

                const VoiceSourceType srcType = voices[v].source;
                const float pmOffset = pmPhaseOffsets[v];
                float voiceSig = 0.0f;

                if (srcType == VoiceSourceType::DDSP) {
                    // 60 Harmonische der DDSP-Vorhersage mit Resonator-/Korpusdämpfung
                    for (size_t h = 0; h < mNumHarmonics; ++h) {
                        const float startAmp = prevAmps ? prevAmps[h] : 0.0f;
                        const float endAmp   = targetAmps ? targetAmps[h] : 0.0f;
                        const float amp = startAmp + frac * (endAmp - startAmp);

                        if (amp < 0.00001f) continue;

                        const float harmFreq = vF0 * static_cast<float>(h + 1);
                        if (harmFreq >= nyquist) break;

                        const float freqRatio = harmFreq / safeCutoff;
                        const float bodyDamping = 1.0f / (1.0f + freqRatio * freqRatio);

                        const float harmPhase = mPhases[v][h] + static_cast<float>(h + 1) * pmOffset;
                        voiceSig += amp * bodyDamping * std::sin(harmPhase);
                    }
                } else if (srcType == VoiceSourceType::Sine) {
                    // Reiner Sinus (Grundwelle)
                    const float harmFreq = vF0;
                    if (harmFreq < nyquist) {
                        const float harmPhase = mPhases[v][0] + pmOffset;
                        voiceSig = std::sin(harmPhase) * currentOverallAmp;
                    }
                } else if (srcType == VoiceSourceType::Saw) {
                    // Bandbegrenzte additive Sägezahnwelle
                    for (size_t h = 0; h < mNumHarmonics; ++h) {
                        const float harmFreq = vF0 * static_cast<float>(h + 1);
                        if (harmFreq >= nyquist) break;

                        const float harmAmp = 1.0f / static_cast<float>(h + 1);
                        const float harmPhase = mPhases[v][h] + static_cast<float>(h + 1) * pmOffset;
                        voiceSig += harmAmp * std::sin(harmPhase);
                    }
                    voiceSig *= 0.60f * currentOverallAmp; // Headroom-Skalierung
                } else if (srcType == VoiceSourceType::Square) {
                    // Bandbegrenzte additive Rechteckwelle (nur ungerade Harmonische)
                    for (size_t h = 0; h < mNumHarmonics; h += 2) {
                        const float harmFreq = vF0 * static_cast<float>(h + 1);
                        if (harmFreq >= nyquist) break;

                        const float harmAmp = 1.0f / static_cast<float>(h + 1);
                        const float harmPhase = mPhases[v][h] + static_cast<float>(h + 1) * pmOffset;
                        voiceSig += harmAmp * std::sin(harmPhase);
                    }
                    voiceSig *= 0.60f * currentOverallAmp;
                } else if (srcType == VoiceSourceType::Triangle) {
                    // Bandbegrenzte additive Dreieckswelle (nur ungerade, 1/(h+1)^2, alternierendes Vorzeichen)
                    int sign = 1;
                    for (size_t h = 0; h < mNumHarmonics; h += 2) {
                        const float harmFreq = vF0 * static_cast<float>(h + 1);
                        if (harmFreq >= nyquist) break;

                        const float hNum = static_cast<float>(h + 1);
                        const float harmAmp = static_cast<float>(sign) / (hNum * hNum);
                        const float harmPhase = mPhases[v][h] + static_cast<float>(h + 1) * pmOffset;
                        voiceSig += harmAmp * std::sin(harmPhase);
                        sign = -sign;
                    }
                    voiceSig *= 0.80f * currentOverallAmp;
                }

                rawVoiceSamples[v] = voiceSig;
            }

            // 3. RingMod und Add über die Modulationsmatrix anwenden
            std::array<float, kNumVoices> processedVoiceSamples = rawVoiceSamples;

            for (size_t dst = 0; dst < kNumVoices; ++dst) {
                if (!voiceActive[dst]) continue;

                for (size_t src = 0; src < kNumVoices; ++src) {
                    const auto mode = matrix[src][dst].mode;
                    const float amt = matrix[src][dst].amount;
                    if (amt <= 0.001f) continue;

                    const float modSignal = mPrevVoiceOutputs[src];

                    if (mode == MatrixModMode::RingMod) {
                        // Bipolare Ringmodulation: S_A * S_B * 4.0
                        const float ringModVal = processedVoiceSamples[dst] * modSignal * 4.0f;
                        processedVoiceSamples[dst] = (1.0f - amt) * processedVoiceSamples[dst] + amt * ringModVal;
                    } else if (mode == MatrixModMode::Add) {
                        // Additive Überlagerung
                        processedVoiceSamples[dst] += amt * modSignal;
                    }
                }

                // 4. Stabilisierende tanh-Sättigung (verhindert Signalexplosionen bei Feedback & DC-Offsets)
                processedVoiceSamples[dst] = std::tanh(processedVoiceSamples[dst]);
            }

            // 5. Vorherige Stimmenausgänge für das nächste Sample (z^-1 Delay) aktualisieren
            mPrevVoiceOutputs = processedVoiceSamples;

            // 6. Stereo-Mischung und Phasenfortschaltung
            for (size_t v = 0; v < kNumVoices; ++v) {
                if (!voiceActive[v]) continue;

                const float vGain = voices[v].gain;
                if (vGain > 0.0001f) {
                    const float clampedPan = std::clamp(voices[v].pan, -1.0f, 1.0f);
                    const float panL = 0.5f * (1.0f - clampedPan);
                    const float panR = 0.5f * (1.0f + clampedPan);
                    const float finalVoiceSig = processedVoiceSamples[v] * vGain;

                    outL[i] += finalVoiceSig * panL;
                    outR[i] += finalVoiceSig * panR;
                }

                // Phasenakkumulation
                const float vF0 = currentBaseF0 * voicePitchMult[v];
                if (vF0 >= 2.0f && vF0 < nyquist) {
                    const float basePhaseInc = twoPi * vF0 * invSampleRate;
                    for (size_t h = 0; h < mNumHarmonics; ++h) {
                        const float inc = basePhaseInc * static_cast<float>(h + 1);
                        mPhases[v][h] += inc;
                        if (mPhases[v][h] >= twoPi) {
                            mPhases[v][h] = std::fmod(mPhases[v][h], twoPi);
                        }
                    }
                }
            }
        }
    }

    /**
     * Vereinfachter Aufruf für Unit-Tests oder Standalone-Harmoniksynthese.
     */
    void processBlock(float prevF0, float targetF0,
                      const float* prevAmps, const float* targetAmps,
                      float* outL, float* outR, size_t numSamples) noexcept {
        std::array<VoiceConfig, kNumVoices> defaultVoices;
        defaultVoices[0].gain = 1.0f;
        defaultVoices[0].octave = 0;
        defaultVoices[0].semitones = 0;
        defaultVoices[0].cents = 0.0f;
        defaultVoices[0].source = VoiceSourceType::DDSP;
        defaultVoices[0].pan = 0.0f;

        for (size_t v = 1; v < kNumVoices; ++v) {
            defaultVoices[v].gain = 0.0f;
        }

        std::array<std::array<MatrixCell, kNumVoices>, kNumVoices> emptyMatrix{};
        processBlock(prevF0, targetF0, prevAmps, targetAmps, outL, outR, numSamples, defaultVoices, emptyMatrix);
    }

    void reset() noexcept {
        for (auto& voicePhases : mPhases) {
            std::fill(voicePhases.begin(), voicePhases.end(), 0.0f);
        }
        mPrevVoiceOutputs.fill(0.0f);
    }

private:
    size_t mNumHarmonics;
    float mSampleRate;
    std::vector<std::vector<float>> mPhases;
    std::array<float, kNumVoices> mPrevVoiceOutputs;
};