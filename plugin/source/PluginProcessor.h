#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "InferenceWorker.h"
#include "PresetManager.h"
#include "../../dsp/include/SequencerEngine.hpp"

class DDSPAudioProcessor : public juce::AudioProcessor {
public:
    DDSPAudioProcessor();
    ~DDSPAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DDSP Timbre Transfer"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return mPresetManager.getNumPresets(); }
    int getCurrentProgram() override { return mCurrentPresetIndex; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override { return mPresetManager.getPresetName(index); }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    SequencerEngine& getSequencer() noexcept { return mSequencer; }
    PresetManager& getPresetManager() noexcept { return mPresetManager; }

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr int fifoCapacity = 8192;

    juce::AbstractFifo mInputFifo{fifoCapacity};
    std::vector<float> mInputBuffer;

    juce::AbstractFifo mOutputFifoL{fifoCapacity};
    std::vector<float> mOutputBufferL;

    juce::AbstractFifo mOutputFifoR{fifoCapacity};
    std::vector<float> mOutputBufferR;

    std::unique_ptr<InferenceWorker> mWorker;
    SequencerEngine mSequencer;
    PresetManager mPresetManager;

    std::atomic<int> mActiveMidiNote{-1};
    std::atomic<float> mActiveMidiVelocity{0.0f};
    int mCurrentPresetIndex = 0;

    std::atomic<float>* mDryWetParam = nullptr;
    std::atomic<float>* mSampleSyncModeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DDSPAudioProcessor)
};