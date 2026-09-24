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

    juce::Slider mSubGainSlider;
    juce::Label  mSubGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSubGainAttach;

    juce::Slider mHighGainSlider;
    juce::Label  mHighGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHighGainAttach;

    // Reihe 2: Voice Pitch Tuning
    juce::Slider mSubOctSlider;
    juce::Label  mSubOctLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSubOctAttach;

    juce::Slider mSubSemiSlider;
    juce::Label  mSubSemiLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSubSemiAttach;

    juce::Slider mHighOctSlider;
    juce::Label  mHighOctLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHighOctAttach;

    juce::Slider mHighSemiSlider;
    juce::Label  mHighSemiLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHighSemiAttach;

    // Reihe 3: Tracking & Timbre
    juce::Slider mToleranceSlider;
    juce::Label  mToleranceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mToleranceAttach;

    juce::Slider mFormantSlider;
    juce::Label  mFormantLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mFormantAttach;

    juce::Slider mTiltSlider;
    juce::Label  mTiltLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTiltAttach;

    juce::Slider mTransientSlider;
    juce::Label  mTransientLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTransientAttach;

    juce::Slider mNoiseGainSlider;
    juce::Label  mNoiseGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mNoiseGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DDSPAudioProcessorEditor)
};