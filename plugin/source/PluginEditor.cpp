#include "PluginProcessor.h"
#include "PluginEditor.h"

DDSPAudioProcessorEditor::DDSPAudioProcessorEditor(DDSPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
    };

    // Reihe 1: Master & Timbre
    setupSlider(mDryWetSlider, mDryWetLabel, "Dry / Wet");
    mDryWetAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "dry_wet", mDryWetSlider);

    setupSlider(mDetuneSlider, mDetuneLabel, "Ensemble");
    mDetuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "detune_cents", mDetuneSlider);

    setupSlider(mSpreadSlider, mSpreadLabel, "Stereo");
    mSpreadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "stereo_spread", mSpreadSlider);

    setupSlider(mTiltSlider, mTiltLabel, "Dyn Tilt");
    mTiltAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "spectral_tilt", mTiltSlider);

    setupSlider(mFormantSlider, mFormantLabel, "Formant");
    mFormantAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "formant_blend", mFormantSlider);

    // Reihe 2: Tracking & Dynamics
    setupSlider(mToleranceSlider, mToleranceLabel, "Tolerance");
    mToleranceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tracking_tolerance", mToleranceSlider);

    setupSlider(mTransientSlider, mTransientLabel, "Attack Track");
    mTransientAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "transient_track", mTransientSlider);

    setupSlider(mNoiseGainSlider, mNoiseGainLabel, "Noise/Breath");
    mNoiseGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "noise_gain", mNoiseGainSlider);

    // Reihe 3: Sub & High 1
    setupSlider(mSubGainSlider, mSubGainLabel, "Sub Level");
    mSubGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "sub_gain", mSubGainSlider);

    setupSlider(mSubOctSlider, mSubOctLabel, "Sub Oct");
    mSubOctAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "sub_octave", mSubOctSlider);

    setupSlider(mSubSemiSlider, mSubSemiLabel, "Sub Semi");
    mSubSemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "sub_semitones", mSubSemiSlider);

    setupSlider(mHigh1GainSlider, mHigh1GainLabel, "H1 Level");
    mHigh1GainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high_gain", mHigh1GainSlider);

    setupSlider(mHigh1OctSlider, mHigh1OctLabel, "H1 Oct");
    mHigh1OctAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high_octave", mHigh1OctSlider);

    setupSlider(mHigh1SemiSlider, mHigh1SemiLabel, "H1 Semi");
    mHigh1SemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high_semitones", mHigh1SemiSlider);

    // Reihe 4: High 2 & High 3
    setupSlider(mHigh2GainSlider, mHigh2GainLabel, "H2 Level");
    mHigh2GainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high2_gain", mHigh2GainSlider);

    setupSlider(mHigh2OctSlider, mHigh2OctLabel, "H2 Oct");
    mHigh2OctAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high2_octave", mHigh2OctSlider);

    setupSlider(mHigh2SemiSlider, mHigh2SemiLabel, "H2 Semi");
    mHigh2SemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high2_semitones", mHigh2SemiSlider);

    setupSlider(mHigh3GainSlider, mHigh3GainLabel, "H3 Level");
    mHigh3GainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high3_gain", mHigh3GainSlider);

    setupSlider(mHigh3OctSlider, mHigh3OctLabel, "H3 Oct");
    mHigh3OctAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high3_octave", mHigh3OctSlider);

    setupSlider(mHigh3SemiSlider, mHigh3SemiLabel, "H3 Semi");
    mHigh3SemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high3_semitones", mHigh3SemiSlider);

    setSize(760, 520);
}

void DDSPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff181a1d));

    g.setColour(juce::Colour(0xff22252b));
    g.fillRoundedRectangle(12.0f, 30.0f, static_cast<float>(getWidth() - 24), 112.0f, 6.0f);
    g.fillRoundedRectangle(12.0f, 150.0f, static_cast<float>(getWidth() - 24), 112.0f, 6.0f);
    g.fillRoundedRectangle(12.0f, 270.0f, static_cast<float>(getWidth() - 24), 112.0f, 6.0f);
    g.fillRoundedRectangle(12.0f, 390.0f, static_cast<float>(getWidth() - 24), 112.0f, 6.0f);

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("DDSP Neural Resynthesizer", getLocalBounds().removeFromTop(26), juce::Justification::centred, true);

    g.setFont(11.0f);
    g.setColour(juce::Colour(0xff8c939e));
    g.drawText("MASTER & TIMBRE", 20, 32, 200, 15, juce::Justification::left);
    g.drawText("TRACKING & DYNAMICS", 20, 152, 200, 15, juce::Justification::left);
    g.drawText("SUB & HIGH 1 VOICES", 20, 272, 200, 15, juce::Justification::left);
    g.drawText("HIGH 2 & HIGH 3 VOICES", 20, 392, 200, 15, juce::Justification::left);
}

void DDSPAudioProcessorEditor::resized() {
    const int marginX = 16;
    auto setupArea = [](juce::Rectangle<int>& row, int width, juce::Slider& slider, juce::Label& label) {
        auto area = row.removeFromLeft(width);
        label.setBounds(area.removeFromBottom(20));
        slider.setBounds(area);
    };

    // Reihe 1: 5 Regler
    const int knobWidth5 = (getWidth() - 2 * marginX) / 5;
    auto row1 = juce::Rectangle<int>(marginX, 44, getWidth() - 2 * marginX, 94);
    setupArea(row1, knobWidth5, mDryWetSlider, mDryWetLabel);
    setupArea(row1, knobWidth5, mDetuneSlider, mDetuneLabel);
    setupArea(row1, knobWidth5, mSpreadSlider, mSpreadLabel);
    setupArea(row1, knobWidth5, mTiltSlider, mTiltLabel);
    setupArea(row1, knobWidth5, mFormantSlider, mFormantLabel);

    // Reihe 2: 3 Regler
    const int knobWidth3 = (getWidth() - 2 * marginX) / 3;
    auto row2 = juce::Rectangle<int>(marginX, 164, getWidth() - 2 * marginX, 94);
    setupArea(row2, knobWidth3, mToleranceSlider, mToleranceLabel);
    setupArea(row2, knobWidth3, mTransientSlider, mTransientLabel);
    setupArea(row2, knobWidth3, mNoiseGainSlider, mNoiseGainLabel);

    // Reihe 3: 6 Regler
    const int knobWidth6 = (getWidth() - 2 * marginX) / 6;
    auto row3 = juce::Rectangle<int>(marginX, 284, getWidth() - 2 * marginX, 94);
    setupArea(row3, knobWidth6, mSubGainSlider, mSubGainLabel);
    setupArea(row3, knobWidth6, mSubOctSlider, mSubOctLabel);
    setupArea(row3, knobWidth6, mSubSemiSlider, mSubSemiLabel);
    setupArea(row3, knobWidth6, mHigh1GainSlider, mHigh1GainLabel);
    setupArea(row3, knobWidth6, mHigh1OctSlider, mHigh1OctLabel);
    setupArea(row3, knobWidth6, mHigh1SemiSlider, mHigh1SemiLabel);

    // Reihe 4: 6 Regler
    auto row4 = juce::Rectangle<int>(marginX, 404, getWidth() - 2 * marginX, 94);
    setupArea(row4, knobWidth6, mHigh2GainSlider, mHigh2GainLabel);
    setupArea(row4, knobWidth6, mHigh2OctSlider, mHigh2OctLabel);
    setupArea(row4, knobWidth6, mHigh2SemiSlider, mHigh2SemiLabel);
    setupArea(row4, knobWidth6, mHigh3GainSlider, mHigh3GainLabel);
    setupArea(row4, knobWidth6, mHigh3OctSlider, mHigh3OctLabel);
    setupArea(row4, knobWidth6, mHigh3SemiSlider, mHigh3SemiLabel);
}