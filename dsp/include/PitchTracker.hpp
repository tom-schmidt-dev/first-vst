#pragma once
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>

class PitchTracker {
public:
    PitchTracker(size_t bufferSize = 2048, float sampleRate = 44100.0f, float threshold = 0.25f)
    : mBufferSize(bufferSize),
    mSampleRate(sampleRate),
    mThreshold(threshold),
    mBuffer(bufferSize, 0.0f),
    mDiffBuffer(bufferSize, 0.0f)
    {
    }

    void setSampleRate(float sampleRate) noexcept {
        mSampleRate = sampleRate;
    }

    void setThreshold(float threshold) noexcept {
        mThreshold = threshold;
    }

    void reset() noexcept {
        std::fill(mBuffer.begin(), mBuffer.end(), 0.0f);
        std::fill(mDiffBuffer.begin(), mDiffBuffer.end(), 0.0f);
    }

    float processBlock(const float* input, size_t numSamples, float maxAperiodicity = 0.70f) noexcept {
        if (numSamples == 0 || input == nullptr) return 0.0f;

        if (numSamples >= mBufferSize) {
            std::copy_n(input + (numSamples - mBufferSize), mBufferSize, mBuffer.begin());
        } else {
            std::copy(mBuffer.begin() + numSamples, mBuffer.end(), mBuffer.begin());
            std::copy_n(input, numSamples, mBuffer.end() - numSamples);
        }

        const size_t halfBuffer = mBufferSize / 2;

        const size_t minTau = std::max<size_t>(2, static_cast<size_t>(mSampleRate / 1200.0f));
        const size_t maxTau = std::min<size_t>(halfBuffer - 1, static_cast<size_t>(mSampleRate / 60.0f));

        if (minTau >= maxTau || maxTau >= halfBuffer) {
            return 0.0f;
        }

        std::fill(mDiffBuffer.begin(), mDiffBuffer.end(), 0.0f);

        for (size_t tau = minTau; tau <= maxTau; ++tau) {
            float sum = 0.0f;
            for (size_t j = 0; j < halfBuffer; j += 2) {
                const float diff = mBuffer[j] - mBuffer[j + tau];
                sum += diff * diff;
            }
            mDiffBuffer[tau] = sum * 2.0f;
        }

        mDiffBuffer[0] = 1.0f;
        float runningSum = 0.0f;

        for (size_t tau = 1; tau < minTau; ++tau) {
            runningSum += mDiffBuffer[tau];
            mDiffBuffer[tau] = 1.0f;
        }

        for (size_t tau = minTau; tau <= maxTau; ++tau) {
            runningSum += mDiffBuffer[tau];
            if (runningSum > 0.00001f) {
                mDiffBuffer[tau] *= static_cast<float>(tau) / runningSum;
            } else {
                mDiffBuffer[tau] = 1.0f;
            }
        }

        size_t tauEstimate = 0;
        for (size_t tau = minTau; tau <= maxTau; ++tau) {
            if (mDiffBuffer[tau] < mThreshold) {
                while (tau + 1 <= maxTau && mDiffBuffer[tau + 1] < mDiffBuffer[tau]) {
                    tau++;
                }
                tauEstimate = tau;
                break;
            }
        }

        if (tauEstimate == 0) {
            auto minIt = std::min_element(mDiffBuffer.begin() + minTau, mDiffBuffer.begin() + maxTau + 1);
            tauEstimate = std::distance(mDiffBuffer.begin(), minIt);
            if (mDiffBuffer[tauEstimate] >= maxAperiodicity) {
                return 0.0f;
            }
        }

        float betterTau = static_cast<float>(tauEstimate);
        if (tauEstimate > minTau && tauEstimate < maxTau) {
            const float s0 = mDiffBuffer[tauEstimate - 1];
            const float s1 = mDiffBuffer[tauEstimate];
            const float s2 = mDiffBuffer[tauEstimate + 1];
            const float bottom = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::abs(bottom) > 0.00001f) {
                betterTau += (s2 - s0) / bottom;
            }
        }

        return mSampleRate / betterTau;
    }

private:
    size_t mBufferSize;
    float mSampleRate;
    float mThreshold;
    std::vector<float> mBuffer;
    std::vector<float> mDiffBuffer;
};
