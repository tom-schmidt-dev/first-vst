#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <memory>

struct SequencerNode {
    float x = 0.0f; // 0.0 bis 1.0 (Phase im Zyklus)
    float y = 0.5f; // 0.0 bis 1.0 (Normalisierter Parameterwert)

    bool operator<(const SequencerNode& other) const noexcept {
        return x < other.x;
    }
};

struct TrackedParamInfo {
    const char* id;
    const char* name;
    juce::Colour colour;
};

inline float denormaliseParam(const juce::NormalisableRange<float>& range, float norm) noexcept {
    const float clamped = std::clamp(norm, 0.0f, 1.0f);
    float val = 0.0f;
    if (std::abs(range.skew - 1.0f) < 0.001f) {
        val = range.start + clamped * (range.end - range.start);
    } else {
        val = range.start + (range.end - range.start) * std::pow(clamped, 1.0f / range.skew);
    }

    if (range.interval > 0.0f) {
        val = range.start + range.interval * std::floor((val - range.start) / range.interval + 0.5f);
    }
    return std::clamp(val, std::min(range.start, range.end), std::max(range.start, range.end));
}

static const TrackedParamInfo kTrackedParams[] = {
    { "dry_wet",            "Dry/Wet",      juce::Colour(0xff4a90e2) }, // 0
    { "detune_cents",       "Detune",       juce::Colour(0xff50e3c2) }, // 1
    { "stereo_spread",      "Spread",       juce::Colour(0xffb8e986) }, // 2
    { "spectral_tilt",      "Dyn Tilt",     juce::Colour(0xfff5a623) }, // 3
    { "formant_blend",      "Formant",      juce::Colour(0xfff8e71c) }, // 4
    { "tracking_tolerance", "Tolerance",    juce::Colour(0xffbd10e0) }, // 5
    { "transient_track",    "Attack",       juce::Colour(0xff9013fe) }, // 6
    { "noise_gain",         "Noise",        juce::Colour(0xff7ed321) }, // 7
    { "lfo_depth",          "LFO Depth",    juce::Colour(0xffff007f) }, // 8
    { "lfo_rate_hz",        "LFO Rate",     juce::Colour(0xffff4040) }, // 9
    { "pitch_quantize",     "Quantize",     juce::Colour(0xff00d2ff) }, // 10
    { "pitch_inertia",      "Inertia",      juce::Colour(0xff00e676) }, // 11
    { "pitch_inversion",    "Inversion",    juce::Colour(0xffff9100) }, // 12
    { "voice_drift",        "Drift",        juce::Colour(0xffe040fb) }, // 13
    { "harmony_balance",    "Balance",      juce::Colour(0xff40c4ff) }, // 14
    { "sub_gain",           "Sub Lvl",      juce::Colour(0xffffab00) }, // 15
    { "sub_octave",         "Sub Oct",      juce::Colour(0xffffd54f) }, // 16
    { "sub_semitones",      "Sub Semi",     juce::Colour(0xffffe082) }, // 17
    { "high_gain",          "H1 Lvl",       juce::Colour(0xffff5252) }, // 18
    { "high_octave",        "H1 Oct",       juce::Colour(0xffef9a9a) }, // 19
    { "high_semitones",     "H1 Semi",      juce::Colour(0xffff8a80) }, // 20
    { "high2_gain",         "H2 Lvl",       juce::Colour(0xffff4081) }, // 21
    { "high2_octave",       "H2 Oct",       juce::Colour(0xfff48fb1) }, // 22
    { "high2_semitones",    "H2 Semi",      juce::Colour(0xffff80ab) }, // 23
    { "high3_gain",         "H3 Lvl",       juce::Colour(0xff7c4dff) }, // 24
    { "high3_octave",       "H3 Oct",       juce::Colour(0xffb39ddb) }, // 25
    { "high3_semitones",    "H3 Semi",      juce::Colour(0xffb388ff) }  // 26
};
static constexpr size_t kNumTrackedParams = sizeof(kTrackedParams) / sizeof(kTrackedParams[0]);

class SequencerEngine {
public:
    static constexpr size_t kTableSize = 1024;

    SequencerEngine() {
        for (size_t i = 0; i < kNumTrackedParams; ++i) {
            mAutomated[i].store(false, std::memory_order_relaxed);
            mCurves[i] = { {0.0f, 0.5f}, {1.0f, 0.5f} };
            rebuildLookupTable(i);
        }
        mCurrentPhase.store(0.0f, std::memory_order_relaxed);
        mStepCount.store(16, std::memory_order_relaxed);
    }

    void setStepCount(int steps) noexcept {
        mStepCount.store(std::clamp(steps, 1, 32), std::memory_order_relaxed);
    }

    int getStepCount() const noexcept {
        return mStepCount.load(std::memory_order_relaxed);
    }

    void setAutomated(size_t paramIdx, bool automated) noexcept {
        if (paramIdx < kNumTrackedParams) {
            mAutomated[paramIdx].store(automated, std::memory_order_relaxed);
        }
    }

    bool isAutomated(size_t paramIdx) const noexcept {
        if (paramIdx < kNumTrackedParams) {
            return mAutomated[paramIdx].load(std::memory_order_relaxed);
        }
        return false;
    }

    float getCurrentPhase() const noexcept {
        return mCurrentPhase.load(std::memory_order_relaxed);
    }

    void updateTransport(double ppqPosition, bool isPlaying) noexcept {
        if (!isPlaying && ppqPosition < 0.0) return;

        const int steps = mStepCount.load(std::memory_order_relaxed);
        const double cycleBeats = static_cast<double>(steps) * 0.25;

        double phase = 0.0;
        if (cycleBeats > 0.0001) {
            phase = std::fmod(ppqPosition, cycleBeats) / cycleBeats;
            if (phase < 0.0) phase += 1.0;
        }

        mCurrentPhase.store(static_cast<float>(phase), std::memory_order_relaxed);
    }

