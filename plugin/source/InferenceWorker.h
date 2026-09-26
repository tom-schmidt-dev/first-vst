#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <onnxruntime_cxx_api.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include "../../dsp/include/PitchTracker.hpp"
#include "../../dsp/include/LoudnessExtractor.hpp"
#include "../../dsp/include/HarmonicSynthesizer.hpp"
#include "../../dsp/include/SequencerEngine.hpp"

class InferenceWorker : public juce::Thread {
public:
    InferenceWorker(juce::AbstractFifo& inputFifo,
                    std::vector<float>& inputBuffer,
                    juce::AbstractFifo& outputFifoL,
                    std::vector<float>& outputBufferL,
                    juce::AbstractFifo& outputFifoR,
                    std::vector<float>& outputBufferR,
                    juce::AudioProcessorValueTreeState& apvts,
                    SequencerEngine& sequencer,
                    const juce::File& onnxModelFile,
                    double sampleRate,
                    std::atomic<int>& activeMidiNote,
                    std::atomic<float>& activeMidiVelocity)
        : juce::Thread("DDSPInferenceWorker"),
          mInputFifo(inputFifo),
          mInputStorage(inputBuffer),
          mOutputFifoL(outputFifoL),
          mOutputStorageL(outputBufferL),
          mOutputFifoR(outputFifoR),
          mOutputStorageR(outputBufferR),
          mApvts(apvts),
          mSequencer(sequencer),
          mActiveMidiNote(activeMidiNote),
          mActiveMidiVelocity(activeMidiVelocity),
          mSampleRate(static_cast<float>(sampleRate)),
          mPitchTracker(2048, static_cast<float>(sampleRate), 0.25f),
          mLoudnessExtractor(-80.0f),
          mSynthesizer(60, static_cast<float>(sampleRate)),
          mEnv(ORT_LOGGING_LEVEL_WARNING, "DDSPPlugin"),
          mSession(nullptr),
          mPrevAmps(60, 0.0f),
          mCurrAmps(60, 0.0f)
    {
        // Master & Timbre
        mTiltParam          = apvts.getRawParameterValue("spectral_tilt");
        mFormantParam       = apvts.getRawParameterValue("formant_blend");
        mTransientParam     = apvts.getRawParameterValue("transient_track");
        mNoiseGainParam     = apvts.getRawParameterValue("noise_gain");
        mSynthModeParam     = apvts.getRawParameterValue("synth_mode");

        // Pitch Modifiers
        mPitchQuantParam    = apvts.getRawParameterValue("pitch_quantize");
        mPitchInertiaParam  = apvts.getRawParameterValue("pitch_inertia");
        mPitchFreezeParam   = apvts.getRawParameterValue("pitch_freeze");
        mPitchInvertParam   = apvts.getRawParameterValue("pitch_inversion");
        mVoiceDriftParam    = apvts.getRawParameterValue("voice_drift");

        // 5 Voices
        for (int v = 0; v < 5; ++v) {
            const juce::String prefix = "v" + juce::String(v + 1) + "_";
            mVoiceGains[v]  = apvts.getRawParameterValue(prefix + "gain");
            mVoiceOcts[v]   = apvts.getRawParameterValue(prefix + "octave");
            mVoiceSemis[v]  = apvts.getRawParameterValue(prefix + "semitones");
            mVoiceCents[v]  = apvts.getRawParameterValue(prefix + "cents");
            mVoiceSource[v] = apvts.getRawParameterValue(prefix + "source");
            mVoicePans[v]   = apvts.getRawParameterValue(prefix + "pan");
        }

        // 5x5 Modulationsmatrix
        for (int src = 0; src < 5; ++src) {
            for (int dst = 0; dst < 5; ++dst) {
                const juce::String cellId = "m_" + juce::String(src + 1) + "_" + juce::String(dst + 1) + "_";
                mMatrixModes[src][dst] = apvts.getRawParameterValue(cellId + "mode");
                mMatrixAmts[src][dst]  = apvts.getRawParameterValue(cellId + "amt");
            }
        }

        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetInterOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#if JUCE_WINDOWS
        mSession = std::make_unique<Ort::Session>(mEnv, onnxModelFile.getFullPathName().toWideCharPointer(), sessionOptions);
#else
        mSession = std::make_unique<Ort::Session>(mEnv, onnxModelFile.getFullPathName().toRawUTF8(), sessionOptions);
#endif
    }

