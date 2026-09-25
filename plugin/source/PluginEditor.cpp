#include "PluginProcessor.h"
#include "PluginEditor.h"

DDSPAudioProcessorEditor::DDSPAudioProcessorEditor(DDSPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    auto setupRotary = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffdbe6f6));
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions{11.0f});
        label.setColour(juce::Label::textColourId, juce::Colour(0xffb0b8c4));
        addAndMakeVisible(label);
    };

    auto setupCombo = [this](juce::ComboBox& combo, juce::Label& label, const juce::String& text) {
        combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252932));
        combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3d4453));
        combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        addAndMakeVisible(combo);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::FontOptions{11.0f});
        label.setColour(juce::Label::textColourId, juce::Colour(0xffb0b8c4));
        addAndMakeVisible(label);
    };

    // Sektion 1: Master, LFO & Timbre
    setupRotary(mDryWetSlider, mDryWetLabel, "Dry / Wet");
    mDryWetAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "dry_wet", mDryWetSlider);

    setupRotary(mDetuneSlider, mDetuneLabel, "Ensemble");
    mDetuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "detune_cents", mDetuneSlider);

    setupRotary(mSpreadSlider, mSpreadLabel, "Stereo");
    mSpreadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "stereo_spread", mSpreadSlider);

    setupRotary(mTiltSlider, mTiltLabel, "Dyn Tilt");
    mTiltAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "spectral_tilt", mTiltSlider);

    setupRotary(mFormantSlider, mFormantLabel, "Formant");
    mFormantAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "formant_blend", mFormantSlider);

    setupRotary(mToleranceSlider, mToleranceLabel, "Tolerance");
    mToleranceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "tracking_tolerance", mToleranceSlider);

    setupRotary(mTransientSlider, mTransientLabel, "Attack Track");
    mTransientAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "transient_track", mTransientSlider);

    setupRotary(mNoiseGainSlider, mNoiseGainLabel, "Breath/Noise");
    mNoiseGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "noise_gain", mNoiseGainSlider);

    setupRotary(mLfoDepthSlider, mLfoDepthLabel, "LFO Depth");
    mLfoDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "lfo_depth", mLfoDepthSlider);

    mLfoWaveCombo.addItemList(juce::StringArray{"Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H"}, 1);
    setupCombo(mLfoWaveCombo, mLfoWaveLabel, "LFO Wave");
    mLfoWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "lfo_wave", mLfoWaveCombo);

    mLfoSyncCombo.addItemList(juce::StringArray{"Free (Hz)", "Tempo Sync"}, 1);
    setupCombo(mLfoSyncCombo, mLfoSyncLabel, "LFO Mode");
    mLfoSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "lfo_sync_mode", mLfoSyncCombo);

    setupRotary(mLfoRateHzSlider, mLfoRateHzLabel, "Rate (Hz)");
    mLfoRateHzAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "lfo_rate_hz", mLfoRateHzSlider);

    mLfoRateSyncCombo.addItemList(juce::StringArray{"1/32", "1/16", "1/8", "1/4", "1/2", "1/1", "2/1", "4/1", "1/8T", "1/4T"}, 1);
    setupCombo(mLfoRateSyncCombo, mLfoRateSyncLabel, "Rate (Beats)");
    mLfoRateSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "lfo_rate_sync", mLfoRateSyncCombo);

    mLfoSyncCombo.onChange = [this]() { updateLfoRateControls(); };

    // Sektion 2: Monophonic Pitch Modifiers
    setupRotary(mPitchQuantSlider, mPitchQuantLabel, "Quantize");
    mPitchQuantAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pitch_quantize", mPitchQuantSlider);

    setupRotary(mPitchInertiaSlider, mPitchInertiaLabel, "Inertia (ms)");
    mPitchInertiaAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pitch_inertia", mPitchInertiaSlider);

    mPitchFreezeCombo.addItemList(juce::StringArray{"Tracking", "Freeze"}, 1);
    setupCombo(mPitchFreezeCombo, mPitchFreezeLabel, "Hold Mode");
    mPitchFreezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "pitch_freeze", mPitchFreezeCombo);

    setupRotary(mPitchInvertSlider, mPitchInvertLabel, "Inversion");
    mPitchInvertAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pitch_inversion", mPitchInvertSlider);

    setupRotary(mVoiceDriftSlider, mVoiceDriftLabel, "Voice Drift");
    mVoiceDriftAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "voice_drift", mVoiceDriftSlider);

    // Sektion 3: Harmony Matrix & Warp Modes
    setupRotary(mHarmBalanceSlider, mHarmBalanceLabel, "Core / Harm");
    mHarmBalanceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "harmony_balance", mHarmBalanceSlider);

    mMixModeCombo.addItemList(juce::StringArray{"Add", "Ring Mod", "Phase Mod (FM)"}, 1);
    setupCombo(mMixModeCombo, mMixModeLabel, "Warp Mode");
    mMixModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "mix_mode", mMixModeCombo);

    juce::StringArray waveChoices{"Sine", "Saw", "Square", "Triangle"};

    auto setupVoice = [&](VoiceControls& v, const juce::String& prefix, const juce::String& tag) {
        setupRotary(v.gainSlider, v.gainLabel, tag + " Level");
        v.gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "_gain", v.gainSlider);

        setupRotary(v.octSlider, v.octLabel, tag + " Oct");
        v.octAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "_octave", v.octSlider);

        setupRotary(v.semiSlider, v.semiLabel, tag + " Semi");
        v.semiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "_semitones", v.semiSlider);

        v.waveCombo.addItemList(waveChoices, 1);
        setupCombo(v.waveCombo, v.waveLabel, tag + " Wave");
        v.waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processorRef.apvts, prefix + (prefix == "high" ? "1_wave" : "_wave"), v.waveCombo);
    };

    setupVoice(mSubVoice, "sub", "Sub");
    setupVoice(mHigh1Voice, "high", "High 1");
    setupVoice(mHigh2Voice, "high2", "High 2");
    setupVoice(mHigh3Voice, "high3", "High 3");

    setSize(920, 680);
    updateLfoRateControls();
}

