#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../dsp/include/SequencerEngine.hpp"
#include <vector>
#include <unordered_map>
#include <string>

struct FactoryPreset {
    const char* name;
    std::unordered_map<std::string, float> params;
    std::vector<SequencerNode> volumeCurve;
};

class PresetManager {
public:
    PresetManager() {
        initFactoryPresets();
    }

    int getNumPresets() const noexcept {
        return static_cast<int>(mPresets.size());
    }

    juce::String getPresetName(int index) const {
        if (index >= 0 && index < static_cast<int>(mPresets.size())) {
            return mPresets[static_cast<size_t>(index)].name;
        }
        return {};
    }

    void applyPreset(int index, juce::AudioProcessorValueTreeState& apvts, SequencerEngine& seq) {
        if (index < 0 || index >= static_cast<int>(mPresets.size())) return;

        const auto& p = mPresets[static_cast<size_t>(index)];

        for (const auto& [paramId, val] : p.params) {
            if (auto* param = apvts.getParameter(paramId)) {
                param->setValueNotifyingHost(param->convertTo0to1(val));
            }
        }

        // Falls das Preset eine eigene Volume-Hüllkurve mitbringt
        if (p.volumeCurve.size() >= 2) {
            seq.setNodes(0, p.volumeCurve);
        } else {
            seq.clearUserNodes(0);
        }
    }

    std::unique_ptr<juce::XmlElement> exportUserPresetXml(const juce::String& name,
                                                          juce::AudioProcessorValueTreeState& apvts,
                                                          const SequencerEngine& seq) {
        auto xml = std::make_unique<juce::XmlElement>("USER_PRESET");
        xml->setAttribute("name", name);

        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> apvtsXml(state.createXml());
        if (apvtsXml) xml->addChildElement(apvtsXml.release());

        auto seqXml = seq.exportXml();
        if (seqXml) xml->addChildElement(seqXml.release());

        return xml;
    }

    bool loadUserPresetXml(const juce::XmlElement* xml,
                           juce::AudioProcessorValueTreeState& apvts,
                           SequencerEngine& seq) {
        if (!xml || !xml->hasTagName("USER_PRESET")) return false;

        if (auto* apvtsXml = xml->getChildByName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*apvtsXml));
        }

        if (auto* seqXml = xml->getChildByName("SEQUENCER")) {
            seq.importXml(seqXml);
        }

        return true;
    }

