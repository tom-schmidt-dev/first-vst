#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SequencerComponent.h"

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

        g.setColour(isEnabled() ? juce::Colour(0xff222630) : juce::Colour(0xff181a1f));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(isEnabled() ? juce::Colour(0xff3d4453) : juce::Colour(0xff282c35));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        const double norm = (getValue() - getMinimum()) / (getMaximum() - getMinimum());
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
    juce::Colour mFillColour{ 0xff4a90e2 };
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
    void updateLfoRateControls();
    void updateAutomationState();

    DDSPAudioProcessor& processorRef;

    // Sektion 1: Master, LFO & Timbre
    DragSlider mDryWetSlider;
    juce::Label mDryWetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mDryWetAttach;

    DragSlider mDetuneSlider;
    juce::Label mDetuneLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mDetuneAttach;

    DragSlider mSpreadSlider;
    juce::Label mSpreadLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mSpreadAttach;

    DragSlider mTiltSlider;
    juce::Label mTiltLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTiltAttach;

    DragSlider mFormantSlider;
    juce::Label mFormantLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mFormantAttach;

    DragSlider mToleranceSlider;
    juce::Label mToleranceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mToleranceAttach;

    DragSlider mTransientSlider;
    juce::Label mTransientLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mTransientAttach;

    DragSlider mNoiseGainSlider;
    juce::Label mNoiseGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mNoiseGainAttach;

    DragSlider mLfoDepthSlider;
    juce::Label mLfoDepthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mLfoDepthAttach;

    juce::ComboBox mLfoWaveCombo;
    juce::Label    mLfoWaveLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mLfoWaveAttach;

    juce::ComboBox mLfoSyncCombo;
    juce::Label    mLfoSyncLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mLfoSyncAttach;

    NumberDragSlider mLfoRateHzSlider;
    juce::Label      mLfoRateHzLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mLfoRateHzAttach;

    juce::ComboBox mLfoRateSyncCombo;
    juce::Label    mLfoRateSyncLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mLfoRateSyncAttach;

    // Sektion 2: Monophonic Pitch Modifiers
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

    // Sektion 3: Harmony Matrix & Warp Modes
    NumberDragSlider mHarmBalanceSlider;
    juce::Label      mHarmBalanceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mHarmBalanceAttach;

    juce::ComboBox mMixModeCombo;
    juce::Label    mMixModeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mMixModeAttach;

    struct VoiceControls {
        DragSlider   gainSlider;
        juce::Label  gainLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;

        DragSlider   octSlider;
        juce::Label  octLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octAttach;

        DragSlider   semiSlider;
        juce::Label  semiLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> semiAttach;

        juce::ComboBox waveCombo;
        juce::Label    waveLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    };

    VoiceControls mSubVoice;
    VoiceControls mHigh1Voice;
    VoiceControls mHigh2Voice;
    VoiceControls mHigh3Voice;

    std::array<juce::ToggleButton, kNumTrackedParams> mSeqAutoButtons;

    struct TrackedParamUI {
        juce::Slider* slider = nullptr;
        juce::Label*  label  = nullptr;
    };
    std::array<TrackedParamUI, kNumTrackedParams> mTrackedParamUIs;

    // Sektion 4: Sequencer Canvas & Toolbar
    SequencerCanvas mSeqCanvas;
    juce::Viewport  mSeqViewport;

    juce::ComboBox     mStepCountCombo;
    juce::ComboBox     mGridSnapCombo;
    juce::ComboBox     mCurveSelectCombo;
    juce::ToggleButton mCurveVisibleToggle;

    // Dynamische Begrenzungsrahmen für paint()
    juce::Rectangle<int> mCard1Area;
    juce::Rectangle<int> mCard2Area;
    juce::Rectangle<int> mCard3Area;
    juce::Rectangle<int> mCard4Area;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DDSPAudioProcessorEditor)
};