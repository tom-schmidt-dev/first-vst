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

    // Reihe 1: Master & Ensemble
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

    // Reihe 2: Tracking & Dynamics
    juce::Slider mToleranceSlider;
    juce::Label  mToleranceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mToleranceAttach;

    juce::Slider mTransientSlider;
    juce::Label  mTransientLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTransientAttach;

    juce::Slider mNoiseGainSlider;
    juce::Label  mNoiseGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mNoiseGainAttach;

    // Reihe 3: Sub & High 1
    juce::Slider mSubGainSlider;
    juce::Label  mSubGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSubGainAttach;

    juce::Slider mSubOctSlider;
    juce::Label  mSubOctLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSubOctAttach;

    juce::Slider mSubSemiSlider;
    juce::Label  mSubSemiLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSubSemiAttach;

    juce::Slider mHigh1GainSlider;
    juce::Label  mHigh1GainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh1GainAttach;

    juce::Slider mHigh1OctSlider;
    juce::Label  mHigh1OctLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh1OctAttach;

    juce::Slider mHigh1SemiSlider;
    juce::Label  mHigh1SemiLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh1SemiAttach;

    // Reihe 4: High 2 & High 3
    juce::Slider mHigh2GainSlider;
    juce::Label  mHigh2GainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh2GainAttach;

    juce::Slider mHigh2OctSlider;
    juce::Label  mHigh2OctLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh2OctAttach;

    juce::Slider mHigh2SemiSlider;
    juce::Label  mHigh2SemiLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh2SemiAttach;

    juce::Slider mHigh3GainSlider;
    juce::Label  mHigh3GainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh3GainAttach;

    juce::Slider mHigh3OctSlider;
    juce::Label  mHigh3OctLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh3OctAttach;

    juce::Slider mHigh3SemiSlider;
    juce::Label  mHigh3SemiLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHigh3SemiAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DDSPAudioProcessorEditor)
};