    float getInterpolatedValue(size_t paramIdx) const noexcept {
        if (paramIdx >= kNumTrackedParams) return 0.5f;

        const float phase = mCurrentPhase.load(std::memory_order_relaxed);
        const size_t index = static_cast<size_t>(std::clamp(phase * static_cast<float>(kTableSize - 1), 0.0f, static_cast<float>(kTableSize - 1)));
        return mLookupTables[paramIdx][index];
    }

    const std::vector<SequencerNode>& getNodes(size_t paramIdx) const {
        return mCurves[paramIdx];
    }

    void setNodes(size_t paramIdx, const std::vector<SequencerNode>& nodes) {
        if (paramIdx >= kNumTrackedParams || nodes.size() < 2) return;
        mCurves[paramIdx] = nodes;
        std::sort(mCurves[paramIdx].begin(), mCurves[paramIdx].end());
        rebuildLookupTable(paramIdx);
    }

    void addOrMoveNode(size_t paramIdx, float x, float y) {
        if (paramIdx >= kNumTrackedParams) return;

        x = std::clamp(x, 0.0f, 1.0f);
        y = std::clamp(y, 0.0f, 1.0f);

        auto& curve = mCurves[paramIdx];

        if (x <= 0.005f) {
            curve.front().y = y;
            rebuildLookupTable(paramIdx);
            return;
        }
        if (x >= 0.995f) {
            curve.back().y = y;
            rebuildLookupTable(paramIdx);
            return;
        }

        auto it = std::find_if(curve.begin(), curve.end(), [x](const SequencerNode& n) {
            return std::abs(n.x - x) < 0.015f;
        });

        if (it != curve.end()) {
            it->y = y;
        } else {
            curve.push_back({x, y});
            std::sort(curve.begin(), curve.end());
        }

        rebuildLookupTable(paramIdx);
    }

    bool removeNode(size_t paramIdx, float x, float radius = 0.03f) {
        if (paramIdx >= kNumTrackedParams) return false;

        auto& curve = mCurves[paramIdx];
        if (curve.size() <= 2) return false;

        for (auto it = curve.begin() + 1; it != curve.end() - 1; ++it) {
            if (std::abs(it->x - x) <= radius) {
                curve.erase(it);
                rebuildLookupTable(paramIdx);
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<juce::XmlElement> exportXml() const {
        auto xml = std::make_unique<juce::XmlElement>("SEQUENCER");
        xml->setAttribute("stepCount", mStepCount.load());

        for (size_t p = 0; p < kNumTrackedParams; ++p) {
            auto* cXml = xml->createNewChildElement("CURVE");
            cXml->setAttribute("id", kTrackedParams[p].id);
            cXml->setAttribute("automated", mAutomated[p].load());

            for (const auto& node : mCurves[p]) {
                auto* nXml = cXml->createNewChildElement("NODE");
                nXml->setAttribute("x", static_cast<double>(node.x));
                nXml->setAttribute("y", static_cast<double>(node.y));
            }
        }
        return xml;
    }

    void importXml(const juce::XmlElement* xml) {
        if (!xml || !xml->hasTagName("SEQUENCER")) return;
        setStepCount(xml->getIntAttribute("stepCount", 16));

        for (auto* cXml : xml->getChildIterator()) {
            if (cXml->hasTagName("CURVE")) {
                const juce::String id = cXml->getStringAttribute("id");
                for (size_t p = 0; p < kNumTrackedParams; ++p) {
                    if (id == kTrackedParams[p].id) {
                        mAutomated[p].store(cXml->getBoolAttribute("automated", false));
                        std::vector<SequencerNode> nodes;
                        for (auto* nXml : cXml->getChildIterator()) {
                            if (nXml->hasTagName("NODE")) {
                                const float nx = static_cast<float>(nXml->getDoubleAttribute("x", 0.0));
                                const float ny = static_cast<float>(nXml->getDoubleAttribute("y", 0.5));
                                nodes.push_back({nx, ny});
                            }
                        }
                        if (nodes.size() >= 2) {
                            setNodes(p, nodes);
                        }
                        break;
                    }
                }
            }
        }
    }

private:
    void rebuildLookupTable(size_t paramIdx) {
        const auto& nodes = mCurves[paramIdx];
        if (nodes.size() < 2) return;

        auto& table = mLookupTables[paramIdx];
        size_t nodeIdx = 0;

        for (size_t i = 0; i < kTableSize; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kTableSize - 1);

            while (nodeIdx + 1 < nodes.size() && nodes[nodeIdx + 1].x < t) {
                nodeIdx++;
            }

            if (nodeIdx + 1 >= nodes.size()) {
                table[i] = nodes.back().y;
            } else {
                const auto& p0 = nodes[nodeIdx];
                const auto& p1 = nodes[nodeIdx + 1];
                const float dx = p1.x - p0.x;
                const float frac = (dx > 0.00001f) ? (t - p0.x) / dx : 0.0f;
                table[i] = std::clamp(p0.y + frac * (p1.y - p0.y), 0.0f, 1.0f);
            }
        }
    }

    std::atomic<int> mStepCount{16};
    std::atomic<float> mCurrentPhase{0.0f};
    std::array<std::atomic<bool>, kNumTrackedParams> mAutomated;

    std::array<std::vector<SequencerNode>, kNumTrackedParams> mCurves;
    std::array<std::array<float, kTableSize>, kNumTrackedParams> mLookupTables;
};