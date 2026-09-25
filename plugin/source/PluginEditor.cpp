#include "PluginProcessor.h"
#include "PluginEditor.h"

DDSPAudioProcessorEditor::DDSPAudioProcessorEditor(DDSPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), mSeqCanvas(p.getSequencer())
{
    setMouseCursor(juce::MouseCursor::NormalCursor);

    auto setupRotary = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 48, 14);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffdbe6f6));
        slider.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions{11.0f});
        label.setColour(juce::Label::textColourId, juce::Colour(0xffb0b8c4));
        label.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(label);
    };

    auto setupCombo = [this](juce::ComboBox& combo, juce::Label& label, const juce::String& text) {
        combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff252932));
        combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3d4453));
        combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        combo.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(combo);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::FontOptions{11.0f});
        label.setColour(juce::Label::textColourId, juce::Colour(0xffb0b8c4));
        label.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(label);
    };

    // Sektion 1: Master & Timbre
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

    mLfoRateHzSlider.setCustomSuffix(" Hz");
    mLfoRateHzSlider.setFillColour(juce::Colour(0xffff4040));
    addAndMakeVisible(mLfoRateHzSlider);
    mLfoRateHzLabel.setText("Rate (Hz)", juce::dontSendNotification);
    mLfoRateHzLabel.setFont(juce::FontOptions{11.0f});
    mLfoRateHzLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb0b8c4));
    addAndMakeVisible(mLfoRateHzLabel);
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
    mHarmBalanceSlider.setFillColour(juce::Colour(0xff40c4ff));
    addAndMakeVisible(mHarmBalanceSlider);
    mHarmBalanceLabel.setText("Core / Harm", juce::dontSendNotification);
    mHarmBalanceLabel.setFont(juce::FontOptions{11.0f});
    mHarmBalanceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb0b8c4));
    addAndMakeVisible(mHarmBalanceLabel);
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

    // Mapping für Greyed-Out-Zustand (27 Parameter)
    mTrackedParamUIs[0]  = { &mDryWetSlider,          &mDryWetLabel };
    mTrackedParamUIs[1]  = { &mDetuneSlider,          &mDetuneLabel };
    mTrackedParamUIs[2]  = { &mSpreadSlider,          &mSpreadLabel };
    mTrackedParamUIs[3]  = { &mTiltSlider,            &mTiltLabel };
    mTrackedParamUIs[4]  = { &mFormantSlider,         &mFormantLabel };
    mTrackedParamUIs[5]  = { &mToleranceSlider,       &mToleranceLabel };
    mTrackedParamUIs[6]  = { &mTransientSlider,       &mTransientLabel };
    mTrackedParamUIs[7]  = { &mNoiseGainSlider,       &mNoiseGainLabel };
    mTrackedParamUIs[8]  = { &mLfoDepthSlider,        &mLfoDepthLabel };
    mTrackedParamUIs[9]  = { &mLfoRateHzSlider,       &mLfoRateHzLabel };
    mTrackedParamUIs[10] = { &mPitchQuantSlider,      &mPitchQuantLabel };
    mTrackedParamUIs[11] = { &mPitchInertiaSlider,    &mPitchInertiaLabel };
    mTrackedParamUIs[12] = { &mPitchInvertSlider,     &mPitchInvertLabel };
    mTrackedParamUIs[13] = { &mVoiceDriftSlider,      &mVoiceDriftLabel };
    mTrackedParamUIs[14] = { &mHarmBalanceSlider,     &mHarmBalanceLabel };
    mTrackedParamUIs[15] = { &mSubVoice.gainSlider,   &mSubVoice.gainLabel };
    mTrackedParamUIs[16] = { &mSubVoice.octSlider,    &mSubVoice.octLabel };
    mTrackedParamUIs[17] = { &mSubVoice.semiSlider,   &mSubVoice.semiLabel };
    mTrackedParamUIs[18] = { &mHigh1Voice.gainSlider, &mHigh1Voice.gainLabel };
    mTrackedParamUIs[19] = { &mHigh1Voice.octSlider,  &mHigh1Voice.octLabel };
    mTrackedParamUIs[20] = { &mHigh1Voice.semiSlider, &mHigh1Voice.semiLabel };
    mTrackedParamUIs[21] = { &mHigh2Voice.gainSlider, &mHigh2Voice.gainLabel };
    mTrackedParamUIs[22] = { &mHigh2Voice.octSlider,  &mHigh2Voice.octLabel };
    mTrackedParamUIs[23] = { &mHigh2Voice.semiSlider, &mHigh2Voice.semiLabel };
    mTrackedParamUIs[24] = { &mHigh3Voice.gainSlider, &mHigh3Voice.gainLabel };
    mTrackedParamUIs[25] = { &mHigh3Voice.octSlider,  &mHigh3Voice.octLabel };
    mTrackedParamUIs[26] = { &mHigh3Voice.semiSlider, &mHigh3Voice.semiLabel };

    // 27 Checkboxen initialisieren
    for (size_t i = 0; i < kNumTrackedParams; ++i) {
        auto& btn = mSeqAutoButtons[i];
        btn.setButtonText("SEQ");
        btn.setColour(juce::ToggleButton::textColourId, kTrackedParams[i].colour);
        btn.setColour(juce::ToggleButton::tickColourId, kTrackedParams[i].colour);
        btn.setMouseCursor(juce::MouseCursor::NormalCursor);
        btn.setToggleState(processorRef.getSequencer().isAutomated(i), juce::dontSendNotification);

        btn.onClick = [this, i]() {
            const bool isAuto = mSeqAutoButtons[i].getToggleState();
            processorRef.getSequencer().setAutomated(i, isAuto);
            if (isAuto) {
                mSeqCanvas.setParamVisible(i, true);
                mSeqCanvas.setSelectedParam(i);
                mCurveSelectCombo.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
                mCurveVisibleToggle.setToggleState(true, juce::dontSendNotification);
            }
            updateAutomationState();
            mSeqCanvas.repaint();
        };
        addAndMakeVisible(btn);
    }

    // Sektion 4: Sequencer Canvas, Viewport & Toolbar
    mSeqViewport.setViewedComponent(&mSeqCanvas, false);
    mSeqViewport.setScrollBarsShown(false, true, false, true);
    mSeqViewport.setMouseCursor(juce::MouseCursor::NormalCursor);
    addAndMakeVisible(mSeqViewport);

    mStepCountCombo.addItemList(juce::StringArray{"1 Step", "2 Steps", "4 Steps", "8 Steps", "16 Steps", "32 Steps"}, 1);
    mStepCountCombo.setSelectedId(5, juce::dontSendNotification);
    mStepCountCombo.onChange = [this]() {
        static const int stepMap[] = { 1, 2, 4, 8, 16, 32 };
        const int steps = stepMap[std::clamp(mStepCountCombo.getSelectedId() - 1, 0, 5)];
        processorRef.getSequencer().setStepCount(steps);
        mSeqCanvas.updateCanvasWidth(mSeqViewport.getWidth());
    };
    addAndMakeVisible(mStepCountCombo);

    mGridSnapCombo.addItemList(juce::StringArray{"Snap: Off", "Snap: 1/4", "Snap: 1/8", "Snap: 1/16", "Snap: 1/32"}, 1);
    mGridSnapCombo.setSelectedId(4, juce::dontSendNotification);
    mGridSnapCombo.onChange = [this]() {
        static const int snapMap[] = { 0, 4, 8, 16, 32 };
        mSeqCanvas.setGridSnap(snapMap[std::clamp(mGridSnapCombo.getSelectedId() - 1, 0, 4)]);
    };
    addAndMakeVisible(mGridSnapCombo);

    for (size_t i = 0; i < kNumTrackedParams; ++i) {
        mCurveSelectCombo.addItem(kTrackedParams[i].name, static_cast<int>(i + 1));
    }
    mCurveSelectCombo.setSelectedId(1, juce::dontSendNotification);
    mCurveSelectCombo.onChange = [this]() {
        const size_t idx = static_cast<size_t>(mCurveSelectCombo.getSelectedId() - 1);
        mSeqCanvas.setSelectedParam(idx);
        mCurveVisibleToggle.setToggleState(mSeqCanvas.isParamVisible(idx), juce::dontSendNotification);
    };
    addAndMakeVisible(mCurveSelectCombo);

    mCurveVisibleToggle.setButtonText("Show Curve");
    mCurveVisibleToggle.setToggleState(true, juce::dontSendNotification);
    mCurveVisibleToggle.setMouseCursor(juce::MouseCursor::NormalCursor);
    mCurveVisibleToggle.onClick = [this]() {
        const size_t idx = mSeqCanvas.getSelectedParam();
        mSeqCanvas.setParamVisible(idx, mCurveVisibleToggle.getToggleState());
    };
    addAndMakeVisible(mCurveVisibleToggle);

    // Fenster skalierbar machen
    setResizable(true, true);
    setResizeLimits(880, 600, 2560, 1600);
    setSize(1040, 880);

    updateLfoRateControls();
    updateAutomationState();
    startTimerHz(60);
}

