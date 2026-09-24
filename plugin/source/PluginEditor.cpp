#include "PluginProcessor.h"
#include "PluginEditor.h"

DDSPAudioProcessorEditor::DDSPAudioProcessorEditor(DDSPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    };

    setupSlider(mDryWetSlider, mDryWetLabel, "Dry / Wet");
    mDryWetAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "dry_wet", mDryWetSlider);

    setupSlider(mDetuneSlider, mDetuneLabel, "Detune (Cent)");
    mDetuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "detune_cents", mDetuneSlider);

    setupSlider(mSpreadSlider, mSpreadLabel, "Stereo Spread");
    mSpreadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "stereo_spread", mSpreadSlider);

    setupSlider(mSubGainSlider, mSubGainLabel, "Sub Octave");
    mSubGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "sub_gain", mSubGainSlider);

    setupSlider(mHighGainSlider, mHighGainLabel, "High Octave");
    mHighGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high_gain", mHighGainSlider);

    setupSlider(mToleranceSlider, mToleranceLabel, "Tolerance");
    mToleranceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tracking_tolerance", mToleranceSlider);

    setSize(740, 180);

}

void DDSPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff222428));

    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("DDSP Timbre Transfer - Violin Ensemble", getLocalBounds().removeFromTop(30), juce::Justification::centred, true);
}

void DDSPAudioProcessorEditor::resized() {
    auto bounds = getLocalBounds().reduced(15);
    bounds.removeFromTop(25);

    const int knobWidth = bounds.getWidth() / 6;

    auto setupArea = [&bounds, knobWidth](juce::Slider& slider, juce::Label& label) {
        auto area = bounds.removeFromLeft(knobWidth);
        label.setBounds(area.removeFromBottom(25));
        slider.setBounds(area);
    };

    setupArea(mDryWetSlider, mDryWetLabel);
    setupArea(mDetuneSlider, mDetuneLabel);
    setupArea(mSpreadSlider, mSpreadLabel);
    setupArea(mSubGainSlider, mSubGainLabel);
    setupArea(mHighGainSlider, mHighGainLabel);
    setupArea(mToleranceSlider, mToleranceLabel);
}
