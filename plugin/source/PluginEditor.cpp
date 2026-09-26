#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

DDSPAudioProcessorEditor::DDSPAudioProcessorEditor(DDSPAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), mSeqCanvas(p.getSequencer())
{
    setLookAndFeel(&mFlatLaf);
    setMouseCursor(juce::MouseCursor::NormalCursor);

    // Lambda für Rotary-Slider im modernen Flat-Design
    auto setupRotary = [this](juce::Slider& slider, juce::Label& label, const juce::String& text, juce::Colour accentColour) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 48, 14);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd0d7de));
        slider.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions{10.5f, juce::Font::bold});
        label.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        label.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(label);
    };

    // Lambda für ComboBoxen
    auto setupCombo = [this](juce::ComboBox& combo, juce::Label& label, const juce::String& text) {
        combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161920));
        combo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a313d));
        combo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
        combo.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(combo);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::FontOptions{10.0f, juce::Font::bold});
        label.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        label.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(label);
    };

    // --- Header & Preset-Bar ---
    mTitleLabel.setText("DDSP TIMBRE TRANSDUCER", juce::dontSendNotification);
    mTitleLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    mTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));
    mTitleLabel.setMouseCursor(juce::MouseCursor::NormalCursor);
    addAndMakeVisible(mTitleLabel);

    for (int i = 0; i < processorRef.getNumPrograms(); ++i) {
        mPresetCombo.addItem(processorRef.getProgramName(i), i + 1);
    }
    mPresetCombo.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    mPresetCombo.setMouseCursor(juce::MouseCursor::NormalCursor);
    mPresetCombo.onChange = [this]() {
        const int idx = mPresetCombo.getSelectedId() - 1;
        if (idx >= 0 && idx < processorRef.getNumPrograms()) {
            processorRef.setCurrentProgram(idx);
            updateAutomationState();
            mSeqCanvas.repaint();
        }
    };
    addAndMakeVisible(mPresetCombo);

    mPrevPresetBtn.setMouseCursor(juce::MouseCursor::NormalCursor);
    mPrevPresetBtn.onClick = [this]() {
        int cur = processorRef.getCurrentProgram();
        int num = processorRef.getNumPrograms();
        if (num > 0) {
            int prev = (cur - 1 + num) % num;
            processorRef.setCurrentProgram(prev);
            mPresetCombo.setSelectedId(prev + 1, juce::dontSendNotification);
            updateAutomationState();
            mSeqCanvas.repaint();
        }
    };
    addAndMakeVisible(mPrevPresetBtn);

    mNextPresetBtn.setMouseCursor(juce::MouseCursor::NormalCursor);
    mNextPresetBtn.onClick = [this]() {
        int cur = processorRef.getCurrentProgram();
        int num = processorRef.getNumPrograms();
        if (num > 0) {
            int next = (cur + 1) % num;
            processorRef.setCurrentProgram(next);
            mPresetCombo.setSelectedId(next + 1, juce::dontSendNotification);
            updateAutomationState();
            mSeqCanvas.repaint();
        }
    };
    addAndMakeVisible(mNextPresetBtn);

    mSavePresetBtn.setMouseCursor(juce::MouseCursor::NormalCursor);
    mSavePresetBtn.onClick = [this]() {
        mFileChooser = std::make_unique<juce::FileChooser>(
            "Save Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
        auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;
        mFileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File{}) {
                auto xml = processorRef.getPresetManager().exportUserPresetXml(file.getFileNameWithoutExtension(), processorRef.apvts, processorRef.getSequencer());
                if (xml) xml->writeTo(file);
            }
        });
    };
    addAndMakeVisible(mSavePresetBtn);

    mLoadPresetBtn.setMouseCursor(juce::MouseCursor::NormalCursor);
    mLoadPresetBtn.onClick = [this]() {
        mFileChooser = std::make_unique<juce::FileChooser>(
            "Load Preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.xml");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        mFileChooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file.existsAsFile()) {
                auto xml = juce::XmlDocument::parse(file);
                if (xml) {
                    processorRef.getPresetManager().loadUserPresetXml(xml.get(), processorRef.apvts, processorRef.getSequencer());
                    updateAutomationState();
                    mSeqCanvas.repaint();
                }
            }
        });
    };
    addAndMakeVisible(mLoadPresetBtn);

    // --- Sektion 1: Master, Timbre & Engine Mode ---
    setupRotary(mDryWetSlider, mDryWetLabel, "Dry / Wet", juce::Colour(0xff4a90e2));
    mDryWetAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "dry_wet", mDryWetSlider);

    setupRotary(mTiltSlider, mTiltLabel, "Dyn Tilt", juce::Colour(0xfff5a623));
    mTiltAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "spectral_tilt", mTiltSlider);

    setupRotary(mFormantSlider, mFormantLabel, "Formant", juce::Colour(0xffff007f));
    mFormantAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "formant_blend", mFormantSlider);

    setupRotary(mTransientSlider, mTransientLabel, "Attack Track", juce::Colour(0xff9013fe));
    mTransientAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "transient_track", mTransientSlider);

    setupRotary(mNoiseGainSlider, mNoiseGainLabel, "Breath/Noise", juce::Colour(0xff7ed321));
    mNoiseGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "noise_gain", mNoiseGainSlider);

    mSynthModeCombo.addItemList(juce::StringArray{"Auto (MIDI/Audio)", "Force MIDI Synth", "Force Audio FX"}, 1);
    setupCombo(mSynthModeCombo, mSynthModeLabel, "Engine Mode");
    mSynthModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "synth_mode", mSynthModeCombo);

    mSampleSyncCombo.addItemList(juce::StringArray{"Timeline Sync", "Classic Resample"}, 1);
    setupCombo(mSampleSyncCombo, mSampleSyncLabel, "Sample Sync");
    mSampleSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "sample_sync_mode", mSampleSyncCombo);

    // --- Sektion 2: Monophonic Pitch Modifiers ---
    setupRotary(mPitchQuantSlider, mPitchQuantLabel, "Quantize", juce::Colour(0xff00d2ff));
    mPitchQuantAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pitch_quantize", mPitchQuantSlider);

    setupRotary(mPitchInertiaSlider, mPitchInertiaLabel, "Inertia (ms)", juce::Colour(0xff00e676));
    mPitchInertiaAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pitch_inertia", mPitchInertiaSlider);

    mPitchFreezeCombo.addItemList(juce::StringArray{"Tracking", "Freeze"}, 1);
    setupCombo(mPitchFreezeCombo, mPitchFreezeLabel, "Hold Mode");
    mPitchFreezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.apvts, "pitch_freeze", mPitchFreezeCombo);

    setupRotary(mPitchInvertSlider, mPitchInvertLabel, "Inversion", juce::Colour(0xffff9100));
    mPitchInvertAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "pitch_inversion", mPitchInvertSlider);

    setupRotary(mVoiceDriftSlider, mVoiceDriftLabel, "Voice Drift", juce::Colour(0xffe040fb));
    mVoiceDriftAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, "voice_drift", mVoiceDriftSlider);

    // --- Sektion 3: 5 Stimmen (Voices 1 bis 5) ---
    juce::StringArray waveChoices{"DDSP", "Sine", "Saw", "Square", "Triangle"};

    for (int v = 1; v <= 5; ++v) {
        const juce::String prefix = "v" + juce::String(v) + "_";
        auto& vc = mVoices[static_cast<size_t>(v - 1)];

        setupRotary(vc.gainSlider, vc.gainLabel, "Gain", kTrackedParams[9 + (v - 1) * 5].colour);
        vc.gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "gain", vc.gainSlider);

        vc.sourceCombo.addItemList(waveChoices, 1);
        setupCombo(vc.sourceCombo, vc.sourceLabel, "Source");
        vc.sourceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processorRef.apvts, prefix + "source", vc.sourceCombo);

        vc.octSlider.setRange(-5.0, 5.0, 1.0);
        vc.octSlider.textFromValueFunction = [](double val) {
            int iv = static_cast<int>(std::round(val));
            return (iv > 0 ? "+" : "") + juce::String(iv) + " Oct";
        };
        vc.octSlider.setFillColour(juce::Colour(0xff40c4ff));
        addAndMakeVisible(vc.octSlider);
        vc.octLabel.setText("Oct", juce::dontSendNotification);
        vc.octLabel.setJustificationType(juce::Justification::centred);
        vc.octLabel.setFont(juce::FontOptions{10.0f});
        vc.octLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        vc.octLabel.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(vc.octLabel);
        vc.octAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "octave", vc.octSlider);

        vc.semiSlider.setRange(-12.0, 12.0, 1.0);
        vc.semiSlider.textFromValueFunction = [](double val) {
            int iv = static_cast<int>(std::round(val));
            return (iv > 0 ? "+" : "") + juce::String(iv) + " St";
        };
        vc.semiSlider.setFillColour(juce::Colour(0xff80d8ff));
        addAndMakeVisible(vc.semiSlider);
        vc.semiLabel.setText("Semi", juce::dontSendNotification);
        vc.semiLabel.setJustificationType(juce::Justification::centred);
        vc.semiLabel.setFont(juce::FontOptions{10.0f});
        vc.semiLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        vc.semiLabel.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(vc.semiLabel);
        vc.semiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "semitones", vc.semiSlider);

        vc.centSlider.setRange(-50.0, 50.0, 0.1);
        vc.centSlider.textFromValueFunction = [](double val) {
            int iv = static_cast<int>(std::round(val));
            return (iv > 0 ? "+" : "") + juce::String(iv) + " Ct";
        };
        vc.centSlider.setFillColour(juce::Colour(0xffb388ff));
        addAndMakeVisible(vc.centSlider);
        vc.centLabel.setText("Cent", juce::dontSendNotification);
        vc.centLabel.setJustificationType(juce::Justification::centred);
        vc.centLabel.setFont(juce::FontOptions{10.0f});
        vc.centLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        vc.centLabel.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(vc.centLabel);
        vc.centAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "cents", vc.centSlider);

        vc.panSlider.setRange(-1.0, 1.0, 0.01);
        vc.panSlider.textFromValueFunction = [](double val) {
            if (std::abs(val) < 0.02) return juce::String("C");
            return val < 0.0 ? juce::String(static_cast<int>(std::abs(val) * 100.0)) + "L"
                             : juce::String(static_cast<int>(val * 100.0)) + "R";
        };
        vc.panSlider.setFillColour(juce::Colour(0xffb8e986));
        addAndMakeVisible(vc.panSlider);
        vc.panLabel.setText("Pan", juce::dontSendNotification);
        vc.panLabel.setJustificationType(juce::Justification::centred);
        vc.panLabel.setFont(juce::FontOptions{10.0f});
        vc.panLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        vc.panLabel.setMouseCursor(juce::MouseCursor::NormalCursor);
        addAndMakeVisible(vc.panLabel);
        vc.panAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, prefix + "pan", vc.panSlider);
    }

    // 5x5 Matrix Overlay Toggle
    mMatrixViewToggle.setMouseCursor(juce::MouseCursor::NormalCursor);
    mMatrixViewToggle.onClick = [this]() {
        mShowMatrixOverlay = !mShowMatrixOverlay;
        mMatrixViewToggle.setButtonText(mShowMatrixOverlay ? "SHOW VOICES" : "5x5 MOD MATRIX");
        for (auto& v : mVoices) {
            v.gainSlider.setVisible(!mShowMatrixOverlay);
            v.gainLabel.setVisible(!mShowMatrixOverlay);
            v.sourceCombo.setVisible(!mShowMatrixOverlay);
            v.sourceLabel.setVisible(!mShowMatrixOverlay);
            v.octSlider.setVisible(!mShowMatrixOverlay);
            v.octLabel.setVisible(!mShowMatrixOverlay);
            v.semiSlider.setVisible(!mShowMatrixOverlay);
            v.semiLabel.setVisible(!mShowMatrixOverlay);
            v.centSlider.setVisible(!mShowMatrixOverlay);
            v.centLabel.setVisible(!mShowMatrixOverlay);
            v.panSlider.setVisible(!mShowMatrixOverlay);
            v.panLabel.setVisible(!mShowMatrixOverlay);
        }
        for (size_t s = 0; s < 5; ++s) {
            for (size_t d = 0; d < 5; ++d) {
                mMatrixCells[s][d].modeCombo.setVisible(mShowMatrixOverlay);
                mMatrixCells[s][d].amtSlider.setVisible(mShowMatrixOverlay);
            }
        }
        for (size_t v = 0; v < 5; ++v) {
            size_t baseIdx = 9 + v * 5;
            for (size_t p = 0; p < 5; ++p) {
                mSeqAutoButtons[baseIdx + p].setVisible(!mShowMatrixOverlay);
            }
        }
        resized();
        repaint();
    };
    addAndMakeVisible(mMatrixViewToggle);

    // 5x5 Matrix Cells Setup
    juce::StringArray matrixModes{"Off", "Add", "RingMod", "PhaseMod (FM)"};
    for (int src = 1; src <= 5; ++src) {
        for (int dst = 1; dst <= 5; ++dst) {
            auto& cell = mMatrixCells[static_cast<size_t>(src - 1)][static_cast<size_t>(dst - 1)];
            cell.modeCombo.addItemList(matrixModes, 1);
            cell.modeCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161920));
            cell.modeCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a313d));
            cell.modeCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
            cell.modeCombo.setMouseCursor(juce::MouseCursor::NormalCursor);
            cell.modeCombo.setVisible(false);
            addAndMakeVisible(cell.modeCombo);

            cell.amtSlider.setRange(0.0, 1.0, 0.01);
            cell.amtSlider.textFromValueFunction = [](double v) {
                return juce::String(static_cast<int>(std::round(v * 100.0))) + "%";
            };
            cell.amtSlider.setFillColour(juce::Colour(0xff00e5ff));
            cell.amtSlider.setMouseCursor(juce::MouseCursor::NormalCursor);
            cell.amtSlider.setVisible(false);
            addAndMakeVisible(cell.amtSlider);

            const juce::String cellId = "m_" + juce::String(src) + "_" + juce::String(dst) + "_";
            cell.modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processorRef.apvts, cellId + "mode", cell.modeCombo);
            cell.amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.apvts, cellId + "amt", cell.amtSlider);
        }
    }

    // Mapping für Greyed-Out-Zustand (34 Parameter)
    mTrackedParamUIs[0]  = { nullptr, nullptr }; // Dedizierte Volume-Hüllkurve
    mTrackedParamUIs[1]  = { &mDryWetSlider,       &mDryWetLabel };
    mTrackedParamUIs[2]  = { &mTiltSlider,         &mTiltLabel };
    mTrackedParamUIs[3]  = { &mTransientSlider,    &mTransientLabel };
    mTrackedParamUIs[4]  = { &mNoiseGainSlider,    &mNoiseGainLabel };
    mTrackedParamUIs[5]  = { &mPitchQuantSlider,   &mPitchQuantLabel };
    mTrackedParamUIs[6]  = { &mPitchInertiaSlider, &mPitchInertiaLabel };
    mTrackedParamUIs[7]  = { &mPitchInvertSlider,  &mPitchInvertLabel };
    mTrackedParamUIs[8]  = { &mVoiceDriftSlider,   &mVoiceDriftLabel };

    for (int v = 0; v < 5; ++v) {
        const size_t b = 9 + v * 5;
        mTrackedParamUIs[b + 0] = { &mVoices[static_cast<size_t>(v)].gainSlider, &mVoices[static_cast<size_t>(v)].gainLabel };
        mTrackedParamUIs[b + 1] = { &mVoices[static_cast<size_t>(v)].octSlider,  &mVoices[static_cast<size_t>(v)].octLabel };
        mTrackedParamUIs[b + 2] = { &mVoices[static_cast<size_t>(v)].semiSlider, &mVoices[static_cast<size_t>(v)].semiLabel };
        mTrackedParamUIs[b + 3] = { &mVoices[static_cast<size_t>(v)].centSlider, &mVoices[static_cast<size_t>(v)].centLabel };
        mTrackedParamUIs[b + 4] = { &mVoices[static_cast<size_t>(v)].panSlider,  &mVoices[static_cast<size_t>(v)].panLabel };
    }

    // 34 Checkboxen initialisieren
    for (size_t i = 0; i < kNumTrackedParams; ++i) {
        auto& btn = mSeqAutoButtons[i];
        btn.setButtonText(i == 0 ? "VOL SEQ" : "SEQ");
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

    // --- Sektion 4: Sequencer Canvas, Viewport & Toolbar ---
    mSeqViewport.setViewedComponent(&mSeqCanvas, false);
    mSeqViewport.setScrollBarsShown(false, true, false, true);
    mSeqViewport.setMouseCursor(juce::MouseCursor::NormalCursor);
    addAndMakeVisible(mSeqViewport);

    mStepCountCombo.addItemList(juce::StringArray{"1 Step", "2 Steps", "4 Steps", "8 Steps", "16 Steps", "32 Steps"}, 1);
    mStepCountCombo.setSelectedId(5, juce::dontSendNotification);
    mStepCountCombo.setMouseCursor(juce::MouseCursor::NormalCursor);
    mStepCountCombo.onChange = [this]() {
        static const int stepMap[] = { 1, 2, 4, 8, 16, 32 };
        const int steps = stepMap[std::clamp(mStepCountCombo.getSelectedId() - 1, 0, 5)];
        processorRef.getSequencer().setStepCount(steps);
        mSeqCanvas.updateCanvasWidth(mSeqViewport.getWidth());
    };
    addAndMakeVisible(mStepCountCombo);

    mGridSnapCombo.addItemList(juce::StringArray{"Snap: Off", "Snap: 1/4", "Snap: 1/8", "Snap: 1/16", "Snap: 1/32"}, 1);
    mGridSnapCombo.setSelectedId(4, juce::dontSendNotification);
    mGridSnapCombo.setMouseCursor(juce::MouseCursor::NormalCursor);
    mGridSnapCombo.onChange = [this]() {
        static const int snapMap[] = { 0, 4, 8, 16, 32 };
        mSeqCanvas.setGridSnap(snapMap[std::clamp(mGridSnapCombo.getSelectedId() - 1, 0, 4)]);
    };
    addAndMakeVisible(mGridSnapCombo);

    mCurveTypeCombo.addItemList(juce::StringArray{"Curve: Linear", "Curve: Exp", "Curve: Log", "Curve: S-Curve", "Curve: Stepped"}, 1);
    mCurveTypeCombo.setSelectedId(1, juce::dontSendNotification);
    mCurveTypeCombo.setMouseCursor(juce::MouseCursor::NormalCursor);
    mCurveTypeCombo.onChange = [this]() {
        static const SegmentCurveType types[] = {
            SegmentCurveType::Linear, SegmentCurveType::Exponential,
            SegmentCurveType::Logarithmic, SegmentCurveType::SCurve,
            SegmentCurveType::Stepped
        };
        mSeqCanvas.setCurrentCurveType(types[std::clamp(mCurveTypeCombo.getSelectedId() - 1, 0, 4)]);
    };
    addAndMakeVisible(mCurveTypeCombo);

    for (size_t i = 0; i < kNumTrackedParams; ++i) {
        mCurveSelectCombo.addItem(kTrackedParams[i].name, static_cast<int>(i + 1));
    }
    mCurveSelectCombo.setSelectedId(1, juce::dontSendNotification);
    mCurveSelectCombo.setMouseCursor(juce::MouseCursor::NormalCursor);
    mCurveSelectCombo.onChange = [this]() {
        const size_t idx = static_cast<size_t>(std::clamp(mCurveSelectCombo.getSelectedId() - 1, 0, static_cast<int>(kNumTrackedParams - 1)));
        mSeqCanvas.setSelectedParam(idx);
        mCurveVisibleToggle.setToggleState(mSeqCanvas.isParamVisible(idx), juce::dontSendNotification);
    };
    addAndMakeVisible(mCurveSelectCombo);

    mCurveVisibleToggle.setButtonText("SHOW CURVE");
    mCurveVisibleToggle.setToggleState(true, juce::dontSendNotification);
    mCurveVisibleToggle.setMouseCursor(juce::MouseCursor::NormalCursor);
    mCurveVisibleToggle.onClick = [this]() {
        const size_t idx = mSeqCanvas.getSelectedParam();
        mSeqCanvas.setParamVisible(idx, mCurveVisibleToggle.getToggleState());
    };
    addAndMakeVisible(mCurveVisibleToggle);

    mClearCurveBtn.setMouseCursor(juce::MouseCursor::NormalCursor);
    mClearCurveBtn.onClick = [this]() {
        const size_t idx = mSeqCanvas.getSelectedParam();
        processorRef.getSequencer().clearUserNodes(idx);
        processorRef.getSequencer().setAutomated(idx, false);
        updateAutomationState();
        mSeqCanvas.repaint();
    };
    addAndMakeVisible(mClearCurveBtn);

    // Fenster konfigurieren
    setResizable(true, true);
    setResizeLimits(980, 720, 2560, 1600);
    setSize(1060, 920);

    updateAutomationState();
    startTimerHz(60);
}

