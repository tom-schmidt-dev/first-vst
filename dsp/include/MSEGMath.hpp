#pragma once
#include <cmath>
#include <algorithm>

enum class SegmentCurveType {
    Linear = 0,
    Exponential,
    Logarithmic,
    SCurve,
    Stepped
};

struct SequencerNode {
    float x = 0.0f;                             // 0.0 bis 1.0 (Phase im Zyklus)
    float y = 0.5f;                             // 0.0 bis 1.0 (Normalisierter Parameterwert)
    SegmentCurveType curve = SegmentCurveType::Linear; // Kurventyp zum nächsten Knoten
    float tension = 0.0f;                       // -1.0 bis +1.0 (Biegt die Segmentform)

    bool operator<(const SequencerNode& other) const noexcept {
        return x < other.x;
    }
};

/**
 * Interpoliert ein Segment zwischen (0, y0) und (1, y1) bei normiertem Fortschritt u in [0, 1].
 */
inline float interpolateSegment(float u, float y0, float y1, SegmentCurveType type, float tension) noexcept {
    u = std::clamp(u, 0.0f, 1.0f);

    // 1. Spannungsbiegung (Tension) anwenden: u_bent = u^(2^(2 * tension))
    float u_bent = u;
    if (std::abs(tension) > 0.005f) {
        const float p = std::pow(2.0f, 2.0f * std::clamp(tension, -1.0f, 1.0f));
        u_bent = std::pow(u, p);
    }

    // 2. Geometrischen Kurventyp anwenden
    float shaped = u_bent;
    switch (type) {
        case SegmentCurveType::Linear:
            shaped = u_bent;
            break;

        case SegmentCurveType::Exponential: {
            const float k = 3.0f;
            shaped = (std::exp(k * u_bent) - 1.0f) / (std::exp(k) - 1.0f);
            break;
        }

        case SegmentCurveType::Logarithmic: {
            const float eMinus1 = 1.718281828459f;
            shaped = std::log(1.0f + eMinus1 * u_bent);
            break;
        }

        case SegmentCurveType::SCurve: {
            // Hermite / Smoothstep: 3*u^2 - 2*u^3
            shaped = u_bent * u_bent * (3.0f - 2.0f * u_bent);
            break;
        }

        case SegmentCurveType::Stepped: {
            shaped = (u < 1.0f) ? 0.0f : 1.0f;
            break;
        }
    }

    return std::clamp(y0 + shaped * (y1 - y0), 0.0f, 1.0f);
}