void DDSPAudioProcessorEditor::updateLfoRateControls() {
    const bool isSync = (mLfoSyncCombo.getSelectedId() == 2);
    mLfoRateHzSlider.setEnabled(!isSync);
    mLfoRateHzSlider.setAlpha(isSync ? 0.35f : 1.0f);
    mLfoRateSyncCombo.setEnabled(isSync);
    mLfoRateSyncCombo.setAlpha(isSync ? 1.0f : 0.35f);
}

void DDSPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff121417));

    auto drawCard = [&g](float x, float y, float w, float h, const juce::String& title) {
        g.setColour(juce::Colour(0xff1a1d23));
        g.fillRoundedRectangle(x, y, w, h, 6.0f);

        g.setColour(juce::Colour(0xff2b303b));
        g.drawRoundedRectangle(x, y, w, h, 6.0f, 1.0f);

        g.setColour(juce::Colour(0xff7a8494));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, static_cast<int>(x + 14), static_cast<int>(y + 8), 350, 16, juce::Justification::left);
    };

    drawCard(12.0f, 30.0f, 896.0f, 160.0f, "1. MASTER, TIMBRE & LFO INTENSITY");
    drawCard(12.0f, 200.0f, 896.0f, 110.0f, "2. MONOPHONIC PITCH TRACKING & MODIFIERS");
    drawCard(12.0f, 320.0f, 896.0f, 348.0f, "3. ENSEMBLE HARMONY MATRIX & WARP MODES");

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("DDSP Neural Resynthesizer", 20, 7, 400, 20, juce::Justification::left);

    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colour(0xff606a7a));
    g.drawText("Multi-Voice DDSP Engine", getWidth() - 220, 7, 200, 20, juce::Justification::right);
}