DDSPAudioProcessorEditor::~DDSPAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void DDSPAudioProcessorEditor::timerCallback() {
    mSeqCanvas.repaint();
    updateAutomationState();
}

void DDSPAudioProcessorEditor::syncSlidersToSequencer() {
    // Optionaler Live-Abgleich für GUI-Feedback
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
    g.fillAll(juce::Colour(0xff0e1013));

    auto drawCard = [&g](const juce::Rectangle<int>& area, const juce::String& title, const juce::Colour& titleColour = juce::Colour(0xff8b949e)) {
        if (area.isEmpty()) return;
        const auto fArea = area.toFloat();
        g.setColour(juce::Colour(0xff161920));
        g.fillRoundedRectangle(fArea, 6.0f);
        g.setColour(juce::Colour(0xff2a313d));
        g.drawRoundedRectangle(fArea, 6.0f, 1.0f);

        g.setColour(titleColour);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, area.getX() + 14, area.getY() + 6, area.getWidth() - 28, 16, juce::Justification::left);
    };

    drawCard(mCard1Area, "1. MASTER, TIMBRE & ENGINE MODE", juce::Colour(0xff00e5ff));
    drawCard(mCard2Area, "2. MONOPHONIC PITCH MODIFIERS", juce::Colour(0xff00e676));
    drawCard(mCard3Area, mShowMatrixOverlay ? "3. 5x5 CROSS-MODULATION MATRIX (ALL-TO-ALL)" : "3. 5-VOICE HARMONIC MATRIX", juce::Colour(0xffffb300));
    drawCard(mCard4Area, "4. MSEG SEQUENCER & AUTOMATION CANVAS (DRAG & DROP AUDIO SUPPORT)", juce::Colour(0xffff007f));

    // Matrix Header Beschriftung im Overlay-Modus
    if (mShowMatrixOverlay && !mCard3Area.isEmpty()) {
        const int startX = mCard3Area.getX() + 85;
        const int colW = (mCard3Area.getWidth() - 95) / 5;
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff00e5ff));
        for (int c = 0; c < 5; ++c) {
            g.drawText("TO V" + juce::String(c + 1), startX + c * colW, mCard3Area.getY() + 22, colW, 14, juce::Justification::centred);
        }

        const int startY = mCard3Area.getY() + 38;
        const int rowH = (mCard3Area.getHeight() - 44) / 5;
        g.setColour(juce::Colour(0xffffb300));
        for (int r = 0; r < 5; ++r) {
            g.drawText("SRC V" + juce::String(r + 1), mCard3Area.getX() + 10, startY + r * rowH, 70, rowH, juce::Justification::centredLeft);
        }
    } else if (!mShowMatrixOverlay && !mCard3Area.isEmpty()) {
        const int colW = (mCard3Area.getWidth() - 20) / 5;
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        static const char* voiceTitles[] = {
            "VOICE 1 (LEAD)", "VOICE 2 (SUB)", "VOICE 3 (HARM 1)", "VOICE 4 (HARM 2)", "VOICE 5 (HARM 3)"
        };
        for (int v = 0; v < 5; ++v) {
            const int colX = mCard3Area.getX() + 10 + v * colW;
            g.setColour(kTrackedParams[9 + v * 5].colour);
            g.drawText(voiceTitles[v], colX, mCard3Area.getY() + 6, colW - 6, 16, juce::Justification::centred);

            if (v > 0) {
                g.setColour(juce::Colour(0xff222733));
                g.drawVerticalLine(colX - 3, mCard3Area.getY() + 24.0f, static_cast<float>(mCard3Area.getBottom() - 10));
            }
        }
    }
}