    ~InferenceWorker() override {
        stopThread(2000);
    }

    void run() override {
        const size_t hopSize = 128;
        std::array<float, hopSize> chunkBuffer;
        std::array<float, hopSize> synthBufferL;
        std::array<float, hopSize> synthBufferR;
        std::array<float, hopSize> noiseBuffer;

        std::array<int64_t, 3> inputShape = {1, 1, 1};
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        const char* inputNames[] = {"f0", "loudness"};
        const char* outputNames[] = {"harmonic_amps", "overall_amp", "noise_mags"};

        std::array<float, 3> pitchHistory = {220.0f, 220.0f, 220.0f};
        int unvoicedHoldCounter = 0;
        const int maxHoldFrames = 50;

        float smoothedAmp = 0.0f;
        float smoothedPitch = 220.0f;
        float frozenPitchVal = 220.0f;
        std::array<float, 5> voiceDriftPhases = {0.0f, 1.3f, 2.7f, 4.2f, 5.5f};

        const std::array<float, 8> formantFreqs = {
            300.0f, 600.0f, 1000.0f, 1600.0f, 2400.0f, 3400.0f, 4800.0f, 7000.0f
        };

        const float noiseCutoff = 2800.0f;
        const float noiseAlpha  = 1.0f - std::exp(-6.2831853f * noiseCutoff / mSampleRate);
        const float hopTimeSec  = static_cast<float>(hopSize) / mSampleRate;

        while (!threadShouldExit()) {
            bool processedChunk = false;

            while (mInputFifo.getNumReady() >= static_cast<int>(hopSize) &&
                   mOutputFifoL.getFreeSpace() >= static_cast<int>(hopSize) &&
                   mOutputFifoR.getFreeSpace() >= static_cast<int>(hopSize)) {

                processedChunk = true;

                // 1. Audio-Chunk aus FIFO lesen
                int start1, size1, start2, size2;
                mInputFifo.prepareToRead(static_cast<int>(hopSize), start1, size1, start2, size2);

                if (size1 > 0) std::copy_n(&mInputStorage[start1], size1, chunkBuffer.begin());
                if (size2 > 0) std::copy_n(&mInputStorage[start2], size2, chunkBuffer.begin() + size1);
                mInputFifo.finishedRead(static_cast<int>(hopSize));

                // 2. Feature-Extraktion (stabile, gleitend gefilterte Frequenzextraktion mit fester Hysterese)
                float detectedPitch = mPitchTracker.processBlock(chunkBuffer.data(), hopSize, 0.35f);
                float normalizedLoudness = mLoudnessExtractor.processBlock(chunkBuffer.data(), hopSize);

                float validAudioPitch = detectedPitch;
                if (validAudioPitch < 50.0f && normalizedLoudness > 0.06f && unvoicedHoldCounter < maxHoldFrames) {
                    validAudioPitch = mLastValidPitch;
                    unvoicedHoldCounter++;
                } else if (validAudioPitch >= 50.0f) {
                    mLastValidPitch = validAudioPitch;
                    mLastStableAudioPitch = validAudioPitch;
                    unvoicedHoldCounter = 0;
                } else {
                    unvoicedHoldCounter = maxHoldFrames;
                }

                // 3-Punkte Median-Filter zur Ausreißer-Eliminierung
                pitchHistory[0] = pitchHistory[1];
                pitchHistory[1] = pitchHistory[2];
                pitchHistory[2] = validAudioPitch;

                std::array<float, 3> sortedPitches = pitchHistory;
                std::sort(sortedPitches.begin(), sortedPitches.end());
                float filteredAudioPitch = sortedPitches[1];

                // 3. Basistonhöhe ermitteln: MIDI-Synthesizer vs. Live-Audio-FX
                const int synthMode = mSynthModeParam ? static_cast<int>(mSynthModeParam->load()) : 0;
                const int midiNote = mActiveMidiNote.load(std::memory_order_relaxed);

                float targetCarrierPitch = 220.0f;

                if (synthMode == 1 || (synthMode == 0 && midiNote >= 0)) {
                    // MIDI-Synthesizer-Modus: f_carrier = 440.0 * 2^((note - 69) / 12)
                    const int effectiveNote = (midiNote >= 0) ? midiNote : 60; // Fallback C4
                    const float fCarrier = 440.0f * std::pow(2.0f, static_cast<float>(effectiveNote - 69) / 12.0f);

                    // Relative Frequenzabweichung (Vibrato/Bends) übertragen
                    if (filteredAudioPitch >= 50.0f && mLastStableAudioPitch >= 50.0f) {
                        const float pitchRatio = filteredAudioPitch / mLastStableAudioPitch;
                        targetCarrierPitch = fCarrier * std::clamp(pitchRatio, 0.5f, 2.0f);
                    } else {
                        targetCarrierPitch = fCarrier;
                    }
                } else {
                    // Live-Audio-FX-Modus: Audio liefert die absolute Tonhöhe
                    targetCarrierPitch = (filteredAudioPitch >= 50.0f) ? filteredAudioPitch : mLastValidPitch;
                }

                // 4. Monophone Pitch-Modifikatoren
                const float quantStrength = getEffectiveParam(5, "pitch_quantize",  mPitchQuantParam,   0.0f);
                const float inertiaMs     = getEffectiveParam(6, "pitch_inertia",   mPitchInertiaParam, 0.0f);
                const bool  freezeActive  = mPitchFreezeParam ? (mPitchFreezeParam->load() > 0.5f) : false;
                const float invertAmount  = getEffectiveParam(7, "pitch_inversion", mPitchInvertParam,  0.0f);
                const float driftCentsMax = getEffectiveParam(8, "voice_drift",     mVoiceDriftParam,   0.0f);

                float modifiedPitch = targetCarrierPitch;

                if (freezeActive) {
                    modifiedPitch = frozenPitchVal;
                } else {
                    frozenPitchVal = modifiedPitch;
                }

                if (invertAmount > 0.001f && modifiedPitch > 40.0f) {
                    const float refFreq = 261.63f; // C4 Referenzachse
                    const float invFreq = (refFreq * refFreq) / modifiedPitch;
                    modifiedPitch = (1.0f - invertAmount) * modifiedPitch + invertAmount * invFreq;
                }

                if (quantStrength > 0.001f && modifiedPitch > 40.0f) {
                    const float midiNoteF = 69.0f + 12.0f * std::log2(modifiedPitch / 440.0f);
                    const float roundedMidi = std::round(midiNoteF);
                    const float quantPitch = 440.0f * std::pow(2.0f, (roundedMidi - 69.0f) / 12.0f);
                    modifiedPitch = (1.0f - quantStrength) * modifiedPitch + quantStrength * quantPitch;
                }

                if (inertiaMs > 1.0f) {
                    const float tau = inertiaMs * 0.001f;
                    const float alpha = 1.0f - std::exp(-hopTimeSec / tau);
                    smoothedPitch += alpha * (modifiedPitch - smoothedPitch);
                    modifiedPitch = smoothedPitch;
                } else {
                    smoothedPitch = modifiedPitch;
                }

                // Oktavierungs-Hysterese in den validen Spielbereich des Geigenmodells (G3 = 196.0 Hz)
                float playPitch = modifiedPitch;
                while (playPitch > 0.0f && playPitch < 196.0f) {
                    playPitch *= 2.0f;
                }

                // 5. DDSP ONNX-Inferenz
                const float boostedLoudness = std::clamp(std::pow(normalizedLoudness, 0.6f), 0.0f, 1.0f);
                const float normalizedF0 = std::clamp(playPitch / 3000.0f, 0.0f, 1.0f);

                std::array<float, 1> f0Data = { normalizedF0 };
                std::array<float, 1> loudnessData = { boostedLoudness };

                std::array<Ort::Value, 2> inputTensors = {
                    Ort::Value::CreateTensor<float>(memoryInfo, f0Data.data(), 1, inputShape.data(), inputShape.size()),
                    Ort::Value::CreateTensor<float>(memoryInfo, loudnessData.data(), 1, inputShape.data(), inputShape.size())
                };

                auto outputTensors = mSession->Run(
                    Ort::RunOptions{nullptr},
                    inputNames,
                    inputTensors.data(),
                    2,
                    outputNames,
                    3
                );

                const float* rawAmps = outputTensors[0].GetTensorData<float>();
                const float* overallAmpPtr = outputTensors[1].GetTensorData<float>();
                const float overallAmp = (overallAmpPtr != nullptr) ? *overallAmpPtr : 1.0f;

                const float* rawNoise = outputTensors[2].GetTensorData<float>();
                const size_t noiseBinCount = outputTensors[2].GetTensorTypeAndShapeInfo().GetElementCount();

                // 6. Dynamische Transienten-Ansprache & Noten-Gating
                const float transientTrack = getEffectiveParam(3, "transient_track", mTransientParam, 0.5f);
                const bool hasMidi = (midiNote >= 0);
                const float midiVel = mActiveMidiVelocity.load(std::memory_order_relaxed);

                const bool isSounding = (synthMode == 1) ? hasMidi
                                      : (synthMode == 2) ? (normalizedLoudness > 0.06f)
                                      : (hasMidi || normalizedLoudness > 0.06f);

                const float targetAmp = isSounding ? (hasMidi ? (midiVel > 0.0f ? midiVel : 0.8f) : overallAmp) : 0.0f;

                const float attackCoeff  = std::clamp(0.20f + 0.79f * transientTrack, 0.05f, 0.99f);
                const float releaseCoeff = std::clamp(0.08f + 0.40f * transientTrack, 0.02f, 0.50f);

                if (targetAmp > smoothedAmp) {
                    smoothedAmp += attackCoeff * (targetAmp - smoothedAmp);
                } else {
                    smoothedAmp += releaseCoeff * (targetAmp - smoothedAmp);
                    if (smoothedAmp < 0.0001f) smoothedAmp = 0.0f;
                }

                // 7. Formant-Cross-Synthese (nur wenn formantBlend > 0.01f aktiviert)
                const float formantBlend = mFormantParam ? mFormantParam->load() : 0.00f;

                if (formantBlend > 0.01f) {
                    std::array<float, 8> formantMags = {0.0f};
                    float maxFormant = 1e-5f;

                    for (size_t b = 0; b < 8; ++b) {
                        const float omega = 6.283185307f * formantFreqs[b] / mSampleRate;
                        float cosSum = 0.0f;
                        float sinSum = 0.0f;
                        for (size_t n = 0; n < hopSize; ++n) {
                            cosSum += chunkBuffer[n] * std::cos(omega * static_cast<float>(n));
                            sinSum -= chunkBuffer[n] * std::sin(omega * static_cast<float>(n));
                        }
                        formantMags[b] = std::sqrt(cosSum * cosSum + sinSum * sinSum);
                        if (formantMags[b] > maxFormant) maxFormant = formantMags[b];
                    }
                    for (size_t b = 0; b < 8; ++b) {
                        formantMags[b] /= maxFormant;
                    }
                    formantMags[6] *= 0.65f;
                    formantMags[7] *= 0.40f;

                    for (size_t h = 0; h < 60; ++h) {
                        float harmAmp = rawAmps[h] * smoothedAmp;

                        if (playPitch > 10.0f) {
                            const float f = playPitch * static_cast<float>(h + 1);
                            float formantScale = 1.0f;
                            if (f <= formantFreqs[0]) {
                                formantScale = formantMags[0];
                            } else if (f >= formantFreqs[7]) {
                                formantScale = formantMags[7];
                            } else {
                                for (size_t b = 0; b < 7; ++b) {
                                    if (f >= formantFreqs[b] && f <= formantFreqs[b + 1]) {
                                        const float frac = (f - formantFreqs[b]) / (formantFreqs[b + 1] - formantFreqs[b]);
                                        formantScale = formantMags[b] + frac * (formantMags[b + 1] - formantMags[b]);
                                        break;
                                    }
                                }
                            }
                            harmAmp *= ((1.0f - formantBlend) + formantBlend * formantScale);
                        }

                        mCurrAmps[h] = harmAmp;
                    }
                } else {
                    // 100% reine DDSP-Neuronalsynthese ohne DFT-Filterbankverluste
                    for (size_t h = 0; h < 60; ++h) {
                        mCurrAmps[h] = rawAmps[h] * smoothedAmp;
                    }
                }

                // 8. 5-Stimmen-Konfiguration & Drift
                std::array<float, 5> driftCents = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
                for (size_t d = 0; d < 5; ++d) {
                    voiceDriftPhases[d] += hopTimeSec * (0.3f + 0.15f * static_cast<float>(d));
                    if (voiceDriftPhases[d] >= 6.2831853f) voiceDriftPhases[d] -= 6.2831853f;
                    driftCents[d] = std::sin(voiceDriftPhases[d]) * driftCentsMax;
                }

                std::array<VoiceConfig, 5> voices;
                for (size_t v = 0; v < 5; ++v) {
                    const size_t baseParamIdx = 9 + v * 5;
                    const juce::String prefix = "v" + juce::String(v + 1) + "_";

                    voices[v].gain      = getEffectiveParam(baseParamIdx + 0, prefix + "gain",      mVoiceGains[v],  (v == 0 ? 1.0f : 0.0f));
                    voices[v].octave    = static_cast<int>(std::round(getEffectiveParam(baseParamIdx + 1, prefix + "octave", mVoiceOcts[v],   0.0f)));
                    voices[v].semitones = static_cast<int>(std::round(getEffectiveParam(baseParamIdx + 2, prefix + "semitones", mVoiceSemis[v], 0.0f)));
                    voices[v].cents     = getEffectiveParam(baseParamIdx + 3, prefix + "cents",     mVoiceCents[v],  0.0f) + driftCents[v];
                    voices[v].pan       = getEffectiveParam(baseParamIdx + 4, prefix + "pan",       mVoicePans[v],   0.0f);

                    const int srcIdx = mVoiceSource[v] ? static_cast<int>(mVoiceSource[v]->load()) : 0;
                    voices[v].source    = static_cast<VoiceSourceType>(std::clamp(srcIdx, 0, 4));
                }

                // 9. 5x5 All-to-All Cross-Modulationsmatrix
                std::array<std::array<MatrixCell, 5>, 5> matrix;
                for (size_t s = 0; s < 5; ++s) {
                    for (size_t d = 0; d < 5; ++d) {
                        const int modeIdx = mMatrixModes[s][d] ? static_cast<int>(mMatrixModes[s][d]->load()) : 0;
                        const float amt = mMatrixAmts[s][d] ? mMatrixAmts[s][d]->load() : 0.0f;
                        matrix[s][d].mode = static_cast<MatrixModMode>(std::clamp(modeIdx, 0, 3));
                        matrix[s][d].amount = std::clamp(amt, 0.0f, 1.0f);
                    }
                }

                // 10. Synthesizer-Block ausführen
                const float spectralTilt = getEffectiveParam(2, "spectral_tilt", mTiltParam, 0.50f);
                const float bodyCutoffHz = 1800.0f + spectralTilt * 12200.0f;
                const float tiltGainComp = 1.0f + (1.0f - spectralTilt) * 1.2f;
                const float synthTargetF0 = (playPitch >= 50.0f) ? playPitch : (mPrevF0 > 0.0f ? mPrevF0 : 220.0f);

                mSynthesizer.processBlock(
                    mPrevF0 > 0.0f ? mPrevF0 : synthTargetF0,
                    synthTargetF0,
                    mPrevAmps.data(), mCurrAmps.data(),
                    synthBufferL.data(), synthBufferR.data(), hopSize,
                    voices, matrix, bodyCutoffHz,
                    mPrevSmoothedAmp, smoothedAmp
                );

                mPrevF0 = synthTargetF0;
                mPrevAmps = mCurrAmps;
                mPrevSmoothedAmp = smoothedAmp;

                // 11. Timbre-Rauschen hinzufügen
                const float noiseGain = getEffectiveParam(4, "noise_gain", mNoiseGainParam, 0.20f);
                std::fill(noiseBuffer.begin(), noiseBuffer.end(), 0.0f);

                if (noiseGain > 0.001f && isSounding && noiseBinCount > 0) {
                    float totalNoiseMag = 0.0f;
                    const size_t binsToCheck = std::min<size_t>(noiseBinCount, 32);
                    for (size_t k = 0; k < binsToCheck; ++k) {
                        totalNoiseMag += rawNoise[k];
                    }
                    const float meanNoiseMag = totalNoiseMag / static_cast<float>(binsToCheck);

                    for (size_t i = 0; i < hopSize; ++i) {
                        const float rawRandom = (static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) * 0.5f)) - 1.0f;
                        mNoiseFilterState += noiseAlpha * (rawRandom - mNoiseFilterState);
                        noiseBuffer[i] = mNoiseFilterState * meanNoiseMag * noiseGain * smoothedAmp * 2.0f;
                    }
                }

                // 12. Dedizierte Volume-Hüllkurve aus dem Sequencer anwenden (Index 0)
                const float volumeEnv = mSequencer.getInterpolatedValue(0);
                const float makeupGain = 3.0f;

                for (size_t i = 0; i < hopSize; ++i) {
                    synthBufferL[i] = ((synthBufferL[i] * makeupGain * tiltGainComp) + noiseBuffer[i]) * volumeEnv;
                    synthBufferR[i] = ((synthBufferR[i] * makeupGain * tiltGainComp) + noiseBuffer[i]) * volumeEnv;
                }

                // 13. In Stereo-Ausgangs-FIFOs schreiben
                mOutputFifoL.prepareToWrite(static_cast<int>(hopSize), start1, size1, start2, size2);
                if (size1 > 0) std::copy_n(synthBufferL.begin(), size1, &mOutputStorageL[start1]);
                if (size2 > 0) std::copy_n(synthBufferL.begin() + size1, size2, &mOutputStorageL[start2]);
                mOutputFifoL.finishedWrite(static_cast<int>(hopSize));

                mOutputFifoR.prepareToWrite(static_cast<int>(hopSize), start1, size1, start2, size2);
                if (size1 > 0) std::copy_n(synthBufferR.begin(), size1, &mOutputStorageR[start1]);
                if (size2 > 0) std::copy_n(synthBufferR.begin() + size1, size2, &mOutputStorageR[start2]);
                mOutputFifoR.finishedWrite(static_cast<int>(hopSize));
            }