private:
    void initFactoryPresets() {
        mPresets.clear();

        // 1. Pure Timbre Transfer (Original Neural Violin Resynthesis)
        {
            FactoryPreset p;
            p.name = "Pure Timbre Transfer";
            p.params = {
                {"dry_wet", 1.00f}, {"spectral_tilt", 0.50f}, {"transient_track", 0.50f}, {"noise_gain", 0.15f},
                {"formant_blend", 0.00f}, {"sample_sync_mode", 1.0f}, {"synth_mode", 0.0f},
                {"pitch_quantize", 0.0f}, {"pitch_inertia", 0.0f}, {"pitch_freeze", 0.0f}, {"pitch_inversion", 0.0f}, {"voice_drift", 0.0f},
                {"v1_gain", 1.0f}, {"v1_octave", 0.0f}, {"v1_semitones", 0.0f}, {"v1_cents", 0.0f}, {"v1_source", 0.0f}, {"v1_pan", 0.0f},
                {"v2_gain", 0.0f}, {"v2_octave", 0.0f}, {"v2_semitones", 0.0f}, {"v2_cents", 0.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.0f},
                {"v3_gain", 0.0f}, {"v3_octave", 0.0f}, {"v3_semitones", 0.0f}, {"v3_cents", 0.0f}, {"v3_source", 1.0f}, {"v3_pan", 0.0f},
                {"v4_gain", 0.0f}, {"v4_octave", 0.0f}, {"v4_semitones", 0.0f}, {"v4_cents", 0.0f}, {"v4_source", 1.0f}, {"v4_pan", 0.0f},
                {"v5_gain", 0.0f}, {"v5_octave", 0.0f}, {"v5_semitones", 0.0f}, {"v5_cents", 0.0f}, {"v5_source", 1.0f}, {"v5_pan", 0.0f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Linear, 0.0f}, {1.0f, 1.0f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 2. Neuro Reese Bass
        {
            FactoryPreset p;
            p.name = "Neuro Reese Bass";
            p.params = {
                {"dry_wet", 0.85f}, {"spectral_tilt", 0.35f}, {"transient_track", 0.70f}, {"noise_gain", 0.15f},
                {"v1_gain", 1.0f}, {"v1_octave", -1.0f}, {"v1_semitones", 0.0f}, {"v1_cents", -12.0f}, {"v1_source", 2.0f}, {"v1_pan", -0.4f},
                {"v2_gain", 1.0f}, {"v2_octave", -1.0f}, {"v2_semitones", 0.0f}, {"v2_cents", 12.0f}, {"v2_source", 2.0f}, {"v2_pan", 0.4f},
                {"v3_gain", 0.8f}, {"v3_octave", -2.0f}, {"v3_semitones", 0.0f}, {"v3_cents", 0.0f}, {"v3_source", 1.0f}, {"v3_pan", 0.0f},
                {"v4_gain", 0.0f}, {"v5_gain", 0.0f},
                {"m_1_2_mode", 2.0f}, {"m_1_2_amt", 0.40f} // RingMod V1 -> V2
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.9f, SegmentCurveType::Exponential, 0.3f} };
            mPresets.push_back(p);
        }

        // 2. Vocal Formant Lead
        {
            FactoryPreset p;
            p.name = "Vocal Formant Lead";
            p.params = {
                {"dry_wet", 0.75f}, {"spectral_tilt", 0.65f}, {"formant_blend", 0.90f}, {"voice_drift", 15.0f},
                {"v1_gain", 1.0f}, {"v1_octave", 0.0f}, {"v1_source", 0.0f}, {"v1_pan", 0.0f}, // DDSP
                {"v2_gain", 0.4f}, {"v2_octave", 1.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.3f},
                {"v3_gain", 0.0f}, {"v4_gain", 0.0f}, {"v5_gain", 0.0f}
            };
            p.volumeCurve = { {0.0f, 0.0f, SegmentCurveType::SCurve, 0.0f}, {0.1f, 1.0f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.8f, SegmentCurveType::Exponential, 0.2f} };
            mPresets.push_back(p);
        }

        // 3. Sub-Harmonic Drone
        {
            FactoryPreset p;
            p.name = "Sub-Harmonic Drone";
            p.params = {
                {"dry_wet", 0.90f}, {"spectral_tilt", 0.20f}, {"pitch_inertia", 400.0f},
                {"v1_gain", 0.8f}, {"v1_octave", -2.0f}, {"v1_source", 1.0f}, {"v1_pan", 0.0f},
                {"v2_gain", 0.9f}, {"v2_octave", -3.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.0f},
                {"v3_gain", 0.3f}, {"v3_octave", -1.0f}, {"v3_source", 3.0f}, {"v3_pan", 0.0f},
                {"v4_gain", 0.0f}, {"v5_gain", 0.0f}
            };
            mPresets.push_back(p);
        }

        // 4. Cyberpunk FM Pad
        {
            FactoryPreset p;
            p.name = "Cyberpunk FM Pad";
            p.params = {
                {"dry_wet", 0.80f}, {"spectral_tilt", 0.55f}, {"pitch_inertia", 200.0f},
                {"v1_gain", 0.8f}, {"v1_octave", 0.0f}, {"v1_source", 1.0f}, {"v1_pan", -0.5f},
                {"v2_gain", 0.7f}, {"v2_octave", 0.0f}, {"v2_source", 2.0f}, {"v2_pan", 0.5f},
                {"v3_gain", 0.5f}, {"v3_octave", 1.0f}, {"v3_source", 1.0f}, {"v3_pan", 0.0f},
                {"m_2_1_mode", 3.0f}, {"m_2_1_amt", 0.75f}, // PhaseMod V2 -> V1
                {"m_3_2_mode", 3.0f}, {"m_3_2_amt", 0.50f}
            };
            p.volumeCurve = { {0.0f, 0.0f, SegmentCurveType::SCurve, 0.0f}, {0.3f, 1.0f, SegmentCurveType::Linear, 0.0f}, {0.8f, 0.9f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.0f, SegmentCurveType::Exponential, 0.3f} };
            mPresets.push_back(p);
        }

        // 5. Bell Texture
        {
            FactoryPreset p;
            p.name = "Bell Texture";
            p.params = {
                {"dry_wet", 0.70f}, {"spectral_tilt", 0.80f},
                {"v1_gain", 0.8f}, {"v1_octave", 1.0f}, {"v1_source", 1.0f}, {"v1_pan", -0.3f},
                {"v2_gain", 0.6f}, {"v2_octave", 2.0f}, {"v2_semitones", 7.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.3f},
                {"m_2_1_mode", 2.0f}, {"m_2_1_amt", 0.65f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Exponential, 0.6f}, {1.0f, 0.0f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 6. Dirty Saw Stab
        {
            FactoryPreset p;
            p.name = "Dirty Saw Stab";
            p.params = {
                {"dry_wet", 0.90f}, {"spectral_tilt", 0.70f}, {"transient_track", 0.95f},
                {"v1_gain", 1.0f}, {"v1_octave", 0.0f}, {"v1_source", 2.0f}, {"v1_pan", -0.4f},
                {"v2_gain", 0.9f}, {"v2_octave", 0.0f}, {"v2_cents", 18.0f}, {"v2_source", 2.0f}, {"v2_pan", 0.4f},
                {"v3_gain", 0.7f}, {"v3_octave", -1.0f}, {"v3_source", 3.0f}, {"v3_pan", 0.0f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Exponential, 0.5f}, {0.35f, 0.0f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.0f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 7. Violin Hybrid Solo
        {
            FactoryPreset p;
            p.name = "Violin Hybrid Solo";
            p.params = {
                {"dry_wet", 0.80f}, {"spectral_tilt", 0.50f}, {"formant_blend", 0.85f},
                {"v1_gain", 1.0f}, {"v1_octave", 0.0f}, {"v1_source", 0.0f}, {"v1_pan", 0.0f},
                {"v2_gain", 0.4f}, {"v2_octave", -1.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.0f},
                {"v3_gain", 0.3f}, {"v3_octave", 1.0f}, {"v3_source", 2.0f}, {"v3_pan", 0.4f}
            };
            mPresets.push_back(p);
        }

        // 8. Screaming Formant Sync
        {
            FactoryPreset p;
            p.name = "Screaming Formant Sync";
            p.params = {
                {"dry_wet", 0.85f}, {"spectral_tilt", 0.90f}, {"pitch_inversion", 0.40f},
                {"v1_gain", 1.0f}, {"v1_octave", 0.0f}, {"v1_source", 2.0f}, {"v1_pan", 0.0f},
                {"m_1_1_mode", 3.0f}, {"m_1_1_amt", 0.85f} // Selbst-PM
            };
            mPresets.push_back(p);
        }

        // 9. 808 Sub Boom
        {
            FactoryPreset p;
            p.name = "808 Sub Boom";
            p.params = {
                {"dry_wet", 1.0f}, {"spectral_tilt", 0.15f}, {"transient_track", 0.90f},
                {"v1_gain", 1.0f}, {"v1_octave", -2.0f}, {"v1_source", 1.0f}, {"v1_pan", 0.0f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Exponential, 0.4f}, {0.8f, 0.0f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.0f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 10. Industrial RingMod FX
        {
            FactoryPreset p;
            p.name = "Industrial RingMod FX";
            p.params = {
                {"dry_wet", 0.85f}, {"pitch_quantize", 1.0f},
                {"v1_gain", 0.9f}, {"v1_octave", 0.0f}, {"v1_source", 3.0f}, {"v1_pan", -0.5f},
                {"v2_gain", 0.9f}, {"v2_octave", 0.0f}, {"v2_semitones", 6.0f}, {"v2_source", 3.0f}, {"v2_pan", 0.5f},
                {"m_1_2_mode", 2.0f}, {"m_1_2_amt", 0.90f},
                {"m_2_1_mode", 2.0f}, {"m_2_1_amt", 0.90f}
            };
            mPresets.push_back(p);
        }

        // 11. Ambient Choir Pad
        {
            FactoryPreset p;
            p.name = "Ambient Choir Pad";
            p.params = {
                {"dry_wet", 0.80f}, {"spectral_tilt", 0.45f}, {"pitch_inertia", 350.0f},
                {"v1_gain", 0.7f}, {"v1_octave", 0.0f}, {"v1_source", 0.0f}, {"v1_pan", 0.0f},
                {"v2_gain", 0.6f}, {"v2_octave", -1.0f}, {"v2_source", 1.0f}, {"v2_pan", -0.4f},
                {"v3_gain", 0.5f}, {"v3_octave", 0.0f}, {"v3_semitones", 7.0f}, {"v3_source", 4.0f}, {"v3_pan", 0.4f},
                {"v4_gain", 0.4f}, {"v4_octave", 1.0f}, {"v4_source", 1.0f}, {"v4_pan", -0.7f},
                {"v5_gain", 0.3f}, {"v5_octave", 1.0f}, {"v5_semitones", 4.0f}, {"v5_source", 1.0f}, {"v5_pan", 0.7f}
            };
            p.volumeCurve = { {0.0f, 0.0f, SegmentCurveType::SCurve, 0.0f}, {0.35f, 1.0f, SegmentCurveType::Linear, 0.0f}, {0.85f, 0.9f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.0f, SegmentCurveType::SCurve, 0.0f} };
            mPresets.push_back(p);
        }

        // 12. Phase Mod Pluck
        {
            FactoryPreset p;
            p.name = "Phase Mod Pluck";
            p.params = {
                {"dry_wet", 0.75f}, {"spectral_tilt", 0.60f},
                {"v1_gain", 1.0f}, {"v1_octave", 0.0f}, {"v1_source", 1.0f}, {"v1_pan", 0.0f},
                {"v2_gain", 0.8f}, {"v2_octave", 1.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.0f},
                {"m_2_1_mode", 3.0f}, {"m_2_1_amt", 0.85f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Exponential, 0.7f}, {0.4f, 0.0f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.0f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 13. Glitch Step Resonator
        {
            FactoryPreset p;
            p.name = "Glitch Step Resonator";
            p.params = {
                {"dry_wet", 0.90f}, {"noise_gain", 0.40f}, {"pitch_quantize", 1.0f},
                {"v1_gain", 0.9f}, {"v1_octave", 0.0f}, {"v1_source", 3.0f}, {"v1_pan", -0.6f},
                {"v2_gain", 0.8f}, {"v2_octave", 1.0f}, {"v2_source", 2.0f}, {"v2_pan", 0.6f},
                {"m_1_2_mode", 1.0f}, {"m_1_2_amt", 0.50f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Stepped, 0.0f}, {0.25f, 0.2f, SegmentCurveType::Stepped, 0.0f}, {0.5f, 1.0f, SegmentCurveType::Stepped, 0.0f}, {0.75f, 0.4f, SegmentCurveType::Stepped, 0.0f}, {1.0f, 1.0f, SegmentCurveType::Stepped, 0.0f} };
            mPresets.push_back(p);
        }

        // 14. Dark Ambient Drone
        {
            FactoryPreset p;
            p.name = "Dark Ambient Drone";
            p.params = {
                {"dry_wet", 0.85f}, {"spectral_tilt", 0.25f}, {"pitch_freeze", 1.0f},
                {"v1_gain", 0.9f}, {"v1_octave", -1.0f}, {"v1_source", 0.0f}, {"v1_pan", -0.3f},
                {"v2_gain", 0.7f}, {"v2_octave", -2.0f}, {"v2_source", 4.0f}, {"v2_pan", 0.3f}
            };
            mPresets.push_back(p);
        }

        // 15. Retro Synthwave Lead
        {
            FactoryPreset p;
            p.name = "Retro Synthwave Lead";
            p.params = {
                {"dry_wet", 0.80f}, {"spectral_tilt", 0.65f}, {"transient_track", 0.60f},
                {"v1_gain", 0.9f}, {"v1_octave", 0.0f}, {"v1_source", 2.0f}, {"v1_pan", -0.4f},
                {"v2_gain", 0.9f}, {"v2_octave", 0.0f}, {"v2_cents", 10.0f}, {"v2_source", 3.0f}, {"v2_pan", 0.4f},
                {"v3_gain", 0.5f}, {"v3_octave", -1.0f}, {"v3_source", 1.0f}, {"v3_pan", 0.0f}
            };
            mPresets.push_back(p);
        }

        // 16. Cosmic FM Chime
        {
            FactoryPreset p;
            p.name = "Cosmic FM Chime";
            p.params = {
                {"dry_wet", 0.75f}, {"spectral_tilt", 0.85f},
                {"v1_gain", 0.7f}, {"v1_octave", 1.0f}, {"v1_source", 1.0f}, {"v1_pan", -0.6f},
                {"v2_gain", 0.6f}, {"v2_octave", 2.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.6f},
                {"v3_gain", 0.5f}, {"v3_octave", 2.0f}, {"v3_semitones", 7.0f}, {"v3_source", 1.0f}, {"v3_pan", 0.0f},
                {"m_3_1_mode", 3.0f}, {"m_3_1_amt", 0.60f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Exponential, 0.5f}, {1.0f, 0.0f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 17. Heavy Detune Swarm
        {
            FactoryPreset p;
            p.name = "Heavy Detune Swarm";
            p.params = {
                {"dry_wet", 0.85f}, {"spectral_tilt", 0.55f},
                {"v1_gain", 0.8f}, {"v1_octave", 0.0f}, {"v1_cents", 0.0f},   {"v1_source", 2.0f}, {"v1_pan", 0.0f},
                {"v2_gain", 0.7f}, {"v2_octave", 0.0f}, {"v2_cents", -15.0f}, {"v2_source", 2.0f}, {"v2_pan", -0.5f},
                {"v3_gain", 0.7f}, {"v3_octave", 0.0f}, {"v3_cents", 15.0f},  {"v3_source", 2.0f}, {"v3_pan", 0.5f},
                {"v4_gain", 0.6f}, {"v4_octave", 0.0f}, {"v4_cents", -30.0f}, {"v4_source", 2.0f}, {"v4_pan", -0.9f},
                {"v5_gain", 0.6f}, {"v5_octave", 0.0f}, {"v5_cents", 30.0f},  {"v5_source", 2.0f}, {"v5_pan", 0.9f}
            };
            mPresets.push_back(p);
        }

        // 18. Vocal Morph Bass
        {
            FactoryPreset p;
            p.name = "Vocal Morph Bass";
            p.params = {
                {"dry_wet", 0.80f}, {"spectral_tilt", 0.40f}, {"formant_blend", 0.95f},
                {"v1_gain", 1.0f}, {"v1_octave", -1.0f}, {"v1_source", 0.0f}, {"v1_pan", 0.0f},
                {"v2_gain", 0.8f}, {"v2_octave", -1.0f}, {"v2_source", 2.0f}, {"v2_pan", 0.3f},
                {"v3_gain", 0.7f}, {"v3_octave", -2.0f}, {"v3_source", 1.0f}, {"v3_pan", -0.3f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::SCurve, 0.0f}, {0.5f, 0.5f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.9f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 19. Crystal Glass Bell
        {
            FactoryPreset p;
            p.name = "Crystal Glass Bell";
            p.params = {
                {"dry_wet", 0.70f}, {"spectral_tilt", 0.80f},
                {"v1_gain", 0.8f}, {"v1_octave", 2.0f}, {"v1_source", 4.0f}, {"v1_pan", -0.4f},
                {"v2_gain", 0.7f}, {"v2_octave", 2.0f}, {"v2_semitones", 11.0f}, {"v2_source", 1.0f}, {"v2_pan", 0.4f},
                {"m_2_1_mode", 2.0f}, {"m_2_1_amt", 0.55f}
            };
            p.volumeCurve = { {0.0f, 1.0f, SegmentCurveType::Exponential, 0.8f}, {0.6f, 0.0f, SegmentCurveType::Linear, 0.0f}, {1.0f, 0.0f, SegmentCurveType::Linear, 0.0f} };
            mPresets.push_back(p);
        }

        // 20. Distorted Reese Monster
        {
            FactoryPreset p;
            p.name = "Distorted Reese Monster";
            p.params = {
                {"dry_wet", 0.95f}, {"spectral_tilt", 0.45f}, {"transient_track", 0.85f},
                {"v1_gain", 1.0f}, {"v1_octave", -1.0f}, {"v1_source", 2.0f}, {"v1_pan", -0.6f},
                {"v2_gain", 1.0f}, {"v2_octave", -1.0f}, {"v2_cents", 20.0f}, {"v2_source", 2.0f}, {"v2_pan", 0.6f},
                {"v3_gain", 0.9f}, {"v3_octave", -2.0f}, {"v3_source", 3.0f}, {"v3_pan", 0.0f},
                {"m_1_1_mode", 3.0f}, {"m_1_1_amt", 0.70f},
                {"m_2_3_mode", 2.0f}, {"m_2_3_amt", 0.60f}
            };
            mPresets.push_back(p);
        }
    }

    std::vector<FactoryPreset> mPresets;
};
