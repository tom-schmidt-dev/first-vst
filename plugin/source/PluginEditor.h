#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "SequencerComponent.h"
#include "PresetManager.h"

class FlatLookAndFeel : public juce::LookAndFeel_V4 {
public:
    FlatLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff0e1013));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff161920));
        setColour(juce::PopupMenu::textColourId, juce::Colours::white);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff222733));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff00e5ff));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e232e));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd0d7de));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff161920));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2a313d));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);
        auto radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto center = bounds.getCentre();

        // 1. Dunkler Hintergrundkreis
        g.setColour(juce::Colour(0xff161920));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        // 2. 1px Hairline-Border (#2A313D)
        g.setColour(juce::Colour(0xff2a313d));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // 3. Dezent inaktive Bogenbahn
        const float arcRadius = radius - 3.5f;
        juce::Path bgArc;
        bgArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff222733));
        g.strokePath(bgArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Aktiver Bogen mit Akzentfarbe
        const float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path activeArc;
        activeArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, currentAngle, true);
        auto accentColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(slider.isEnabled() ? accentColour : accentColour.withAlpha(0.35f));
        g.strokePath(activeArc, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 5. Zeigerpunkt
        const float dotDist = radius * 0.65f;
        const float dotX = center.x + dotDist * std::sin(currentAngle);
        const float dotY = center.y - dotDist * std::cos(currentAngle);
        g.setColour(slider.isEnabled() ? juce::Colours::white : juce::Colour(0xff7a8494));
        g.fillEllipse(dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);
    }
};

class DragSlider : public juce::Slider {
public:
    DragSlider() {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    juce::MouseCursor getMouseCursor() override {
        return juce::MouseCursor::NormalCursor;
    }
};

class NumberDragSlider : public juce::Slider {
public:
    NumberDragSlider() {
        setSliderStyle(juce::Slider::LinearBarVertical);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    juce::MouseCursor getMouseCursor() override {
        return juce::MouseCursor::NormalCursor;
    }

    void mouseDown(const juce::MouseEvent& e) override {
        mLastY = e.position.y;
        juce::Slider::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const float dy = mLastY - e.position.y;
        mLastY = e.position.y;

        const double range = getMaximum() - getMinimum();
        const double factor = e.mods.isShiftDown() ? 0.001 : 0.005;
        const double delta = dy * range * factor;

        setValue(std::clamp(getValue() + delta, getMinimum(), getMaximum()), juce::sendNotificationSync);
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(isEnabled() ? juce::Colour(0xff161920) : juce::Colour(0xff121417));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(isEnabled() ? juce::Colour(0xff2a313d) : juce::Colour(0xff1e222a));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        const double norm = (getMaximum() > getMinimum()) ? ((getValue() - getMinimum()) / (getMaximum() - getMinimum())) : 0.0;
        auto fillBounds = bounds.reduced(1.5f);
        fillBounds.setWidth(fillBounds.getWidth() * static_cast<float>(std::clamp(norm, 0.0, 1.0)));
        g.setColour(mFillColour.withAlpha(isEnabled() ? 0.35f : 0.12f));
        g.fillRoundedRectangle(fillBounds, 3.0f);

        g.setColour(isEnabled() ? juce::Colours::white : juce::Colour(0xff6a7280));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));

        juce::String text = getTextFromValue(getValue());
        if (mSuffix.isNotEmpty() && !text.endsWith(mSuffix)) text += mSuffix;

        g.drawText(text, bounds, juce::Justification::centred, true);
    }

    void setCustomSuffix(const juce::String& s) { mSuffix = s; }
    void setFillColour(juce::Colour c) { mFillColour = c; repaint(); }

private:
    float mLastY = 0.0f;
    juce::Colour mFillColour{ 0xff00e5ff };
    juce::String mSuffix;
};

class DDSPAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit DDSPAudioProcessorEditor(DDSPAudioProcessor&);
    ~DDSPAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    juce::MouseCursor getMouseCursor() override {
        return juce::MouseCursor::NormalCursor;
    }

