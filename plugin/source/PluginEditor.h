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

    juce::Slider mToleranceSlider;
    juce::Label  mToleranceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mToleranceAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DDSPAudioProcessorEditor)
};
