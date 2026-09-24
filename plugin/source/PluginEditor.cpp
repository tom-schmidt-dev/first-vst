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

    // Reihe 1: Master & Ensemble
    setupSlider(mDryWetSlider, mDryWetLabel, "Dry / Wet");
    mDryWetAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "dry_wet", mDryWetSlider);

    setupSlider(mDetuneSlider, mDetuneLabel, "Ensemble");
    mDetuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "detune_cents", mDetuneSlider);

    setupSlider(mSpreadSlider, mSpreadLabel, "Stereo");
    mSpreadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "stereo_spread", mSpreadSlider);

    setupSlider(mSubGainSlider, mSubGainLabel, "Sub Level");
    mSubGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "sub_gain", mSubGainSlider);

    setupSlider(mHighGainSlider, mHighGainLabel, "High Level");
    mHighGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high_gain", mHighGainSlider);

    // Reihe 2: Voice Pitch Tuning
    setupSlider(mSubOctSlider, mSubOctLabel, "Sub Oct");
    mSubOctAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "sub_octave", mSubOctSlider);

    setupSlider(mSubSemiSlider, mSubSemiLabel, "Sub Semi");
    mSubSemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "sub_semitones", mSubSemiSlider);

    setupSlider(mHighOctSlider, mHighOctLabel, "High Oct");
    mHighOctAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high_octave", mHighOctSlider);

    setupSlider(mHighSemiSlider, mHighSemiLabel, "High Semi");
    mHighSemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "high_semitones", mHighSemiSlider);

    // Reihe 3: Tracking & Timbre
    setupSlider(mToleranceSlider, mToleranceLabel, "Tolerance");
    mToleranceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tracking_tolerance", mToleranceSlider);

    setupSlider(mFormantSlider, mFormantLabel, "Formant");
    mFormantAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "formant_blend", mFormantSlider);

    setupSlider(mTiltSlider, mTiltLabel, "Dyn Tilt");
    mTiltAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "spectral_tilt", mTiltSlider);

    setupSlider(mTransientSlider, mTransientLabel, "Attack Track");
    mTransientAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "transient_track", mTransientSlider);

    setupSlider(mNoiseGainSlider, mNoiseGainLabel, "Noise / Breath");
    mNoiseGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "noise_gain", mNoiseGainSlider);

    setSize(720, 430);
}

void DDSPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1c1e));

    // Rack-Sektionen
    g.setColour(juce::Colour(0xff25282e));
    g.fillRoundedRectangle(12.0f, 32.0f, static_cast<float>(getWidth() - 24), 118.0f, 6.0f);
    g.fillRoundedRectangle(12.0f, 160.0f, static_cast<float>(getWidth() - 24), 118.0f, 6.0f);
    g.fillRoundedRectangle(12.0f, 288.0f, static_cast<float>(getWidth() - 24), 118.0f, 6.0f);

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("DDSP Neural Resynthesizer", getLocalBounds().removeFromTop(28), juce::Justification::centred, true);

    g.setFont(11.0f);
    g.setColour(juce::Colour(0xff8c939e));
    g.drawText("SYNTHESIS & STEREO", 20, 34, 200, 15, juce::Justification::left);
    g.drawText("VOICE PITCH SHIFT", 20, 162, 200, 15, juce::Justification::left);
    g.drawText("VOCAL & TIMBRE TRACKING", 20, 290, 200, 15, juce::Justification::left);
}

void DDSPAudioProcessorEditor::resized() {
    const int marginX = 16;
    auto setupArea = [](juce::Rectangle<int>& row, int width, juce::Slider& slider, juce::Label& label) {
        auto area = row.removeFromLeft(width);
        label.setBounds(area.removeFromBottom(22));
        slider.setBounds(area);
    };

    // Reihe 1: 5 Regler
    const int knobWidth5 = (getWidth() - 2 * marginX) / 5;
    auto row1 = juce::Rectangle<int>(marginX, 46, getWidth() - 2 * marginX, 100);
    setupArea(row1, knobWidth5, mDryWetSlider, mDryWetLabel);
    setupArea(row1, knobWidth5, mDetuneSlider, mDetuneLabel);
    setupArea(row1, knobWidth5, mSpreadSlider, mSpreadLabel);
    setupArea(row1, knobWidth5, mSubGainSlider, mSubGainLabel);
    setupArea(row1, knobWidth5, mHighGainSlider, mHighGainLabel);

    // Reihe 2: 4 Regler zentriert
    const int knobWidth4 = (getWidth() - 2 * marginX) / 4;
    auto row2 = juce::Rectangle<int>(marginX, 174, getWidth() - 2 * marginX, 100);
    setupArea(row2, knobWidth4, mSubOctSlider, mSubOctLabel);
    setupArea(row2, knobWidth4, mSubSemiSlider, mSubSemiLabel);
    setupArea(row2, knobWidth4, mHighOctSlider, mHighOctLabel);
    setupArea(row2, knobWidth4, mHighSemiSlider, mHighSemiLabel);

    // Reihe 3: 5 Regler
    auto row3 = juce::Rectangle<int>(marginX, 302, getWidth() - 2 * marginX, 100);
    setupArea(row3, knobWidth5, mToleranceSlider, mToleranceLabel);
    setupArea(row3, knobWidth5, mFormantSlider, mFormantLabel);
    setupArea(row3, knobWidth5, mTiltSlider, mTiltLabel);
    setupArea(row3, knobWidth5, mTransientSlider, mTransientLabel);
    setupArea(row3, knobWidth5, mNoiseGainSlider, mNoiseGainLabel);
}