#pragma once
#include <cmath>
#include <algorithm>
#include <cstdlib>

enum class LFOWaveform {
    Sine = 0,
    Triangle,
    SawUp,
    SawDown,
    Square,
    SampleAndHold,
    NumWaveforms
};

class LFO {
public:
    explicit LFO(float sampleRate = 44100.0f) : mSampleRate(sampleRate) {}

    void setSampleRate(float sampleRate) noexcept {
        mSampleRate = std::max(100.0f, sampleRate);
    }

    void reset() noexcept {
        mPhase = 0.0f;
        mLastSampleAndHold = 0.0f;
    }

    float processSample(float frequencyHz, LFOWaveform wave) noexcept {
        const float freq = std::max(0.01f, frequencyHz);
        const float phaseInc = freq / mSampleRate;

        float output = 0.0f;

        switch (wave) {
            case LFOWaveform::Sine:
                output = std::sin(mPhase * 6.283185307179586f);
                break;
            case LFOWaveform::Triangle:
                output = 4.0f * std::abs(mPhase - 0.5f) - 1.0f;
                break;
            case LFOWaveform::SawUp:
                output = 2.0f * mPhase - 1.0f;
                break;
            case LFOWaveform::SawDown:
                output = 1.0f - 2.0f * mPhase;
                break;
            case LFOWaveform::Square:
                output = (mPhase < 0.5f) ? 1.0f : -1.0f;
                break;
            case LFOWaveform::SampleAndHold:
                if (mPhase < phaseInc) {
                    mLastSampleAndHold = (static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) * 0.5f)) - 1.0f;
                }
                output = mLastSampleAndHold;
                break;
            default:
                output = 0.0f;
                break;
        }

        mPhase += phaseInc;
        if (mPhase >= 1.0f) {
            mPhase -= std::floor(mPhase);
        }

        return output;
    }

private:
    float mSampleRate = 44100.0f;
    float mPhase = 0.0f;
    float mLastSampleAndHold = 0.0f;
};