void DDSPAudioProcessorEditor::resized() {
    const int totalW = getWidth();
    const int totalH = getHeight();
    const int marginX = 12;

    auto setupAreaWithSeq = [this](juce::Rectangle<int>& area, int width, juce::Slider& slider, juce::Label& label, size_t paramIdx) {
        auto knobArea = area.removeFromLeft(width);
        if (paramIdx < kNumTrackedParams) {
            mSeqAutoButtons[paramIdx].setBounds(knobArea.getX() + (width - 40) / 2, knobArea.getBottom() - 14, 40, 14);
        }
        label.setBounds(knobArea.removeFromBottom(28).removeFromTop(14));
        slider.setBounds(knobArea);
    };

    // 1. Header & Preset-Bar
    mHeaderArea = juce::Rectangle<int>(marginX, 8, totalW - 2 * marginX, 30);
    mTitleLabel.setBounds(mHeaderArea.getX(), mHeaderArea.getY(), 250, 30);

    const int presetStartX = mHeaderArea.getX() + 260;
    mPrevPresetBtn.setBounds(presetStartX, mHeaderArea.getY() + 2, 26, 26);
    mPresetCombo.setBounds(presetStartX + 30, mHeaderArea.getY() + 2, 210, 26);
    mNextPresetBtn.setBounds(presetStartX + 244, mHeaderArea.getY() + 2, 26, 26);
    mSavePresetBtn.setBounds(presetStartX + 276, mHeaderArea.getY() + 2, 54, 26);
    mLoadPresetBtn.setBounds(presetStartX + 334, mHeaderArea.getY() + 2, 54, 26);

    int currentY = 44;

    // 2. Sektion 1 & Sektion 2
    const int row1H = 135;
    const int totalAvailableW = totalW - 2 * marginX;
    const int card1W = (totalAvailableW - 10) * 58 / 100;
    const int card2W = totalAvailableW - card1W - 10;

    mCard1Area = juce::Rectangle<int>(marginX, currentY, card1W, row1H);
    mCard2Area = juce::Rectangle<int>(marginX + card1W + 10, currentY, card2W, row1H);
    currentY += row1H + 8;

    // Card 1 Layout
    const int c1KnobsW = card1W - 165;
    auto topKnobRow = juce::Rectangle<int>(mCard1Area.getX() + 8, mCard1Area.getY() + 22, c1KnobsW, 105);
    const int c1KnobW = c1KnobsW / 5;
    setupAreaWithSeq(topKnobRow, c1KnobW, mDryWetSlider, mDryWetLabel, 1);
    setupAreaWithSeq(topKnobRow, c1KnobW, mTiltSlider, mTiltLabel, 2);
    setupAreaWithSeq(topKnobRow, c1KnobW, mFormantSlider, mFormantLabel, 9999);
    setupAreaWithSeq(topKnobRow, c1KnobW, mTransientSlider, mTransientLabel, 3);
    setupAreaWithSeq(topKnobRow, c1KnobW, mNoiseGainSlider, mNoiseGainLabel, 4);

    const int c1CombosX = mCard1Area.getX() + c1KnobsW + 12;
    mSynthModeLabel.setBounds(c1CombosX, mCard1Area.getY() + 22, 140, 14);
    mSynthModeCombo.setBounds(c1CombosX, mCard1Area.getY() + 38, 140, 22);
    mSampleSyncLabel.setBounds(c1CombosX, mCard1Area.getY() + 66, 140, 14);
    mSampleSyncCombo.setBounds(c1CombosX, mCard1Area.getY() + 82, 140, 22);

    // Card 2 Layout
    const int c2KnobsW = card2W - 110;
    auto midKnobRow = juce::Rectangle<int>(mCard2Area.getX() + 8, mCard2Area.getY() + 22, c2KnobsW, 105);
    const int c2KnobW = c2KnobsW / 4;
    setupAreaWithSeq(midKnobRow, c2KnobW, mPitchQuantSlider, mPitchQuantLabel, 5);
    setupAreaWithSeq(midKnobRow, c2KnobW, mPitchInertiaSlider, mPitchInertiaLabel, 6);
    setupAreaWithSeq(midKnobRow, c2KnobW, mPitchInvertSlider, mPitchInvertLabel, 7);
    setupAreaWithSeq(midKnobRow, c2KnobW, mVoiceDriftSlider, mVoiceDriftLabel, 8);

    const int c2ComboX = mCard2Area.getX() + c2KnobsW + 10;
    mPitchFreezeLabel.setBounds(c2ComboX, mCard2Area.getY() + 34, 90, 14);
    mPitchFreezeCombo.setBounds(c2ComboX, mCard2Area.getY() + 52, 90, 24);

    // 3. Sektion 3: 5 Stimmen oder 5x5 Mod Matrix
    const int card3H = 220;
    mCard3Area = juce::Rectangle<int>(marginX, currentY, totalAvailableW, card3H);
    currentY += card3H + 8;

    mMatrixViewToggle.setBounds(mCard3Area.getRight() - 135, mCard3Area.getY() + 4, 125, 20);

    if (!mShowMatrixOverlay) {
        const int colW = (mCard3Area.getWidth() - 20) / 5;
        for (int v = 0; v < 5; ++v) {
            const int colX = mCard3Area.getX() + 10 + v * colW;
            auto colArea = juce::Rectangle<int>(colX, mCard3Area.getY() + 24, colW - 6, card3H - 28);
            auto& vc = mVoices[static_cast<size_t>(v)];
            const size_t baseIdx = 9 + v * 5;

            // Gain Knob + SEQ
            auto gainKnobArea = juce::Rectangle<int>(colArea.getX() + (colW - 64) / 2, colArea.getY(), 60, 60);
            vc.gainSlider.setBounds(gainKnobArea);
            vc.gainLabel.setBounds(colArea.getX(), gainKnobArea.getBottom() + 1, colW - 6, 13);
            mSeqAutoButtons[baseIdx + 0].setBounds(colArea.getX() + (colW - 44) / 2, gainKnobArea.getBottom() + 14, 40, 13);

            // Source Combo
            vc.sourceCombo.setBounds(colArea.getX() + 4, colArea.getY() + 92, colW - 14, 20);

            // Drag Slider 2x2 Grid (Oct, Semi, Cent, Pan)
            const int subW = (colW - 18) / 2;
            vc.octSlider.setBounds(colArea.getX() + 4, colArea.getY() + 116, subW, 18);
            vc.semiSlider.setBounds(colArea.getX() + 8 + subW, colArea.getY() + 116, subW, 18);

            vc.centSlider.setBounds(colArea.getX() + 4, colArea.getY() + 138, subW, 18);
            vc.panSlider.setBounds(colArea.getX() + 8 + subW, colArea.getY() + 138, subW, 18);

            // 4 Mini SEQ Buttons (Oct, Semi, Cent, Pan)
            const int bW = (colW - 20) / 4;
            mSeqAutoButtons[baseIdx + 1].setBounds(colArea.getX() + 4, colArea.getY() + 160, bW, 13);
            mSeqAutoButtons[baseIdx + 2].setBounds(colArea.getX() + 4 + bW, colArea.getY() + 160, bW, 13);
            mSeqAutoButtons[baseIdx + 3].setBounds(colArea.getX() + 4 + 2 * bW, colArea.getY() + 160, bW, 13);
            mSeqAutoButtons[baseIdx + 4].setBounds(colArea.getX() + 4 + 3 * bW, colArea.getY() + 160, bW, 13);
        }
    } else {
        const int startX = mCard3Area.getX() + 85;
        const int gridW = mCard3Area.getWidth() - 95;
        const int colW = gridW / 5;
        const int startY = mCard3Area.getY() + 38;
        const int rowH = (mCard3Area.getHeight() - 44) / 5;

        for (int src = 0; src < 5; ++src) {
            for (int dst = 0; dst < 5; ++dst) {
                const int cellX = startX + dst * colW + 4;
                const int cellY = startY + src * rowH + 2;
                const int modeW = (colW - 14) * 55 / 100;
                const int amtW = (colW - 14) - modeW - 4;

                auto& cell = mMatrixCells[static_cast<size_t>(src)][static_cast<size_t>(dst)];
                cell.modeCombo.setBounds(cellX, cellY + 4, modeW, rowH - 8);
                cell.amtSlider.setBounds(cellX + modeW + 4, cellY + 4, amtW, rowH - 8);
            }
        }
    }

    // 4. Sektion 4: MSEG Sequencer & Toolbar (Nimmt gesamten restlichen Platz ein)
    const int card4H = std::max(220, totalH - currentY - marginX);
    mCard4Area = juce::Rectangle<int>(marginX, currentY, totalAvailableW, card4H);

    const int tbY = mCard4Area.getY() + 24;
    int tbX = mCard4Area.getX() + 12;

    mStepCountCombo.setBounds(tbX, tbY, 95, 22); tbX += 100;
    mGridSnapCombo.setBounds(tbX, tbY, 95, 22); tbX += 100;
    mCurveTypeCombo.setBounds(tbX, tbY, 115, 22); tbX += 120;
    mCurveSelectCombo.setBounds(tbX, tbY, 150, 22); tbX += 155;
    mCurveVisibleToggle.setBounds(tbX, tbY, 95, 22); tbX += 100;
    mSeqAutoButtons[0].setBounds(tbX, tbY, 75, 22); tbX += 80;
    mClearCurveBtn.setBounds(tbX, tbY, 65, 22);

    const int viewY = tbY + 28;
    const int viewH = std::max(120, card4H - 58);
    mSeqViewport.setBounds(mCard4Area.getX() + 10, viewY, mCard4Area.getWidth() - 20, viewH);

    mSeqCanvas.updateCanvasWidth(mSeqViewport.getWidth());
}