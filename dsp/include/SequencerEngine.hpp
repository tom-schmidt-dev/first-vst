#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <memory>
#include "MSEGMath.hpp"

enum class SampleSyncMode {
    TimelineSync = 0,   // Phasenstarr zum Sequencer-Playhead
    ClassicResample = 1 // Tonhöhenabhängiges Resampling (Fractional Read Pointer)
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
    { "volume",             "Volume",       juce::Colour(0xffff007f) }, // 0: Dedizierte Lautstärke-Hüllkurve
    { "dry_wet",            "Dry/Wet",      juce::Colour(0xff4a90e2) }, // 1
    { "spectral_tilt",      "Dyn Tilt",     juce::Colour(0xfff5a623) }, // 2
    { "transient_track",    "Attack",       juce::Colour(0xff9013fe) }, // 3
    { "noise_gain",         "Noise",        juce::Colour(0xff7ed321) }, // 4
    { "pitch_quantize",     "Quantize",     juce::Colour(0xff00d2ff) }, // 5
    { "pitch_inertia",      "Inertia",      juce::Colour(0xff00e676) }, // 6
    { "pitch_inversion",    "Inversion",    juce::Colour(0xffff9100) }, // 7
    { "voice_drift",        "Drift",        juce::Colour(0xffe040fb) }, // 8

    // Voice 1 (Lead / DDSP)
    { "v1_gain",            "V1 Gain",      juce::Colour(0xff50e3c2) }, // 9
    { "v1_octave",          "V1 Oct",       juce::Colour(0xff40c4ff) }, // 10
    { "v1_semitones",       "V1 Semi",      juce::Colour(0xff80d8ff) }, // 11
    { "v1_cents",           "V1 Cent",      juce::Colour(0xffb388ff) }, // 12
    { "v1_pan",             "V1 Pan",       juce::Colour(0xffb8e986) }, // 13

    // Voice 2 (Sub)
    { "v2_gain",            "V2 Gain",      juce::Colour(0xffffab00) }, // 14
    { "v2_octave",          "V2 Oct",       juce::Colour(0xffffd54f) }, // 15
    { "v2_semitones",       "V2 Semi",      juce::Colour(0xffffe082) }, // 16
    { "v2_cents",           "V2 Cent",      juce::Colour(0xffffcc80) }, // 17
    { "v2_pan",             "V2 Pan",       juce::Colour(0xffffb74d) }, // 18

    // Voice 3 (Harm 1)
    { "v3_gain",            "V3 Gain",      juce::Colour(0xffff5252) }, // 19
    { "v3_octave",          "V3 Oct",       juce::Colour(0xffef9a9a) }, // 20
    { "v3_semitones",       "V3 Semi",      juce::Colour(0xffff8a80) }, // 21
    { "v3_cents",           "V3 Cent",      juce::Colour(0xffff80ab) }, // 22
    { "v3_pan",             "V3 Pan",       juce::Colour(0xffff4081) }, // 23

    // Voice 4 (Harm 2)
    { "v4_gain",            "V4 Gain",      juce::Colour(0xff7c4dff) }, // 24
    { "v4_octave",          "V4 Oct",       juce::Colour(0xffb39ddb) }, // 25
    { "v4_semitones",       "V4 Semi",      juce::Colour(0xffd1c4e9) }, // 26
    { "v4_cents",           "V4 Cent",      juce::Colour(0xffea80fc) }, // 27
    { "v4_pan",             "V4 Pan",       juce::Colour(0xffe040fb) }, // 28

    // Voice 5 (Harm 3)
    { "v5_gain",            "V5 Gain",      juce::Colour(0xff00e5ff) }, // 29
    { "v5_octave",          "V5 Oct",       juce::Colour(0xff18ffff) }, // 30
    { "v5_semitones",       "V5 Semi",      juce::Colour(0xff84ffff) }, // 31
    { "v5_cents",           "V5 Cent",      juce::Colour(0xffa7ffeb) }, // 32
    { "v5_pan",             "V5 Pan",       juce::Colour(0xff64ffda) }  // 33
};
static constexpr size_t kNumTrackedParams = sizeof(kTrackedParams) / sizeof(kTrackedParams[0]);

class SequencerEngine {
public:
    static constexpr size_t kTableSize = 1024;

