#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class DDSPAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit DDSPAudioProcessorEditor(DDSPAudioProcessor&);
    ~DDSPAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DDSPAudioProcessor& processorRef;

    // Sektion 1: Master, LFO & Timbre
    juce::Slider mDryWetSlider;
    juce::Label  mDryWetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mDryWetAttach;

    juce::Slider mDetuneSlider;
    juce::Label  mDetuneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mDetuneAttach;

    juce::Slider mSpreadSlider;
    juce::Label  mSpreadLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSpreadAttach;

    juce::Slider mTiltSlider;
    juce::Label  mTiltLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTiltAttach;

    juce::Slider mFormantSlider;
    juce::Label  mFormantLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mFormantAttach;

    juce::Slider mToleranceSlider;
    juce::Label  mToleranceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mToleranceAttach;

    juce::Slider mTransientSlider;
    juce::Label  mTransientLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTransientAttach;

    juce::Slider mNoiseGainSlider;
    juce::Label  mNoiseGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mNoiseGainAttach;

    juce::Slider mLfoDepthSlider;
    juce::Label  mLfoDepthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mLfoDepthAttach;

    juce::ComboBox mLfoWaveCombo;
    juce::Label    mLfoWaveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mLfoWaveAttach;

    juce::ComboBox mLfoSyncCombo;
    juce::Label    mLfoSyncLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mLfoSyncAttach;

    juce::Slider   mLfoRateHzSlider;
    juce::Label    mLfoRateHzLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mLfoRateHzAttach;

    juce::ComboBox mLfoRateSyncCombo;
    juce::Label    mLfoRateSyncLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mLfoRateSyncAttach;

    // Sektion 2: Monophonic Pitch Modifiers
    juce::Slider mPitchQuantSlider;
    juce::Label  mPitchQuantLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mPitchQuantAttach;

    juce::Slider mPitchInertiaSlider;
    juce::Label  mPitchInertiaLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mPitchInertiaAttach;

    juce::ComboBox mPitchFreezeCombo;
    juce::Label    mPitchFreezeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mPitchFreezeAttach;

    juce::Slider mPitchInvertSlider;
    juce::Label  mPitchInvertLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mPitchInvertAttach;

    juce::Slider mVoiceDriftSlider;
    juce::Label  mVoiceDriftLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mVoiceDriftAttach;

    // Sektion 3: Harmony Matrix & Warp Modes
    juce::Slider mHarmBalanceSlider;
    juce::Label  mHarmBalanceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHarmBalanceAttach;

    juce::ComboBox mMixModeCombo;
    juce::Label    mMixModeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mMixModeAttach;

    struct VoiceControls {
        juce::Slider   gainSlider;
        juce::Label    gainLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;

        juce::Slider   octSlider;
        juce::Label    octLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octAttach;

        juce::Slider   semiSlider;
        juce::Label    semiLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> semiAttach;

        juce::ComboBox waveCombo;
        juce::Label    waveLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    };

    VoiceControls mSubVoice;
    VoiceControls mHigh1Voice;
    VoiceControls mHigh2Voice;
    VoiceControls mHigh3Voice;

    void updateLfoRateControls();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DDSPAudioProcessorEditor)
};