private:
    void timerCallback() override;
    void updateAutomationState();
    void syncSlidersToSequencer();

    DDSPAudioProcessor& processorRef;
    FlatLookAndFeel mFlatLaf;

    // --- Header & Preset-Bar ---
    juce::Label        mTitleLabel;
    juce::ComboBox     mPresetCombo;
    juce::TextButton   mPrevPresetBtn{"<"};
    juce::TextButton   mNextPresetBtn{">"};
    juce::TextButton   mSavePresetBtn{"SAVE"};
    juce::TextButton   mLoadPresetBtn{"LOAD"};
    std::unique_ptr<juce::FileChooser> mFileChooser;

    // --- Sektion 1: Master, Timbre & Engine Mode ---
    DragSlider mDryWetSlider;
    juce::Label mDryWetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mDryWetAttach;

    DragSlider mTiltSlider;
    juce::Label mTiltLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTiltAttach;

    DragSlider mFormantSlider;
    juce::Label mFormantLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mFormantAttach;

    DragSlider mTransientSlider;
    juce::Label mTransientLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTransientAttach;

    DragSlider mNoiseGainSlider;
    juce::Label mNoiseGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mNoiseGainAttach;

    juce::ComboBox mSynthModeCombo;
    juce::Label    mSynthModeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mSynthModeAttach;

    juce::ComboBox mSampleSyncCombo;
    juce::Label    mSampleSyncLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mSampleSyncAttach;

    // --- Sektion 2: Monophonic Pitch Modifiers ---
    DragSlider mPitchQuantSlider;
    juce::Label mPitchQuantLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mPitchQuantAttach;

    DragSlider mPitchInertiaSlider;
    juce::Label mPitchInertiaLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mPitchInertiaAttach;

    juce::ComboBox mPitchFreezeCombo;
    juce::Label    mPitchFreezeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mPitchFreezeAttach;

    DragSlider mPitchInvertSlider;
    juce::Label mPitchInvertLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mPitchInvertAttach;

    DragSlider mVoiceDriftSlider;
    juce::Label mVoiceDriftLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mVoiceDriftAttach;

    // --- Sektion 3: 5 Stimmen (Voices 1 bis 5) ---
    struct VoiceControls {
        DragSlider       gainSlider;
        juce::Label      gainLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;

        juce::ComboBox   sourceCombo;
        juce::Label      sourceLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttach;

        NumberDragSlider octSlider;
        juce::Label      octLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octAttach;

        NumberDragSlider semiSlider;
        juce::Label      semiLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> semiAttach;

        NumberDragSlider centSlider;
        juce::Label      centLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> centAttach;

        NumberDragSlider panSlider;
        juce::Label      panLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttach;
    };
    std::array<VoiceControls, 5> mVoices;

    // 5x5 Modulationsmatrix Modal/Overlay Button
    juce::TextButton mMatrixViewToggle{"5x5 MOD MATRIX"};
    bool mShowMatrixOverlay = false;

    struct MatrixCellUI {
        juce::ComboBox modeCombo;
        NumberDragSlider amtSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;
    };
    std::array<std::array<MatrixCellUI, 5>, 5> mMatrixCells;

    // --- Automations-Kopplung (34 Parameter) ---
    std::array<juce::ToggleButton, kNumTrackedParams> mSeqAutoButtons;

    struct TrackedParamUI {
        juce::Slider* slider = nullptr;
        juce::Label*  label  = nullptr;
    };
    std::array<TrackedParamUI, kNumTrackedParams> mTrackedParamUIs;

    // --- Sektion 4: MSEG Sequencer Canvas & Toolbar ---
    SequencerCanvas mSeqCanvas;
    juce::Viewport  mSeqViewport;

    juce::ComboBox     mStepCountCombo;
    juce::ComboBox     mGridSnapCombo;
    juce::ComboBox     mCurveTypeCombo;
    juce::ComboBox     mCurveSelectCombo;
    juce::ToggleButton mCurveVisibleToggle;
    juce::TextButton   mClearCurveBtn{"RESET"};

    // Dynamische Begrenzungsrahmen für paint()
    juce::Rectangle<int> mHeaderArea;
    juce::Rectangle<int> mCard1Area;
    juce::Rectangle<int> mCard2Area;
    juce::Rectangle<int> mCard3Area;
    juce::Rectangle<int> mCard4Area;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DDSPAudioProcessorEditor)
};