void DDSPAudioProcessorEditor::resized() {
    auto setupArea = [](juce::Rectangle<int>& area, int width, juce::Slider& slider, juce::Label& label) {
        auto knobArea = area.removeFromLeft(width);
        label.setBounds(knobArea.removeFromBottom(18));
        slider.setBounds(knobArea);
    };

    // 1. Master & Timbre
    auto topRow = juce::Rectangle<int>(20, 52, 580, 125);
    const int topKnobW = topRow.getWidth() / 8;
    setupArea(topRow, topKnobW, mDryWetSlider, mDryWetLabel);
    setupArea(topRow, topKnobW, mDetuneSlider, mDetuneLabel);
    setupArea(topRow, topKnobW, mSpreadSlider, mSpreadLabel);
    setupArea(topRow, topKnobW, mTiltSlider, mTiltLabel);
    setupArea(topRow, topKnobW, mFormantSlider, mFormantLabel);
    setupArea(topRow, topKnobW, mToleranceSlider, mToleranceLabel);
    setupArea(topRow, topKnobW, mTransientSlider, mTransientLabel);
    setupArea(topRow, topKnobW, mNoiseGainSlider, mNoiseGainLabel);

    // LFO-Block
    auto lfoDepthArea = juce::Rectangle<int>(610, 52, 70, 125);
    setupArea(lfoDepthArea, 70, mLfoDepthSlider, mLfoDepthLabel);

    mLfoWaveLabel.setBounds(690, 54, 60, 20);
    mLfoWaveCombo.setBounds(755, 54, 135, 22);

    mLfoSyncLabel.setBounds(690, 84, 60, 20);
    mLfoSyncCombo.setBounds(755, 84, 135, 22);

    mLfoRateSyncLabel.setBounds(690, 114, 60, 20);
    mLfoRateSyncCombo.setBounds(755, 114, 135, 22);

    auto lfoRateArea = juce::Rectangle<int>(690, 138, 200, 40);
    mLfoRateHzLabel.setBounds(690, 140, 60, 20);
    mLfoRateHzSlider.setBounds(755, 140, 135, 22);

    // 2. Pitch Modifiers
    auto midRow = juce::Rectangle<int>(20, 222, 880, 80);
    const int midKnobW = 140;
    setupArea(midRow, midKnobW, mPitchQuantSlider, mPitchQuantLabel);
    setupArea(midRow, midKnobW, mPitchInertiaSlider, mPitchInertiaLabel);

    auto freezeArea = midRow.removeFromLeft(140);
    mPitchFreezeLabel.setBounds(freezeArea.getX(), freezeArea.getY() + 15, 130, 18);
    mPitchFreezeCombo.setBounds(freezeArea.getX(), freezeArea.getY() + 35, 130, 24);

    setupArea(midRow, midKnobW, mPitchInvertSlider, mPitchInvertLabel);
    setupArea(midRow, midKnobW, mVoiceDriftSlider, mVoiceDriftLabel);

    // 3. Harmony Matrix Header (Warp Modes)
    auto warpArea = juce::Rectangle<int>(20, 345, 880, 50);
    auto balArea = warpArea.removeFromLeft(120);
    setupArea(balArea, 120, mHarmBalanceSlider, mHarmBalanceLabel);

    mMixModeLabel.setBounds(150, 355, 70, 22);
    mMixModeCombo.setBounds(225, 355, 160, 24);

    // Voices Columns
    const int colW = 880 / 4;
    auto layoutVoiceCol = [&](VoiceControls& v, int colIndex) {
        const int colX = 20 + colIndex * colW;
        auto colArea = juce::Rectangle<int>(colX, 410, colW, 245);

        auto gainArea = colArea.removeFromTop(85);
        setupArea(gainArea, colW, v.gainSlider, v.gainLabel);

        auto octSemiArea = colArea.removeFromTop(85);
        const int halfW = colW / 2;
        setupArea(octSemiArea, halfW, v.octSlider, v.octLabel);
        setupArea(octSemiArea, halfW, v.semiSlider, v.semiLabel);

        v.waveLabel.setBounds(colX + 15, colArea.getY() + 10, colW - 30, 18);
        v.waveCombo.setBounds(colX + 15, colArea.getY() + 30, colW - 30, 24);
    };

    layoutVoiceCol(mSubVoice, 0);
    layoutVoiceCol(mHigh1Voice, 1);
    layoutVoiceCol(mHigh2Voice, 2);
    layoutVoiceCol(mHigh3Voice, 3);
}