DDSPAudioProcessorEditor::~DDSPAudioProcessorEditor() {
    stopTimer();
}

void DDSPAudioProcessorEditor::timerCallback() {
    mSeqCanvas.repaint();
}

void DDSPAudioProcessorEditor::updateLfoRateControls() {
    const bool isSync = (mLfoSyncCombo.getSelectedId() == 2);
    mLfoRateHzSlider.setEnabled(!isSync && !processorRef.getSequencer().isAutomated(9));
    mLfoRateHzSlider.setAlpha(isSync ? 0.35f : (processorRef.getSequencer().isAutomated(9) ? 0.35f : 1.0f));
    mLfoRateSyncCombo.setEnabled(isSync);
    mLfoRateSyncCombo.setAlpha(isSync ? 1.0f : 0.35f);
}

void DDSPAudioProcessorEditor::updateAutomationState() {
    for (size_t i = 0; i < kNumTrackedParams; ++i) {
        const bool isAuto = processorRef.getSequencer().isAutomated(i);
        mSeqAutoButtons[i].setToggleState(isAuto, juce::dontSendNotification);
        if (mTrackedParamUIs[i].slider) {
            mTrackedParamUIs[i].slider->setEnabled(!isAuto);
            mTrackedParamUIs[i].slider->setAlpha(isAuto ? 0.35f : 1.0f);
        }
        if (mTrackedParamUIs[i].label) {
            mTrackedParamUIs[i].label->setEnabled(!isAuto);
            mTrackedParamUIs[i].label->setAlpha(isAuto ? 0.35f : 1.0f);
        }
    }
}

void DDSPAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff121417));

    auto drawCard = [&g](const juce::Rectangle<int>& area, const juce::String& title) {
        if (area.isEmpty()) return;
        const auto fArea = area.toFloat();
        g.setColour(juce::Colour(0xff1a1d23));
        g.fillRoundedRectangle(fArea, 6.0f);
        g.setColour(juce::Colour(0xff2b303b));
        g.drawRoundedRectangle(fArea, 6.0f, 1.0f);

        g.setColour(juce::Colour(0xff7a8494));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, area.getX() + 14, area.getY() + 6, area.getWidth() - 28, 16, juce::Justification::left);
    };

    drawCard(mCard1Area, "1. MASTER, TIMBRE & LFO INTENSITY");
    drawCard(mCard2Area, "2. MONOPHONIC PITCH TRACKING & MODIFIERS");
    drawCard(mCard3Area, "3. ENSEMBLE HARMONY MATRIX & WARP MODES");
    drawCard(mCard4Area, "4. DAW-SYNCED STEP SEQUENCER & AUTOMATION MATRIX");

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("DDSP Neural Resynthesizer", 20, 7, 400, 20, juce::Justification::left);

    g.setFont(juce::FontOptions(12.0f));
    g.setColour(juce::Colour(0xff606a7a));
    g.drawText("Multi-Voice DDSP Engine", getWidth() - 220, 7, 200, 20, juce::Justification::right);
}

