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

class InferenceWorker : public juce::Thread {
public:
    InferenceWorker(juce::AbstractFifo& inputFifo,
                    std::vector<float>& inputBuffer,
                    juce::AbstractFifo& outputFifoL,
                    std::vector<float>& outputBufferL,
                    juce::AbstractFifo& outputFifoR,
                    std::vector<float>& outputBufferR,
                    juce::AudioProcessorValueTreeState& apvts,
                    const juce::File& onnxModelFile,
                    double sampleRate)
        : juce::Thread("DDSPInferenceWorker"),
          mInputFifo(inputFifo),
          mInputStorage(inputBuffer),
          mOutputFifoL(outputFifoL),
          mOutputStorageL(outputBufferL),
          mOutputFifoR(outputFifoR),
          mOutputStorageR(outputBufferR),
          mSampleRate(static_cast<float>(sampleRate)),
          mPitchTracker(2048, static_cast<float>(sampleRate), 0.35f),
          mLoudnessExtractor(-80.0f),
          mSynthesizer(60, static_cast<float>(sampleRate)),
          mEnv(ORT_LOGGING_LEVEL_WARNING, "DDSPPlugin"),
          mSession(nullptr),
          mPrevAmps(60, 0.0f),
          mCurrAmps(60, 0.0f)
    {
        mDetuneParam       = apvts.getRawParameterValue("detune_cents");
        mSpreadParam       = apvts.getRawParameterValue("stereo_spread");
        mSubGainParam      = apvts.getRawParameterValue("sub_gain");
        mHighGainParam     = apvts.getRawParameterValue("high_gain");
        mToleranceParam    = apvts.getRawParameterValue("tracking_tolerance");
        mFormantParam      = apvts.getRawParameterValue("formant_blend");
        mTiltParam         = apvts.getRawParameterValue("spectral_tilt");
        mTransientParam    = apvts.getRawParameterValue("transient_track");
        mNoiseGainParam    = apvts.getRawParameterValue("noise_gain");
        mSubOctParam       = apvts.getRawParameterValue("sub_octave");
        mSubSemiParam      = apvts.getRawParameterValue("sub_semitones");
        mHighOctParam      = apvts.getRawParameterValue("high_octave");
        mHighSemiParam     = apvts.getRawParameterValue("high_semitones");
        

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

        const std::array<float, 8> formantFreqs = {
            300.0f, 600.0f, 1000.0f, 1600.0f, 2400.0f, 3400.0f, 4800.0f, 7000.0f
        };

        // Filterkoeffizient für das One-Pole-Rauschfilter (~2.800 Hz Tiefpass)
        const float noiseCutoff = 2800.0f;
        const float noiseAlpha  = 1.0f - std::exp(-6.2831853f * noiseCutoff / mSampleRate);

        while (!threadShouldExit()) {
            bool processedChunk = false;

            while (mInputFifo.getNumReady() >= static_cast<int>(hopSize) &&
                   mOutputFifoL.getFreeSpace() >= static_cast<int>(hopSize) &&
                   mOutputFifoR.getFreeSpace() >= static_cast<int>(hopSize)) {

                processedChunk = true;

                // 1. Chunk lesen
                int start1, size1, start2, size2;
                mInputFifo.prepareToRead(static_cast<int>(hopSize), start1, size1, start2, size2);

                if (size1 > 0) std::copy_n(&mInputStorage[start1], size1, chunkBuffer.begin());
                if (size2 > 0) std::copy_n(&mInputStorage[start2], size2, chunkBuffer.begin() + size1);
                mInputFifo.finishedRead(static_cast<int>(hopSize));

                // 2. Features extrahieren
                const float tolerance = mToleranceParam ? mToleranceParam->load() : 0.70f;
                float detectedPitch = mPitchTracker.processBlock(chunkBuffer.data(), hopSize, tolerance);
                float normalizedLoudness = mLoudnessExtractor.processBlock(chunkBuffer.data(), hopSize);

                float validPitch = detectedPitch;
                if (validPitch < 50.0f && normalizedLoudness > 0.06f && unvoicedHoldCounter < maxHoldFrames) {
                    validPitch = mLastValidPitch;
                    unvoicedHoldCounter++;
                } else if (validPitch >= 50.0f) {
                    mLastValidPitch = validPitch;
                    unvoicedHoldCounter = 0;
                } else {
                    unvoicedHoldCounter = maxHoldFrames;
                }

                // Median-Filter
                pitchHistory[0] = pitchHistory[1];
                pitchHistory[1] = pitchHistory[2];
                pitchHistory[2] = validPitch;

                std::array<float, 3> sortedPitches = pitchHistory;
                std::sort(sortedPitches.begin(), sortedPitches.end());
                float filteredPitch = sortedPitches[1];

                // Oktavierung in den spielbaren Bereich
                float playPitch = filteredPitch;
                while (playPitch > 0.0f && playPitch < 196.0f) {
                    playPitch *= 2.0f;
                }

                float boostedLoudness = std::clamp(std::pow(normalizedLoudness, 0.6f), 0.0f, 1.0f);
                float normalizedF0 = std::clamp(playPitch / 3000.0f, 0.0f, 1.0f);

                // 3. ONNX Inferenz
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
                float overallAmp = (overallAmpPtr != nullptr) ? *overallAmpPtr : 1.0f;

                const float* rawNoise = outputTensors[2].GetTensorData<float>();
                const size_t noiseBinCount = outputTensors[2].GetTensorTypeAndShapeInfo().GetElementCount();

                // 4. Parameter atomar laden
                const float detuneCents    = mDetuneParam ? mDetuneParam->load() : 8.0f;
                const float stereoSpread   = mSpreadParam ? mSpreadParam->load() : 0.7f;
                const float subGain        = mSubGainParam ? mSubGainParam->load() : 0.35f;
                const float highGain       = mHighGainParam ? mHighGainParam->load() : 0.2f;
                const float formantBlend   = mFormantParam ? mFormantParam->load() : 0.5f;
                const float spectralTilt   = mTiltParam ? mTiltParam->load() : 0.5f;
                const float transientTrack = mTransientParam ? mTransientParam->load() : 0.5f;
                const float noiseGain      = mNoiseGainParam ? mNoiseGainParam->load() : 0.2f;

                // 5. Dynamische Transienten-Ansprache
                const bool isSounding = (normalizedLoudness > 0.06f);
                const float targetAmp = isSounding ? overallAmp : 0.0f;

                const float attackCoeff  = std::clamp(0.20f + 0.79f * transientTrack, 0.05f, 0.99f);
                const float releaseCoeff = std::clamp(0.08f + 0.40f * transientTrack, 0.02f, 0.50f);

                if (targetAmp > smoothedAmp) {
                    smoothedAmp += attackCoeff * (targetAmp - smoothedAmp);
                } else {
                    smoothedAmp += releaseCoeff * (targetAmp - smoothedAmp);
                }

                // 6. Formant-Analyse (8 Bänder)
                std::array<float, 8> formantMags = {0.0f};
                float maxFormant = 1e-5f;

                if (formantBlend > 0.01f) {
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
                    // Sanfte Dämpfung der beiden obersten Formantbänder gegen Pfeifen
                    formantMags[6] *= 0.65f;
                    formantMags[7] *= 0.40f;
                }

                // 7. Harmonische Amplituden vorbereiten
                for (size_t h = 0; h < 60; ++h) {
                    float harmAmp = rawAmps[h] * smoothedAmp;

                    if (formantBlend > 0.01f && playPitch > 10.0f) {
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

                float synthTargetF0 = (playPitch >= 50.0f) ? playPitch : (mPrevF0 > 0.0f ? mPrevF0 : 220.0f);

                // Pitch-Multiplikatoren aus Oktaven und Halbtönen berechnen
                const float subOct   = mSubOctParam ? mSubOctParam->load() : -1.0f;
                const float subSemi  = mSubSemiParam ? mSubSemiParam->load() : 0.0f;
                const float highOct  = mHighOctParam ? mHighOctParam->load() : 1.0f;
                const float highSemi = mHighSemiParam ? mHighSemiParam->load() : 0.0f;

                const float subPitchMult  = std::pow(2.0f, (subOct * 12.0f + subSemi) / 12.0f);
                const float highPitchMult = std::pow(2.0f, (highOct * 12.0f + highSemi) / 12.0f);

                // Dynamischer Tilt: 0.0 = dunkel (1.800 Hz), 1.0 = voll geöffnet (14.000 Hz)
                const float bodyCutoffHz = 2200.0f + (1.0f - spectralTilt) * 4300.0f;
                const float tiltGainComp = 1.0f + (1.0f - spectralTilt) * 1.2f;

                // 8. Oszillatoren mit variabler Tonhöhe und Korpusfilter berechnen
                mSynthesizer.processBlock(
                    mPrevF0 > 0.0f ? mPrevF0 : synthTargetF0,
                    synthTargetF0,
                    mPrevAmps.data(), mCurrAmps.data(),
                    synthBufferL.data(), synthBufferR.data(), hopSize,
                    detuneCents, stereoSpread, subGain, highGain,
                    bodyCutoffHz, subPitchMult, highPitchMult
                );

                mPrevF0 = synthTargetF0;
                mPrevAmps = mCurrAmps;

                // Pegelausgleich für die Dämpfung bei geschlossenem Tilt
                for (size_t i = 0; i < hopSize; ++i) {
                    synthBufferL[i] *= tiltGainComp;
                    synthBufferR[i] *= tiltGainComp;
                }

                // 9. Kontinuierlicher Rauschsynthesizer (ohne Sinusschwingungen)
                std::fill(noiseBuffer.begin(), noiseBuffer.end(), 0.0f);
                if (noiseGain > 0.001f && isSounding && noiseBinCount > 0) {
                    float totalNoiseMag = 0.0f;
                    const size_t binsToCheck = std::min<size_t>(noiseBinCount, 32);
                    for (size_t k = 0; k < binsToCheck; ++k) {
                        totalNoiseMag += rawNoise[k];
                    }
                    const float meanNoiseMag = totalNoiseMag / static_cast<float>(binsToCheck);

                    for (size_t i = 0; i < hopSize; ++i) {
                        // Kontinuierliches weißes Rauschen [-1.0, 1.0]
                        const float rawRandom = (static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) * 0.5f)) - 1.0f;
                        // Zustand des Tiefpassfilters aktualisieren
                        mNoiseFilterState += noiseAlpha * (rawRandom - mNoiseFilterState);

                        noiseBuffer[i] = mNoiseFilterState * meanNoiseMag * noiseGain * smoothedAmp * 2.0f;
                    }
                }

                // 10. Signalzusammenführung
                const float makeupGain = 3.0f;
                for (size_t i = 0; i < hopSize; ++i) {
                    synthBufferL[i] = (synthBufferL[i] * makeupGain) + noiseBuffer[i];
                    synthBufferR[i] = (synthBufferR[i] * makeupGain) + noiseBuffer[i];
                }

                // 11. In Stereo-Ausgangs-FIFOs schreiben
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
    juce::AbstractFifo& mInputFifo;
    std::vector<float>& mInputStorage;
    juce::AbstractFifo& mOutputFifoL;
    std::vector<float>& mOutputStorageL;
    juce::AbstractFifo& mOutputFifoR;
    std::vector<float>& mOutputStorageR;

    std::atomic<float>* mDetuneParam    = nullptr;
    std::atomic<float>* mSpreadParam    = nullptr;
    std::atomic<float>* mSubGainParam   = nullptr;
    std::atomic<float>* mHighGainParam  = nullptr;
    std::atomic<float>* mToleranceParam = nullptr;
    std::atomic<float>* mFormantParam   = nullptr;
    std::atomic<float>* mTiltParam      = nullptr;
    std::atomic<float>* mTransientParam = nullptr;
    std::atomic<float>* mNoiseGainParam = nullptr;

    std::atomic<float>* mSubOctParam    = nullptr;
    std::atomic<float>* mSubSemiParam   = nullptr;
    std::atomic<float>* mHighOctParam   = nullptr;
    std::atomic<float>* mHighSemiParam  = nullptr;

    float mSampleRate;
    PitchTracker mPitchTracker;
    LoudnessExtractor mLoudnessExtractor;
    HarmonicSynthesizer mSynthesizer;

    Ort::Env mEnv;
    std::unique_ptr<Ort::Session> mSession;

    float mPrevF0 = 0.0f;
    float mLastValidPitch = 220.0f;
    std::vector<float> mPrevAmps;
    std::vector<float> mCurrAmps;

    // Filterzustand für stochastisches Rauschen
    float mNoiseFilterState = 0.0f;
};