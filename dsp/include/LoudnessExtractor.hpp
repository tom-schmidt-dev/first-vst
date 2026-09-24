#pragma once
#include <cmath>
#include <algorithm>
#include <cstddef>

class LoudnessExtractor {
public:
    LoudnessExtractor(float loudnessFloorDb = -80.0f)
        : mLoudnessFloorDb(loudnessFloorDb) {}

    // Prozessiert einen Audio-Puffer von hopSize Samples
    float processBlock(const float* buffer, size_t numSamples) noexcept {
        if (numSamples == 0) {
            return 0.0f;
        }

        float sumSquares = 0.0f;
        for (size_t i = 0; i < numSamples; ++i) {
            sumSquares += buffer[i] * buffer[i];
        }

        float rms = std::sqrt(sumSquares / static_cast<float>(numSamples));
        rms = std::max(rms, 1e-5f);

        // Umrechnung in dBFS
        float db = 20.0f * std::log10(rms);
        db = std::clamp(db, mLoudnessFloorDb, 0.0f);

        // Skalierung auf [0.0, 1.0]
        float normalized = (db - mLoudnessFloorDb) / (-mLoudnessFloorDb);
        return normalized;
    }

private:
    float mLoudnessFloorDb;
};