void DDSPAudioProcessorEditor::resized() {
    const int totalW = getWidth();
    const int totalH = getHeight();
    const int marginX = 14;

    // Dynamischer Umbruch: Bei geringer Höhe (< 840 px) rutschen Sektion 1 & 2 in eine gemeinsame Zeile
    const bool compactMode = (totalH < 840);

    auto setupAreaWithSeq = [this](juce::Rectangle<int>& area, int width, juce::Slider& slider, juce::Label& label, size_t paramIdx) {
        auto knobArea = area.removeFromLeft(width);
        if (paramIdx < kNumTrackedParams) {
            mSeqAutoButtons[paramIdx].setBounds(knobArea.getX() + (width - 40) / 2, knobArea.getBottom() - 14, 40, 14);
        }
        label.setBounds(knobArea.removeFromBottom(28).removeFromTop(14));
        slider.setBounds(knobArea);
    };

    int currentY = 30;

    if (!compactMode) {
        // --- NORMALER 4-ZEILEN-MODUS ---
        const int card1H = 155;
        mCard1Area = juce::Rectangle<int>(marginX, currentY, totalW - 2 * marginX, card1H);
        currentY += card1H + 10;

        const int card2H = 110;
        mCard2Area = juce::Rectangle<int>(marginX, currentY, totalW - 2 * marginX, card2H);
        currentY += card2H + 10;

        // Sektion 1 Layout (Full Width)
        auto topRow = juce::Rectangle<int>(mCard1Area.getX() + 8, mCard1Area.getY() + 24, mCard1Area.getWidth() * 62 / 100, 122);
        const int topKnobW = topRow.getWidth() / 8;
        setupAreaWithSeq(topRow, topKnobW, mDryWetSlider, mDryWetLabel, 0);
        setupAreaWithSeq(topRow, topKnobW, mDetuneSlider, mDetuneLabel, 1);
        setupAreaWithSeq(topRow, topKnobW, mSpreadSlider, mSpreadLabel, 2);
        setupAreaWithSeq(topRow, topKnobW, mTiltSlider, mTiltLabel, 3);
        setupAreaWithSeq(topRow, topKnobW, mFormantSlider, mFormantLabel, 4);
        setupAreaWithSeq(topRow, topKnobW, mToleranceSlider, mToleranceLabel, 5);
        setupAreaWithSeq(topRow, topKnobW, mTransientSlider, mTransientLabel, 6);
        setupAreaWithSeq(topRow, topKnobW, mNoiseGainSlider, mNoiseGainLabel, 7);

        // LFO Block rechts in Card 1
        auto lfoArea = juce::Rectangle<int>(mCard1Area.getX() + mCard1Area.getWidth() * 63 / 100, mCard1Area.getY() + 24, mCard1Area.getWidth() * 36 / 100, 122);
        auto lfoDepthArea = lfoArea.removeFromLeft(65);
        setupAreaWithSeq(lfoDepthArea, 65, mLfoDepthSlider, mLfoDepthLabel, 8);

        const int comboW = lfoArea.getWidth() - 85;
        mLfoWaveLabel.setBounds(lfoArea.getX() + 8, lfoArea.getY() + 2, 70, 18);
        mLfoWaveCombo.setBounds(lfoArea.getX() + 80, lfoArea.getY() + 2, comboW, 20);

        mLfoSyncLabel.setBounds(lfoArea.getX() + 8, lfoArea.getY() + 28, 70, 18);
        mLfoSyncCombo.setBounds(lfoArea.getX() + 80, lfoArea.getY() + 28, comboW, 20);

        mLfoRateSyncLabel.setBounds(lfoArea.getX() + 8, lfoArea.getY() + 54, 70, 18);
        mLfoRateSyncCombo.setBounds(lfoArea.getX() + 80, lfoArea.getY() + 54, comboW, 20);

        mLfoRateHzLabel.setBounds(lfoArea.getX() + 8, lfoArea.getY() + 80, 70, 20);
        mLfoRateHzSlider.setBounds(lfoArea.getX() + 80, lfoArea.getY() + 80, comboW - 46, 20);
        mSeqAutoButtons[9].setBounds(lfoArea.getX() + 80 + comboW - 42, lfoArea.getY() + 83, 40, 14);

        // Sektion 2 Layout (Full Width)
        auto midRow = juce::Rectangle<int>(mCard2Area.getX() + 10, mCard2Area.getY() + 24, mCard2Area.getWidth() - 20, 80);
        const int midKnobW = midRow.getWidth() / 5;
        setupAreaWithSeq(midRow, midKnobW, mPitchQuantSlider, mPitchQuantLabel, 10);
        setupAreaWithSeq(midRow, midKnobW, mPitchInertiaSlider, mPitchInertiaLabel, 11);

        auto freezeArea = midRow.removeFromLeft(midKnobW);
        mPitchFreezeLabel.setBounds(freezeArea.getX() + (midKnobW - 100) / 2, freezeArea.getY() + 12, 100, 16);
        mPitchFreezeCombo.setBounds(freezeArea.getX() + (midKnobW - 100) / 2, freezeArea.getY() + 32, 100, 22);

        setupAreaWithSeq(midRow, midKnobW, mPitchInvertSlider, mPitchInvertLabel, 12);
        setupAreaWithSeq(midRow, midKnobW, mVoiceDriftSlider, mVoiceDriftLabel, 13);
    } else {
        // --- KOMPAKTER MODUS: Sektion 1 & 2 teilen sich eine Zeile ---
        const int row1H = 175;
        const int card1W = (totalW - 2 * marginX) * 58 / 100;
        const int card2W = (totalW - 2 * marginX) - card1W - 10;

        mCard1Area = juce::Rectangle<int>(marginX, currentY, card1W, row1H);
        mCard2Area = juce::Rectangle<int>(marginX + card1W + 10, currentY, card2W, row1H);
        currentY += row1H + 10;

        // Card 1 kompakt (2 Zeilen à 4 Drehregler links + LFO rechts)
        const int timbreW = mCard1Area.getWidth() - 170;
        auto rowA = juce::Rectangle<int>(mCard1Area.getX() + 6, mCard1Area.getY() + 22, timbreW, 70);
        auto rowB = juce::Rectangle<int>(mCard1Area.getX() + 6, mCard1Area.getY() + 94, timbreW, 70);
        const int subKnobW = timbreW / 4;

        setupAreaWithSeq(rowA, subKnobW, mDryWetSlider, mDryWetLabel, 0);
        setupAreaWithSeq(rowA, subKnobW, mDetuneSlider, mDetuneLabel, 1);
        setupAreaWithSeq(rowA, subKnobW, mSpreadSlider, mSpreadLabel, 2);
        setupAreaWithSeq(rowA, subKnobW, mTiltSlider, mTiltLabel, 3);

        setupAreaWithSeq(rowB, subKnobW, mFormantSlider, mFormantLabel, 4);
        setupAreaWithSeq(rowB, subKnobW, mToleranceSlider, mToleranceLabel, 5);
        setupAreaWithSeq(rowB, subKnobW, mTransientSlider, mTransientLabel, 6);
        setupAreaWithSeq(rowB, subKnobW, mNoiseGainSlider, mNoiseGainLabel, 7);

        // LFO Block rechts in kompakter Card 1
        const int lfoX = mCard1Area.getX() + timbreW + 6;
        auto lfoDepthArea = juce::Rectangle<int>(lfoX, mCard1Area.getY() + 22, 55, 75);
        setupAreaWithSeq(lfoDepthArea, 55, mLfoDepthSlider, mLfoDepthLabel, 8);

        mLfoWaveLabel.setBounds(lfoX + 60, mCard1Area.getY() + 24, 40, 16);
        mLfoWaveCombo.setBounds(lfoX + 98, mCard1Area.getY() + 22, 60, 20);

        mLfoSyncLabel.setBounds(lfoX + 60, mCard1Area.getY() + 48, 40, 16);
        mLfoSyncCombo.setBounds(lfoX + 98, mCard1Area.getY() + 46, 60, 20);

        mLfoRateSyncLabel.setBounds(lfoX + 60, mCard1Area.getY() + 72, 40, 16);
        mLfoRateSyncCombo.setBounds(lfoX + 98, mCard1Area.getY() + 70, 60, 20);

        mLfoRateHzLabel.setBounds(lfoX, mCard1Area.getY() + 105, 55, 18);
        mLfoRateHzSlider.setBounds(lfoX + 60, mCard1Area.getY() + 105, 55, 20);
        mSeqAutoButtons[9].setBounds(lfoX + 120, mCard1Area.getY() + 108, 38, 14);

        // Card 2 kompakt (5 Regler verteilt)
        auto midRowA = juce::Rectangle<int>(mCard2Area.getX() + 8, mCard2Area.getY() + 24, mCard2Area.getWidth() - 16, 70);
        auto midRowB = juce::Rectangle<int>(mCard2Area.getX() + 8, mCard2Area.getY() + 96, mCard2Area.getWidth() - 16, 70);
        const int halfW2 = (mCard2Area.getWidth() - 16) / 3;

        setupAreaWithSeq(midRowA, halfW2, mPitchQuantSlider, mPitchQuantLabel, 10);
        setupAreaWithSeq(midRowA, halfW2, mPitchInertiaSlider, mPitchInertiaLabel, 11);

        auto freezeArea = midRowA.removeFromLeft(halfW2);
        mPitchFreezeLabel.setBounds(freezeArea.getX() + 4, freezeArea.getY() + 6, halfW2 - 8, 16);
        mPitchFreezeCombo.setBounds(freezeArea.getX() + 4, freezeArea.getY() + 26, halfW2 - 8, 22);

        const int halfW3 = (mCard2Area.getWidth() - 16) / 2;
        setupAreaWithSeq(midRowB, halfW3, mPitchInvertSlider, mPitchInvertLabel, 12);
        setupAreaWithSeq(midRowB, halfW3, mVoiceDriftSlider, mVoiceDriftLabel, 13);
    }

    // --- SEKTION 3: HARMONY MATRIX ---
    const int card3H = 265;
    mCard3Area = juce::Rectangle<int>(marginX, currentY, totalW - 2 * marginX, card3H);
    currentY += card3H + 10;

    mHarmBalanceLabel.setBounds(mCard3Area.getX() + 15, mCard3Area.getY() + 26, 75, 20);
    mHarmBalanceSlider.setBounds(mCard3Area.getX() + 95, mCard3Area.getY() + 26, 85, 20);
    mSeqAutoButtons[14].setBounds(mCard3Area.getX() + 185, mCard3Area.getY() + 29, 40, 14);

    mMixModeLabel.setBounds(mCard3Area.getX() + 245, mCard3Area.getY() + 26, 70, 20);
    mMixModeCombo.setBounds(mCard3Area.getX() + 320, mCard3Area.getY() + 26, 150, 20);

    const int colW = (mCard3Area.getWidth() - 20) / 4;
    auto layoutVoiceCol = [&](VoiceControls& v, int colIndex, size_t gainIdx, size_t octIdx, size_t semiIdx) {
        const int colX = mCard3Area.getX() + 10 + colIndex * colW;
        auto colArea = juce::Rectangle<int>(colX, mCard3Area.getY() + 52, colW, 205);

        auto gainArea = colArea.removeFromTop(82);
        setupAreaWithSeq(gainArea, colW, v.gainSlider, v.gainLabel, gainIdx);

        auto octSemiArea = colArea.removeFromTop(78);
        const int halfW = colW / 2;

        auto octArea = octSemiArea.removeFromLeft(halfW);
        setupAreaWithSeq(octArea, halfW, v.octSlider, v.octLabel, octIdx);

        auto semiArea = octSemiArea;
        setupAreaWithSeq(semiArea, halfW, v.semiSlider, v.semiLabel, semiIdx);

        v.waveLabel.setBounds(colX + 15, colArea.getY() + 4, colW - 30, 16);
        v.waveCombo.setBounds(colX + 15, colArea.getY() + 22, colW - 30, 20);
    };

    layoutVoiceCol(mSubVoice,   0, 15, 16, 17);
    layoutVoiceCol(mHigh1Voice, 1, 18, 19, 20);
    layoutVoiceCol(mHigh2Voice, 2, 21, 22, 23);
    layoutVoiceCol(mHigh3Voice, 3, 24, 25, 26);

    // --- SEKTION 4: STEP SEQUENCER & TOOLBAR (Nimmt den gesamten restlichen Raum ein) ---
    const int card4H = std::max(170, totalH - currentY - 10);
    mCard4Area = juce::Rectangle<int>(marginX, currentY, totalW - 2 * marginX, card4H);

    mStepCountCombo.setBounds(mCard4Area.getX() + 12, mCard4Area.getY() + 26, 105, 22);
    mGridSnapCombo.setBounds(mCard4Area.getX() + 125, mCard4Area.getY() + 26, 105, 22);
    mCurveSelectCombo.setBounds(mCard4Area.getX() + 238, mCard4Area.getY() + 26, 150, 22);
    mCurveVisibleToggle.setBounds(mCard4Area.getX() + 396, mCard4Area.getY() + 26, 100, 22);

    const int viewH = std::max(110, card4H - 58);
    mSeqViewport.setBounds(mCard4Area.getX() + 12, mCard4Area.getY() + 52, mCard4Area.getWidth() - 24, viewH);

    mSeqCanvas.updateCanvasWidth(mSeqViewport.getWidth());
}