            if (!processedChunk) {
                wait(2);
            }
        }
    }

private:
    float getEffectiveParam(size_t paramIdx, const juce::String& paramId, std::atomic<float>* rawAtomic, float defaultVal) const noexcept {
        if (mSequencer.isAutomated(paramIdx)) {
            if (auto* p = mApvts.getParameter(paramId)) {
                return denormaliseParam(p->getNormalisableRange(), mSequencer.getInterpolatedValue(paramIdx));
            }
        }
        return rawAtomic ? rawAtomic->load() : defaultVal;
    }

    juce::AbstractFifo& mInputFifo;
    std::vector<float>& mInputStorage;
    juce::AbstractFifo& mOutputFifoL;
    std::vector<float>& mOutputStorageL;
    juce::AbstractFifo& mOutputFifoR;
    std::vector<float>& mOutputStorageR;

    juce::AudioProcessorValueTreeState& mApvts;
    SequencerEngine& mSequencer;
    std::atomic<int>& mActiveMidiNote;
    std::atomic<float>& mActiveMidiVelocity;

    // Parameter-Atomics
    std::atomic<float>* mTiltParam          = nullptr;
    std::atomic<float>* mFormantParam       = nullptr;
    std::atomic<float>* mTransientParam     = nullptr;
    std::atomic<float>* mNoiseGainParam     = nullptr;
    std::atomic<float>* mSynthModeParam     = nullptr;

    std::atomic<float>* mPitchQuantParam    = nullptr;
    std::atomic<float>* mPitchInertiaParam  = nullptr;
    std::atomic<float>* mPitchFreezeParam   = nullptr;
    std::atomic<float>* mPitchInvertParam   = nullptr;
    std::atomic<float>* mVoiceDriftParam    = nullptr;

    std::array<std::atomic<float>*, 5> mVoiceGains;
    std::array<std::atomic<float>*, 5> mVoiceOcts;
    std::array<std::atomic<float>*, 5> mVoiceSemis;
    std::array<std::atomic<float>*, 5> mVoiceCents;
    std::array<std::atomic<float>*, 5> mVoiceSource;
    std::array<std::atomic<float>*, 5> mVoicePans;

    std::array<std::array<std::atomic<float>*, 5>, 5> mMatrixModes;
    std::array<std::array<std::atomic<float>*, 5>, 5> mMatrixAmts;

    float mSampleRate;
    PitchTracker mPitchTracker;
    LoudnessExtractor mLoudnessExtractor;
    HarmonicSynthesizer mSynthesizer;

    Ort::Env mEnv;
    std::unique_ptr<Ort::Session> mSession;

    float mPrevF0 = 0.0f;
    float mLastValidPitch = 220.0f;
    float mLastStableAudioPitch = 220.0f;
    float mPrevSmoothedAmp = 0.0f;
    std::vector<float> mPrevAmps;
    std::vector<float> mCurrAmps;

    float mNoiseFilterState = 0.0f;
};