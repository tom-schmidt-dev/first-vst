#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <onnxruntime_cxx_api.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
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
        mDetuneParam = apvts.getRawParameterValue("detune_cents");
        mSpreadParam = apvts.getRawParameterValue("stereo_spread");
        mSubGainParam = apvts.getRawParameterValue("sub_gain");
        mHighGainParam = apvts.getRawParameterValue("high_gain");
        mToleranceParam = apvts.getRawParameterValue("tracking_tolerance");

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

        std::array<int64_t, 3> inputShape = {1, 1, 1};
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        const char* inputNames[] = {"f0", "loudness"};
        const char* outputNames[] = {"harmonic_amps", "overall_amp", "noise_mags"};

        std::array<float, 3> pitchHistory = {220.0f, 220.0f, 220.0f};
        int unvoicedHoldCounter = 0;
        const int maxHoldFrames = 50;

        float smoothedAmp = 0.0f;

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

                // 3-Punkte Median-Filter
                pitchHistory[0] = pitchHistory[1];
                pitchHistory[1] = pitchHistory[2];
                pitchHistory[2] = validPitch;

                std::array<float, 3> sortedPitches = pitchHistory;
                std::sort(sortedPitches.begin(), sortedPitches.end());
                float filteredPitch = sortedPitches[1];

                // Oktavierung in spielbaren Bereich
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

                const bool isSounding = (normalizedLoudness > 0.06f);
                const float targetAmp = isSounding ? overallAmp : 0.0f;

                const float attackCoeff = 0.50f;
                const float releaseCoeff = 0.15f;
                if (targetAmp > smoothedAmp) {
                    smoothedAmp += attackCoeff * (targetAmp - smoothedAmp);
                } else {
                    smoothedAmp += releaseCoeff * (targetAmp - smoothedAmp);
                }

                for (size_t h = 0; h < 60; ++h) {
                    mCurrAmps[h] = rawAmps[h] * smoothedAmp;
                }

                float synthTargetF0 = (playPitch >= 50.0f) ? playPitch : (mPrevF0 > 0.0f ? mPrevF0 : 220.0f);

                // Parameter atomar laden
                const float detuneCents = mDetuneParam ? mDetuneParam->load() : 8.0f;
                const float stereoSpread = mSpreadParam ? mSpreadParam->load() : 0.7f;
                const float subGain = mSubGainParam ? mSubGainParam->load() : 0.35f;
                const float highGain = mHighGainParam ? mHighGainParam->load() : 0.2f;

                // 4. Stereo-Synthesizer berechnen
                mSynthesizer.processBlock(
                    mPrevF0 > 0.0f ? mPrevF0 : synthTargetF0,
                    synthTargetF0,
                    mPrevAmps.data(), mCurrAmps.data(),
                                          synthBufferL.data(), synthBufferR.data(), hopSize,
                                          detuneCents, stereoSpread, subGain, highGain
                );

                mPrevF0 = synthTargetF0;
                mPrevAmps = mCurrAmps;

                // Make-up Gain (+9 dB)
                const float makeupGain = 3.0f;
                for (size_t i = 0; i < hopSize; ++i) {
                    synthBufferL[i] *= makeupGain;
                    synthBufferR[i] *= makeupGain;
                }

                // 5. In Stereo Output-FIFOs schreiben
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

    std::atomic<float>* mDetuneParam = nullptr;
    std::atomic<float>* mSpreadParam = nullptr;
    std::atomic<float>* mSubGainParam = nullptr;
    std::atomic<float>* mHighGainParam = nullptr;
    std::atomic<float>* mToleranceParam = nullptr;

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
};
