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
                    double sampleRate)
        : juce::Thread("DDSPInferenceWorker"),
          mInputFifo(inputFifo),
          mInputStorage(inputBuffer),
          mOutputFifoL(outputFifoL),
          mOutputStorageL(outputBufferL),
          mOutputFifoR(outputFifoR),
          mOutputStorageR(outputBufferR),
          mApvts(apvts),
          mSequencer(sequencer),
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
        mToleranceParam    = apvts.getRawParameterValue("tracking_tolerance");
        mFormantParam      = apvts.getRawParameterValue("formant_blend");
        mTiltParam         = apvts.getRawParameterValue("spectral_tilt");
        mTransientParam    = apvts.getRawParameterValue("transient_track");
        mNoiseGainParam    = apvts.getRawParameterValue("noise_gain");

        mPitchQuantParam   = apvts.getRawParameterValue("pitch_quantize");
        mPitchInertiaParam = apvts.getRawParameterValue("pitch_inertia");
        mPitchFreezeParam  = apvts.getRawParameterValue("pitch_freeze");
        mPitchInvertParam  = apvts.getRawParameterValue("pitch_inversion");
        mVoiceDriftParam   = apvts.getRawParameterValue("voice_drift");

        mHarmBalanceParam  = apvts.getRawParameterValue("harmony_balance");
        mMixModeParam      = apvts.getRawParameterValue("mix_mode");

        mSubGainParam      = apvts.getRawParameterValue("sub_gain");
        mSubOctParam       = apvts.getRawParameterValue("sub_octave");
        mSubSemiParam      = apvts.getRawParameterValue("sub_semitones");
        mSubWaveParam      = apvts.getRawParameterValue("sub_wave");

        mHigh1GainParam    = apvts.getRawParameterValue("high_gain");
        mHigh1OctParam     = apvts.getRawParameterValue("high_octave");
        mHigh1SemiParam    = apvts.getRawParameterValue("high_semitones");
        mHigh1WaveParam    = apvts.getRawParameterValue("high1_wave");

        mHigh2GainParam    = apvts.getRawParameterValue("high2_gain");
        mHigh2OctParam     = apvts.getRawParameterValue("high2_octave");
        mHigh2SemiParam    = apvts.getRawParameterValue("high2_semitones");
        mHigh2WaveParam    = apvts.getRawParameterValue("high2_wave");

        mHigh3GainParam    = apvts.getRawParameterValue("high3_gain");
        mHigh3OctParam     = apvts.getRawParameterValue("high3_octave");
        mHigh3SemiParam    = apvts.getRawParameterValue("high3_semitones");
        mHigh3WaveParam    = apvts.getRawParameterValue("high3_wave");

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
        std::array<float, 4> voiceDriftPhases = {0.0f, 1.3f, 2.7f, 4.2f};

        const std::array<float, 8> formantFreqs = {
            300.0f, 600.0f, 1000.0f, 1600.0f, 2400.0f, 3400.0f, 4800.0f, 7000.0f
        };

        const float noiseCutoff = 2800.0f;
        const float noiseAlpha  = 1.0f - std::exp(-6.2831853f * noiseCutoff / mSampleRate);
        const float hopTimeSec = static_cast<float>(hopSize) / mSampleRate;

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

                // 2. Feature-Extraktion
                const float tolerance = getEffectiveParam(5, "tracking_tolerance", mToleranceParam, 0.70f);
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

                // Grundton-Manipulation
                const float quantStrength = getEffectiveParam(10, "pitch_quantize",  mPitchQuantParam,   0.0f);
                const float inertiaMs     = getEffectiveParam(11, "pitch_inertia",   mPitchInertiaParam, 0.0f);
                const bool  freezeActive  = mPitchFreezeParam ? (mPitchFreezeParam->load() > 0.5f) : false;
                const float invertAmount  = getEffectiveParam(12, "pitch_inversion", mPitchInvertParam,  0.0f);
                const float driftCentsMax = getEffectiveParam(13, "voice_drift",     mVoiceDriftParam,   0.0f);

                float modifiedPitch = filteredPitch;

                if (freezeActive) {
                    modifiedPitch = frozenPitchVal;
                } else {
                    frozenPitchVal = modifiedPitch;
                }

                if (invertAmount > 0.001f && modifiedPitch > 40.0f) {
                    const float refFreq = 261.63f;
                    const float invFreq = (refFreq * refFreq) / modifiedPitch;
                    modifiedPitch = (1.0f - invertAmount) * modifiedPitch + invertAmount * invFreq;
                }

                if (quantStrength > 0.001f && modifiedPitch > 40.0f) {
                    const float midiNote = 69.0f + 12.0f * std::log2(modifiedPitch / 440.0f);
                    const float roundedMidi = std::round(midiNote);
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

                float playPitch = modifiedPitch;
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

                // 4. Parameter abfragen (mit Sequencer-Override)
                const float detuneCents    = getEffectiveParam(1,  "detune_cents",    mDetuneParam,    8.0f);
                const float stereoSpread   = getEffectiveParam(2,  "stereo_spread",   mSpreadParam,    0.7f);
                const float formantBlend   = getEffectiveParam(4,  "formant_blend",   mFormantParam,   0.5f);
                const float spectralTilt   = getEffectiveParam(3,  "spectral_tilt",   mTiltParam,      0.5f);
                const float transientTrack = getEffectiveParam(6,  "transient_track", mTransientParam, 0.5f);
                const float noiseGain      = getEffectiveParam(7,  "noise_gain",      mNoiseGainParam, 0.2f);

                const float harmBalance    = getEffectiveParam(14, "harmony_balance", mHarmBalanceParam, 0.5f);
                const auto mixMode         = static_cast<SynthesisMixMode>(mMixModeParam ? static_cast<int>(mMixModeParam->load()) : 0);

                // Stimmen-Pegel und Tonhöhen (Oktaven & Halbtöne via Sequencer)
                const float subGain        = getEffectiveParam(15, "sub_gain",       mSubGainParam,   0.35f);
                const float subOct         = getEffectiveParam(16, "sub_octave",     mSubOctParam,   -1.0f);
                const float subSemi        = getEffectiveParam(17, "sub_semitones",  mSubSemiParam,   0.0f);

                const float high1Gain      = getEffectiveParam(18, "high_gain",      mHigh1GainParam, 0.20f);
                const float high1Oct       = getEffectiveParam(19, "high_octave",    mHigh1OctParam,  1.0f);
                const float high1Semi      = getEffectiveParam(20, "high_semitones", mHigh1SemiParam, 0.0f);

                const float high2Gain      = getEffectiveParam(21, "high2_gain",     mHigh2GainParam, 0.00f);
                const float high2Oct       = getEffectiveParam(22, "high2_octave",   mHigh2OctParam,  2.0f);
                const float high2Semi      = getEffectiveParam(23, "high2_semitones", mHigh2SemiParam, 0.0f);

                const float high3Gain      = getEffectiveParam(24, "high3_gain",     mHigh3GainParam, 0.00f);
                const float high3Oct       = getEffectiveParam(25, "high3_octave",   mHigh3OctParam,  1.0f);
                const float high3Semi      = getEffectiveParam(26, "high3_semitones", mHigh3SemiParam, 7.0f);

                const std::array<WaveformType, 4> harmonyWaves = {
                    static_cast<WaveformType>(mSubWaveParam ? static_cast<int>(mSubWaveParam->load()) : 0),
                    static_cast<WaveformType>(mHigh1WaveParam ? static_cast<int>(mHigh1WaveParam->load()) : 0),
                    static_cast<WaveformType>(mHigh2WaveParam ? static_cast<int>(mHigh2WaveParam->load()) : 0),
                    static_cast<WaveformType>(mHigh3WaveParam ? static_cast<int>(mHigh3WaveParam->load()) : 0)
                };

                std::array<float, 4> driftCents = {0.0f, 0.0f, 0.0f, 0.0f};
                for (size_t d = 0; d < 4; ++d) {
                    voiceDriftPhases[d] += hopTimeSec * (0.3f + 0.2f * static_cast<float>(d));
                    if (voiceDriftPhases[d] >= 6.2831853f) voiceDriftPhases[d] -= 6.2831853f;
                    driftCents[d] = std::sin(voiceDriftPhases[d]) * driftCentsMax;
                }

                const float subPitchMult   = std::pow(2.0f, (subOct * 12.0f + subSemi + driftCents[0] / 100.0f) / 12.0f);
                const float high1PitchMult = std::pow(2.0f, (high1Oct * 12.0f + high1Semi + driftCents[1] / 100.0f) / 12.0f);
                const float high2PitchMult = std::pow(2.0f, (high2Oct * 12.0f + high2Semi + driftCents[2] / 100.0f) / 12.0f);
                const float high3PitchMult = std::pow(2.0f, (high3Oct * 12.0f + high3Semi + driftCents[3] / 100.0f) / 12.0f);


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

                // 8. Oszillatoren mit Verknüpfungsmodi berechnen
                float synthTargetF0 = (playPitch >= 50.0f) ? playPitch : (mPrevF0 > 0.0f ? mPrevF0 : 220.0f);
                const float bodyCutoffHz = 1800.0f + spectralTilt * 12200.0f;
                const float tiltGainComp = 1.0f + (1.0f - spectralTilt) * 1.2f;

                mSynthesizer.processBlock(
                    mPrevF0 > 0.0f ? mPrevF0 : synthTargetF0,
                    synthTargetF0,
                    mPrevAmps.data(), mCurrAmps.data(),
                    synthBufferL.data(), synthBufferR.data(), hopSize,
                    detuneCents, stereoSpread,
                    subGain, high1Gain, high2Gain, high3Gain,
                    bodyCutoffHz,
                    subPitchMult, high1PitchMult, high2PitchMult, high3PitchMult,
                    harmonyWaves, harmBalance, mixMode
                );

                mPrevF0 = synthTargetF0;
                mPrevAmps = mCurrAmps;

                for (size_t i = 0; i < hopSize; ++i) {
                    synthBufferL[i] *= tiltGainComp;
                    synthBufferR[i] *= tiltGainComp;
                }

                // 9. Kontinuierlicher Rauschsynthesizer
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
    float getEffectiveParam(size_t paramIdx, const char* paramId, std::atomic<float>* rawAtomic, float defaultVal) const noexcept {
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

    std::atomic<float>* mDetuneParam       = nullptr;
    std::atomic<float>* mSpreadParam       = nullptr;
    std::atomic<float>* mToleranceParam    = nullptr;
    std::atomic<float>* mFormantParam      = nullptr;
    std::atomic<float>* mTiltParam         = nullptr;
    std::atomic<float>* mTransientParam    = nullptr;
    std::atomic<float>* mNoiseGainParam    = nullptr;

    std::atomic<float>* mPitchQuantParam   = nullptr;
    std::atomic<float>* mPitchInertiaParam = nullptr;
    std::atomic<float>* mPitchFreezeParam  = nullptr;
    std::atomic<float>* mPitchInvertParam  = nullptr;
    std::atomic<float>* mVoiceDriftParam   = nullptr;

    std::atomic<float>* mHarmBalanceParam  = nullptr;
    std::atomic<float>* mMixModeParam      = nullptr;

    std::atomic<float>* mSubGainParam      = nullptr;
    std::atomic<float>* mSubOctParam       = nullptr;
    std::atomic<float>* mSubSemiParam      = nullptr;
    std::atomic<float>* mSubWaveParam      = nullptr;

    std::atomic<float>* mHigh1GainParam    = nullptr;
    std::atomic<float>* mHigh1OctParam     = nullptr;
    std::atomic<float>* mHigh1SemiParam    = nullptr;
    std::atomic<float>* mHigh1WaveParam    = nullptr;

    std::atomic<float>* mHigh2GainParam    = nullptr;
    std::atomic<float>* mHigh2OctParam     = nullptr;
    std::atomic<float>* mHigh2SemiParam    = nullptr;
    std::atomic<float>* mHigh2WaveParam    = nullptr;

    std::atomic<float>* mHigh3GainParam    = nullptr;
    std::atomic<float>* mHigh3OctParam     = nullptr;
    std::atomic<float>* mHigh3SemiParam    = nullptr;
    std::atomic<float>* mHigh3WaveParam    = nullptr;

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

    float mNoiseFilterState = 0.0f;
};