    SequencerEngine() {
        for (size_t i = 0; i < kNumTrackedParams; ++i) {
            mAutomated[i].store(false, std::memory_order_relaxed);
            mHasUserNodes[i].store(false, std::memory_order_relaxed);
            mManualValues[i].store(0.5f, std::memory_order_relaxed);
            mCurves[i] = { {0.0f, 0.5f, SegmentCurveType::Linear, 0.0f},
                           {1.0f, 0.5f, SegmentCurveType::Linear, 0.0f} };
            rebuildLookupTable(i);
        }
        // Master Volume (Index 0) initialisiert mit vollem Pegel 1.0
        mManualValues[0].store(1.0f, std::memory_order_relaxed);
        mCurves[0] = { {0.0f, 1.0f, SegmentCurveType::Linear, 0.0f},
                       {1.0f, 1.0f, SegmentCurveType::Linear, 0.0f} };
        rebuildLookupTable(0);

        mCurrentPhase.store(0.0f, std::memory_order_relaxed);
        mStepCount.store(16, std::memory_order_relaxed);
        mGridSnap.store(16, std::memory_order_relaxed);
    }

    int findParamIndex(const juce::String& paramId) const noexcept {
        for (size_t i = 0; i < kNumTrackedParams; ++i) {
            if (paramId == kTrackedParams[i].id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void setStepCount(int steps) noexcept {
        mStepCount.store(std::clamp(steps, 1, 32), std::memory_order_relaxed);
    }

    int getStepCount() const noexcept {
        return mStepCount.load(std::memory_order_relaxed);
    }

    void setGridSnap(int snap) noexcept {
        mGridSnap.store(snap, std::memory_order_relaxed);
    }

    int getGridSnap() const noexcept {
        return mGridSnap.load(std::memory_order_relaxed);
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

    bool hasUserNodes(size_t paramIdx) const noexcept {
        if (paramIdx < kNumTrackedParams) {
            return mHasUserNodes[paramIdx].load(std::memory_order_relaxed);
        }
        return false;
    }

    float getCurrentPhase() const noexcept {
        return mCurrentPhase.load(std::memory_order_relaxed);
    }

    void updateTransport(double ppqPosition, bool isPlaying, double bpm = 120.0, double sampleRate = 44100.0, int numSamples = 0) noexcept {
        mIsTransportPlaying.store(isPlaying, std::memory_order_relaxed);

        if (isPlaying) {
            const int steps = mStepCount.load(std::memory_order_relaxed);
            const double cycleBeats = static_cast<double>(steps) * 0.25;

            double phase = 0.0;
            if (cycleBeats > 0.0001) {
                phase = std::fmod(ppqPosition, cycleBeats) / cycleBeats;
                if (phase < 0.0) phase += 1.0;
            }

            mCurrentPhase.store(static_cast<float>(phase), std::memory_order_relaxed);
        } else if (numSamples > 0) {
            // Wenn DAW-Transport pausiert ist oder in Standalone: Freilaufender / Noten-getriggerter Takt
            const int steps = mStepCount.load(std::memory_order_relaxed);
            const double cycleBeats = static_cast<double>(steps) * 0.25;
            const double cycleSec = cycleBeats * (60.0 / std::max(20.0, bpm));
            const double cycleSamples = cycleSec * std::max(100.0, sampleRate);
            if (cycleSamples > 1.0) {
                float curPhase = mCurrentPhase.load(std::memory_order_relaxed);
                curPhase += static_cast<float>(static_cast<double>(numSamples) / cycleSamples);
                if (curPhase >= 1.0f) curPhase -= std::floor(curPhase);
                mCurrentPhase.store(curPhase, std::memory_order_relaxed);
            }
        }
    }

    void onNoteOn() noexcept {
        if (!mIsTransportPlaying.load(std::memory_order_relaxed)) {
            mCurrentPhase.store(0.0f, std::memory_order_relaxed);
        }
        resetClassicPlayback();
    }

    float getInterpolatedValue(size_t paramIdx) const noexcept {
        if (paramIdx >= kNumTrackedParams) return 0.5f;

        const float phase = mCurrentPhase.load(std::memory_order_relaxed);
        const size_t index = static_cast<size_t>(std::clamp(phase * static_cast<float>(kTableSize - 1), 0.0f, static_cast<float>(kTableSize - 1)));
        return mLookupTables[paramIdx][index];
    }

    float getInterpolatedValueById(const juce::String& paramId) const noexcept {
        int idx = findParamIndex(paramId);
        if (idx >= 0) return getInterpolatedValue(static_cast<size_t>(idx));
        return 0.5f;
    }

    /**
     * Regler-Standardlinien-Logik:
     * Definiert die Position der Nulllinie, solange der Benutzer keine eigenen Punkte gesetzt hat.
     */
    void setManualParamValue(size_t paramIdx, float normVal) {
        if (paramIdx >= kNumTrackedParams) return;
        normVal = std::clamp(normVal, 0.0f, 1.0f);
        mManualValues[paramIdx].store(normVal, std::memory_order_relaxed);

        if (!mHasUserNodes[paramIdx].load(std::memory_order_relaxed)) {
            mCurves[paramIdx] = { {0.0f, normVal, SegmentCurveType::Linear, 0.0f},
                                  {1.0f, normVal, SegmentCurveType::Linear, 0.0f} };
            rebuildLookupTable(paramIdx);
        }
    }

    float getManualParamValue(size_t paramIdx) const noexcept {
        if (paramIdx >= kNumTrackedParams) return 0.5f;
        return mManualValues[paramIdx].load(std::memory_order_relaxed);
    }

    /**
     * Setzt die Kurve wieder auf eine flache Standardlinie des aktuellen Reglerwerts zurück.
     */
    void clearUserNodes(size_t paramIdx) {
        if (paramIdx >= kNumTrackedParams) return;
        const float val = mManualValues[paramIdx].load(std::memory_order_relaxed);
        mHasUserNodes[paramIdx].store(false, std::memory_order_relaxed);
        mAutomated[paramIdx].store(false, std::memory_order_relaxed);
        mCurves[paramIdx] = { {0.0f, val, SegmentCurveType::Linear, 0.0f},
                              {1.0f, val, SegmentCurveType::Linear, 0.0f} };
        rebuildLookupTable(paramIdx);
    }

    const std::vector<SequencerNode>& getNodes(size_t paramIdx) const {
        return mCurves[paramIdx];
    }

    void setNodes(size_t paramIdx, const std::vector<SequencerNode>& nodes) {
        if (paramIdx >= kNumTrackedParams || nodes.size() < 2) return;
        mCurves[paramIdx] = nodes;
        std::sort(mCurves[paramIdx].begin(), mCurves[paramIdx].end());
        mHasUserNodes[paramIdx].store(true, std::memory_order_relaxed);
        mAutomated[paramIdx].store(true, std::memory_order_relaxed);
        rebuildLookupTable(paramIdx);
    }

    void addOrMoveNode(size_t paramIdx, float x, float y,
                       SegmentCurveType curve = SegmentCurveType::Linear,
                       float tension = 0.0f) {
        if (paramIdx >= kNumTrackedParams) return;

        x = std::clamp(x, 0.0f, 1.0f);
        y = std::clamp(y, 0.0f, 1.0f);

        auto& c = mCurves[paramIdx];
        mHasUserNodes[paramIdx].store(true, std::memory_order_relaxed);
        mAutomated[paramIdx].store(true, std::memory_order_relaxed);

        if (x <= 0.005f) {
            c.front().y = y;
            rebuildLookupTable(paramIdx);
            return;
        }
        if (x >= 0.995f) {
            c.back().y = y;
            rebuildLookupTable(paramIdx);
            return;
        }

        auto it = std::find_if(c.begin(), c.end(), [x](const SequencerNode& n) {
            return std::abs(n.x - x) < 0.015f;
        });

        if (it != c.end()) {
            it->y = y;
        } else {
            c.push_back({x, y, curve, tension});
            std::sort(c.begin(), c.end());
        }

        rebuildLookupTable(paramIdx);
    }

    void setSegmentTension(size_t paramIdx, size_t nodeIndex, float tension) {
        if (paramIdx >= kNumTrackedParams) return;
        auto& c = mCurves[paramIdx];
        if (nodeIndex < c.size()) {
            c[nodeIndex].tension = std::clamp(tension, -1.0f, 1.0f);
            mHasUserNodes[paramIdx].store(true, std::memory_order_relaxed);
            mAutomated[paramIdx].store(true, std::memory_order_relaxed);
            rebuildLookupTable(paramIdx);
        }
    }

    void setSegmentCurveType(size_t paramIdx, size_t nodeIndex, SegmentCurveType type) {
        if (paramIdx >= kNumTrackedParams) return;
        auto& c = mCurves[paramIdx];
        if (nodeIndex < c.size()) {
            c[nodeIndex].curve = type;
            mHasUserNodes[paramIdx].store(true, std::memory_order_relaxed);
            mAutomated[paramIdx].store(true, std::memory_order_relaxed);
            rebuildLookupTable(paramIdx);
        }
    }

    bool removeNode(size_t paramIdx, float x, float radius = 0.03f) {
        if (paramIdx >= kNumTrackedParams) return false;

        auto& c = mCurves[paramIdx];
        if (c.size() <= 2) return false;

        for (auto it = c.begin() + 1; it != c.end() - 1; ++it) {
            if (std::abs(it->x - x) <= radius) {
                c.erase(it);
                rebuildLookupTable(paramIdx);
                return true;
            }
        }
        return false;
    }

    // --- SAMPLE DRAG & DROP & PLAYBACK ---

    void setSampleSyncMode(SampleSyncMode mode) noexcept {
        mSampleSyncMode.store(mode, std::memory_order_relaxed);
    }

    SampleSyncMode getSampleSyncMode() const noexcept {
        return mSampleSyncMode.load(std::memory_order_relaxed);
    }

    bool loadSample(juce::AudioFormatReader* reader) {
        if (!reader || reader->lengthInSamples == 0) return false;

        juce::ScopedLock sl(mSampleLock);
        const int numChans = std::min(2, static_cast<int>(reader->numChannels));
        const int numSamps = static_cast<int>(reader->lengthInSamples);

        mSampleBuffer.setSize(numChans, numSamps);
        reader->read(&mSampleBuffer, 0, numSamps, 0, true, true);
        mSampleSampleRate = reader->sampleRate;
        mClassicPlaybackPointer.store(0.0f, std::memory_order_relaxed);
        mHasSample.store(true, std::memory_order_release);
        return true;
    }

    bool loadSampleFile(const juce::File& file, juce::AudioFormatManager& formatMgr) {
        if (!file.existsAsFile()) return false;
        std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(file));
        return loadSample(reader.get());
    }

    void clearSample() noexcept {
        juce::ScopedLock sl(mSampleLock);
        mSampleBuffer.setSize(0, 0);
        mHasSample.store(false, std::memory_order_release);
        mClassicPlaybackPointer.store(0.0f, std::memory_order_relaxed);
    }

    bool hasSample() const noexcept {
        return mHasSample.load(std::memory_order_acquire);
    }

    int getSampleLength() const noexcept {
        juce::ScopedLock sl(mSampleLock);
        return mSampleBuffer.getNumSamples();
    }

    const juce::AudioBuffer<float>& getSampleBuffer() const noexcept {
        return mSampleBuffer;
    }

    juce::CriticalSection& getSampleLock() noexcept {
        return mSampleLock;
    }

    void resetClassicPlayback() noexcept {
        mClassicPlaybackPointer.store(0.0f, std::memory_order_relaxed);
    }

    /**
     * Liest ein Sample im Modus A (TimelineSync) oder Modus B (ClassicResample).
     */
    float readSampleAudio(float phase, float midiPitchRatio = 1.0f, double engineSampleRate = 44100.0, int channel = 0) noexcept {
        if (!mHasSample.load(std::memory_order_relaxed)) return 0.0f;

        juce::ScopedLock sl(mSampleLock);
        const int numSamps = mSampleBuffer.getNumSamples();
        if (numSamps < 2) return 0.0f;

        const int chan = std::clamp(channel, 0, mSampleBuffer.getNumChannels() - 1);
        const float* readPtr = mSampleBuffer.getReadPointer(chan);

        if (mSampleSyncMode.load(std::memory_order_relaxed) == SampleSyncMode::TimelineSync) {
            // Modus A: Timeline Sync (Phasenstarr zum Playhead)
            const float clampedPhase = std::clamp(phase, 0.0f, 1.0f);
            const float fractionalPos = clampedPhase * static_cast<float>(numSamps - 1);
            const int idx0 = static_cast<int>(fractionalPos);
            const int idx1 = std::min(idx0 + 1, numSamps - 1);
            const float frac = fractionalPos - static_cast<float>(idx0);
            return (1.0f - frac) * readPtr[idx0] + frac * readPtr[idx1];
        } else {
            // Modus B: Classic Resample via fractional pointer
            float pos = mClassicPlaybackPointer.load(std::memory_order_relaxed);
            if (pos >= static_cast<float>(numSamps - 1)) {
                pos = std::fmod(pos, static_cast<float>(numSamps - 1));
            }

            const int idx0 = static_cast<int>(pos);
            const int idx1 = std::min(idx0 + 1, numSamps - 1);
            const float frac = pos - static_cast<float>(idx0);
            const float sampleVal = (1.0f - frac) * readPtr[idx0] + frac * readPtr[idx1];

            // Zeiger weiterschalten
            const float speed = midiPitchRatio * static_cast<float>(mSampleSampleRate / std::max(100.0, engineSampleRate));
            pos += speed;
            if (pos >= static_cast<float>(numSamps - 1)) {
                pos = std::fmod(pos, static_cast<float>(numSamps - 1));
            }
            mClassicPlaybackPointer.store(pos, std::memory_order_relaxed);

            return sampleVal;
        }
    }

    /**
     * Liest einen ganzen Block Audiosamples im Modus A (TimelineSync) oder Modus B (ClassicResample).
     * Thread-sicher und echtzeitoptimiert (ScopedLock nur 1x pro Block).
     */
    void readSampleBlock(float* dest, int numSamples, float startPhase, float endPhase,
                         float midiPitchRatio, double engineSampleRate, int channel = 0) noexcept {
        if (!dest || numSamples <= 0) return;

        if (!mHasSample.load(std::memory_order_acquire)) {
            std::fill_n(dest, numSamples, 0.0f);
            return;
        }

        juce::ScopedLock sl(mSampleLock);
        const int numSamps = mSampleBuffer.getNumSamples();
        if (numSamps < 2) {
            std::fill_n(dest, numSamples, 0.0f);
            return;
        }

        const int chan = std::clamp(channel, 0, mSampleBuffer.getNumChannels() - 1);
        const float* readPtr = mSampleBuffer.getReadPointer(chan);
        const auto syncMode = mSampleSyncMode.load(std::memory_order_relaxed);

        if (syncMode == SampleSyncMode::TimelineSync) {
            // Modus A: Timeline Sync (Phasenstarr zum Sequencer-Playhead)
            for (int i = 0; i < numSamples; ++i) {
                const float frac = (numSamples > 1) ? static_cast<float>(i) / static_cast<float>(numSamples - 1) : 0.0f;
                float phase = startPhase + frac * (endPhase - startPhase);
                if (phase < 0.0f) phase += 1.0f;
                if (phase >= 1.0f) phase -= std::floor(phase);

                const float fractionalPos = phase * static_cast<float>(numSamps - 1);
                const int idx0 = static_cast<int>(fractionalPos);
                const int idx1 = std::min(idx0 + 1, numSamps - 1);
                const float interp = fractionalPos - static_cast<float>(idx0);

                dest[i] = (1.0f - interp) * readPtr[idx0] + interp * readPtr[idx1];
            }
        } else {
            // Modus B: Classic Resample (Tonhöhenabhängig über Fractional Read Pointer)
            float pos = mClassicPlaybackPointer.load(std::memory_order_relaxed);
            const float speed = std::clamp(midiPitchRatio, 0.05f, 20.0f) *
                                static_cast<float>(mSampleSampleRate / std::max(100.0, engineSampleRate));

            for (int i = 0; i < numSamples; ++i) {
                while (pos >= static_cast<float>(numSamps - 1)) {
                    pos -= static_cast<float>(numSamps - 1);
                }
                while (pos < 0.0f) {
                    pos += static_cast<float>(numSamps - 1);
                }

                const int idx0 = static_cast<int>(pos);
                const int idx1 = std::min(idx0 + 1, numSamps - 1);
                const float interp = pos - static_cast<float>(idx0);

                dest[i] = (1.0f - interp) * readPtr[idx0] + interp * readPtr[idx1];

                pos += speed;
            }

            while (pos >= static_cast<float>(numSamps - 1)) {
                pos -= static_cast<float>(numSamps - 1);
            }
            mClassicPlaybackPointer.store(pos, std::memory_order_relaxed);
        }
    }

    // --- XML EXPORT & IMPORT ---

    std::unique_ptr<juce::XmlElement> exportXml() const {
        auto xml = std::make_unique<juce::XmlElement>("SEQUENCER");
        xml->setAttribute("stepCount", mStepCount.load(std::memory_order_relaxed));
        xml->setAttribute("gridSnap", mGridSnap.load(std::memory_order_relaxed));
        xml->setAttribute("sampleSyncMode", static_cast<int>(mSampleSyncMode.load(std::memory_order_relaxed)));

        for (size_t p = 0; p < kNumTrackedParams; ++p) {
            auto* cXml = xml->createNewChildElement("CURVE");
            cXml->setAttribute("id", kTrackedParams[p].id);
            cXml->setAttribute("automated", mAutomated[p].load(std::memory_order_relaxed));
            cXml->setAttribute("hasUserNodes", mHasUserNodes[p].load(std::memory_order_relaxed));
            cXml->setAttribute("manualVal", static_cast<double>(mManualValues[p].load(std::memory_order_relaxed)));

            for (const auto& node : mCurves[p]) {
                auto* nXml = cXml->createNewChildElement("NODE");
                nXml->setAttribute("x", static_cast<double>(node.x));
                nXml->setAttribute("y", static_cast<double>(node.y));
                nXml->setAttribute("curve", static_cast<int>(node.curve));
                nXml->setAttribute("tension", static_cast<double>(node.tension));
            }
        }
        return xml;
    }

    void importXml(const juce::XmlElement* xml) {
        if (!xml || !xml->hasTagName("SEQUENCER")) return;
        setStepCount(xml->getIntAttribute("stepCount", 16));
        setGridSnap(xml->getIntAttribute("gridSnap", 16));
        setSampleSyncMode(static_cast<SampleSyncMode>(xml->getIntAttribute("sampleSyncMode", 0)));

        for (auto* cXml : xml->getChildIterator()) {
            if (cXml->hasTagName("CURVE")) {
                const juce::String id = cXml->getStringAttribute("id");
                for (size_t p = 0; p < kNumTrackedParams; ++p) {
                    if (id == kTrackedParams[p].id) {
                        mAutomated[p].store(cXml->getBoolAttribute("automated", false), std::memory_order_relaxed);
                        mHasUserNodes[p].store(cXml->getBoolAttribute("hasUserNodes", false), std::memory_order_relaxed);
                        mManualValues[p].store(static_cast<float>(cXml->getDoubleAttribute("manualVal", 0.5)), std::memory_order_relaxed);

                        std::vector<SequencerNode> nodes;
                        for (auto* nXml : cXml->getChildIterator()) {
                            if (nXml->hasTagName("NODE")) {
                                const float nx = static_cast<float>(nXml->getDoubleAttribute("x", 0.0));
                                const float ny = static_cast<float>(nXml->getDoubleAttribute("y", 0.5));
                                const auto cType = static_cast<SegmentCurveType>(nXml->getIntAttribute("curve", 0));
                                const float tens = static_cast<float>(nXml->getDoubleAttribute("tension", 0.0));
                                nodes.push_back({nx, ny, cType, tens});
                            }
                        }
                        if (nodes.size() >= 2) {
                            mCurves[p] = nodes;
                            std::sort(mCurves[p].begin(), mCurves[p].end());
                            rebuildLookupTable(p);
                        }
                        break;
                    }
                }
            }
        }
    }

private:
    void rebuildLookupTable(size_t paramIdx) {
        if (paramIdx >= kNumTrackedParams) return;
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
                const float u = (dx > 0.00001f) ? (t - p0.x) / dx : 0.0f;
                table[i] = interpolateSegment(u, p0.y, p1.y, p0.curve, p0.tension);
            }
        }
    }

    std::atomic<int> mStepCount{16};
    std::atomic<int> mGridSnap{16};
    std::atomic<float> mCurrentPhase{0.0f};

    std::array<std::atomic<bool>, kNumTrackedParams> mAutomated;
    std::array<std::atomic<bool>, kNumTrackedParams> mHasUserNodes;
    std::array<std::atomic<float>, kNumTrackedParams> mManualValues;

    std::array<std::vector<SequencerNode>, kNumTrackedParams> mCurves;
    std::array<std::array<float, kTableSize>, kNumTrackedParams> mLookupTables;

    // Sample Drag-and-Drop & Resampling
    mutable juce::CriticalSection mSampleLock;
    juce::AudioBuffer<float> mSampleBuffer;
    double mSampleSampleRate = 44100.0;
    std::atomic<bool> mHasSample{false};
    std::atomic<bool> mIsTransportPlaying{false};
    std::atomic<SampleSyncMode> mSampleSyncMode{SampleSyncMode::TimelineSync};
    std::atomic<float> mClassicPlaybackPointer{0.0f};
};