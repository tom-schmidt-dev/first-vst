#include "PluginProcessor.h"
#include "PluginEditor.h"

DDSPAudioProcessor::DDSPAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      mInputBuffer(fifoCapacity, 0.0f),
      mOutputBufferL(fifoCapacity, 0.0f),
      mOutputBufferR(fifoCapacity, 0.0f)
{
    mDryWetParam = apvts.getRawParameterValue("dry_wet");
    mSampleSyncModeParam = apvts.getRawParameterValue("sample_sync_mode");
}

DDSPAudioProcessor::~DDSPAudioProcessor() {
    releaseResources();
}

juce::AudioProcessorValueTreeState::ParameterLayout DDSPAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Master & Timbre
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dry_wet", 1}, "Dry / Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.00f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"spectral_tilt", 1}, "Dynamic Tilt",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"transient_track", 1}, "Attack Track",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"noise_gain", 1}, "Breath / Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"formant_blend", 1}, "Formant Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.00f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"sample_sync_mode", 1}, "Sample Sync Mode",
        juce::StringArray{"Timeline Sync", "Classic Resample"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"synth_mode", 1}, "Engine Mode",
        juce::StringArray{"Auto (MIDI/Audio)", "Force MIDI Synth", "Force Audio FX"}, 0));

    // 2. Monophonic Pitch Modifiers
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pitch_quantize", 1}, "Pitch Quantize",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pitch_inertia", 1}, "Pitch Inertia (ms)",
        juce::NormalisableRange<float>(0.0f, 1500.0f, 1.0f, 0.4f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"pitch_freeze", 1}, "Hold Mode",
        juce::StringArray{"Tracking", "Freeze"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pitch_inversion", 1}, "Pitch Invert",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"voice_drift", 1}, "Voice Drift (Cents)",
        juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 0.0f));

    // 3. 5 Stimmen (Voices 1 bis 5)
    juce::StringArray waveChoices{"DDSP", "Sine", "Saw", "Square", "Triangle"};

    for (int v = 1; v <= 5; ++v) {
        const juce::String prefix = "v" + juce::String(v) + "_";
        const juce::String namePrefix = "Voice " + juce::String(v) + " ";

        const float defaultGain = (v == 1) ? 1.0f : 0.0f;
        const int defaultOct    = 0;
        const int defaultSrc    = (v == 1) ? 0 : 1;
        const float defaultPan  = 0.0f;

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "gain", 1}, namePrefix + "Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), defaultGain));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{prefix + "octave", 1}, namePrefix + "Octave", -5, 5, defaultOct));

        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{prefix + "semitones", 1}, namePrefix + "Semitones", -12, 12, 0));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "cents", 1}, namePrefix + "Cents",
            juce::NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{prefix + "source", 1}, namePrefix + "Source", waveChoices, defaultSrc));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{prefix + "pan", 1}, namePrefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), defaultPan));
    }

    // 4. 5x5 All-to-All Modulationsmatrix
    juce::StringArray matrixModes{"Off", "Add", "RingMod", "PhaseMod (FM)"};

    for (int src = 1; src <= 5; ++src) {
        for (int dst = 1; dst <= 5; ++dst) {
            const juce::String cellId = "m_" + juce::String(src) + "_" + juce::String(dst) + "_";
            const juce::String cellName = "Mod V" + juce::String(src) + "->V" + juce::String(dst) + " ";

            params.push_back(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{cellId + "mode", 1}, cellName + "Mode", matrixModes, 0));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{cellId + "amt", 1}, cellName + "Amount",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
        }
    }

    return { params.begin(), params.end() };
}

void DDSPAudioProcessor::setCurrentProgram(int index) {
    if (index >= 0 && index < mPresetManager.getNumPresets()) {
        mCurrentPresetIndex = index;
        mPresetManager.applyPreset(index, apvts, mSequencer);
    }
}

void DDSPAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    setLatencySamples(2048);

    juce::File currentBinary = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File modelFile = currentBinary.getSiblingFile("ddsp_decoder.onnx");

    if (!modelFile.existsAsFile()) {
        modelFile = currentBinary.getParentDirectory().getChildFile("ddsp_decoder.onnx");
    }

    mWorker = std::make_unique<InferenceWorker>(
        mInputFifo, mInputBuffer,
        mOutputFifoL, mOutputBufferL,
        mOutputFifoR, mOutputBufferR,
        apvts, mSequencer, modelFile, sampleRate,
        mActiveMidiNote, mActiveMidiVelocity
    );
    mWorker->startThread(juce::Thread::Priority::highest);

    reset();
}

void DDSPAudioProcessor::reset() {
    mInputFifo.reset();
    mOutputFifoL.reset();
    mOutputFifoR.reset();

    std::fill(mInputBuffer.begin(), mInputBuffer.end(), 0.0f);
    std::fill(mOutputBufferL.begin(), mOutputBufferL.end(), 0.0f);
    std::fill(mOutputBufferR.begin(), mOutputBufferR.end(), 0.0f);

    const int preBufferSamples = 2048;
    int s1, sz1, s2, sz2;

    mOutputFifoL.prepareToWrite(preBufferSamples, s1, sz1, s2, sz2);
    if (sz1 > 0) std::fill_n(&mOutputBufferL[s1], sz1, 0.0f);
    if (sz2 > 0) std::fill_n(&mOutputBufferL[s2], sz2, 0.0f);
    mOutputFifoL.finishedWrite(preBufferSamples);

    mOutputFifoR.prepareToWrite(preBufferSamples, s1, sz1, s2, sz2);
    if (sz1 > 0) std::fill_n(&mOutputBufferR[s1], sz1, 0.0f);
    if (sz2 > 0) std::fill_n(&mOutputBufferR[s2], sz2, 0.0f);
    mOutputFifoR.finishedWrite(preBufferSamples);
}

void DDSPAudioProcessor::releaseResources() {
    if (mWorker) {
        mWorker->stopThread(2000);
        mWorker.reset();
    }
}

void DDSPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    // 1. Eingehende MIDI-Nachrichten auswerten
    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        if (message.isNoteOn()) {
            mActiveMidiNote.store(message.getNoteNumber(), std::memory_order_relaxed);
            mActiveMidiVelocity.store(message.getFloatVelocity(), std::memory_order_relaxed);
            mSequencer.onNoteOn();
        } else if (message.isNoteOff()) {
            if (mActiveMidiNote.load(std::memory_order_relaxed) == message.getNoteNumber()) {
                mActiveMidiNote.store(-1, std::memory_order_relaxed);
                mActiveMidiVelocity.store(0.0f, std::memory_order_relaxed);
            }
        }
    }

    if (numSamples == 0) return;

    // 2. DAW Transport an Sequencer übergeben
    bool isPlaying = false;
    double ppq = 0.0;
    double bpm = 120.0;
    if (auto* playHead = getPlayHead()) {
        if (auto pos = playHead->getPosition()) {
            if (pos->getPpqPosition().hasValue()) ppq = *pos->getPpqPosition();
            if (pos->getBpm().hasValue()) bpm = *pos->getBpm();
            isPlaying = pos->getIsPlaying();
        }
    }
    mSequencer.updateTransport(ppq, isPlaying, bpm, getSampleRate(), numSamples);

    // 3. Sample-Audio generieren (falls geladen)
    if (mSampleSyncModeParam != nullptr) {
        const int syncVal = static_cast<int>(mSampleSyncModeParam->load());
        mSequencer.setSampleSyncMode(syncVal == 1 ? SampleSyncMode::ClassicResample : SampleSyncMode::TimelineSync);
    }

    std::vector<float> sampleAudio(static_cast<size_t>(numSamples), 0.0f);
    const bool hasSample = mSequencer.hasSample();
    const int activeNote = mActiveMidiNote.load(std::memory_order_relaxed);

    if (hasSample && (isPlaying || activeNote >= 0)) {
        const float startPhase = mSequencer.getCurrentPhase();
        const int steps = mSequencer.getStepCount();
        const double cycleBeats = static_cast<double>(steps) * 0.25;
        const double cycleSec = cycleBeats * (60.0 / std::max(20.0, bpm));
        const float phaseDelta = (cycleSec > 0.001) ? static_cast<float>(static_cast<double>(numSamples) / (cycleSec * getSampleRate())) : 0.0f;
        const float endPhase = startPhase + phaseDelta;

        const float midiPitchRatio = (activeNote >= 0) ? std::pow(2.0f, static_cast<float>(activeNote - 60) / 12.0f) : 1.0f;
        mSequencer.readSampleBlock(sampleAudio.data(), numSamples, startPhase, endPhase, midiPitchRatio, getSampleRate(), 0);
    }

    // 4. Eingangssignal in FIFO schreiben (für Formant- und Audio-FX-Tracking)
    const float* inL = (totalNumInputChannels > 0) ? buffer.getReadPointer(0) : nullptr;
    const float* inR = (totalNumInputChannels > 1) ? buffer.getReadPointer(1) : inL;

    int start1, size1, start2, size2;
    mInputFifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) {
        float inSig = inL ? 0.5f * (inL[i] + (inR ? inR[i] : inL[i])) : 0.0f;
        if (hasSample) inSig += sampleAudio[static_cast<size_t>(i)];
        mInputBuffer[start1 + i] = inSig;
    }
    for (int i = 0; i < size2; ++i) {
        float inSig = inL ? 0.5f * (inL[size1 + i] + (inR ? inR[size1 + i] : inL[size1 + i])) : 0.0f;
        if (hasSample) inSig += sampleAudio[static_cast<size_t>(size1 + i)];
        mInputBuffer[start2 + i] = inSig;
    }
    mInputFifo.finishedWrite(numSamples);

    if (mWorker != nullptr) mWorker->notify();

    if (isNonRealtime() && mWorker != nullptr) {
        while (mOutputFifoL.getNumReady() < numSamples && mWorker->isThreadRunning()) {
            mWorker->notify();
            juce::Thread::sleep(1);
        }
    }

    // 5. Sequencer-übersteuerter Dry/Wet Parameter (Index 1)
    const float dryWet = (mSequencer.isAutomated(1) && apvts.getParameter("dry_wet"))
        ? denormaliseParam(apvts.getParameter("dry_wet")->getNormalisableRange(), mSequencer.getInterpolatedValue(1))
        : (mDryWetParam ? mDryWetParam->load() : 1.00f);

    // 6. Ausgangs-FIFOs lesen und Dry/Wet mischen
    float* channelDataL = buffer.getWritePointer(0);
    float* channelDataR = (totalNumOutputChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    const int readyL = mOutputFifoL.getNumReady();
    const int readyR = mOutputFifoR.getNumReady();
    const int samplesToRead = std::min({ numSamples, readyL, readyR });

    if (samplesToRead > 0) {
        int readStart1L, readSize1L, readStart2L, readSize2L;
        int readStart1R, readSize1R, readStart2R, readSize2R;

        mOutputFifoL.prepareToRead(samplesToRead, readStart1L, readSize1L, readStart2L, readSize2L);
        mOutputFifoR.prepareToRead(samplesToRead, readStart1R, readSize1R, readStart2R, readSize2R);

        for (int i = 0; i < samplesToRead; ++i) {
            float dryL = channelDataL[i];
            float dryR = channelDataR ? channelDataR[i] : dryL;

            if (hasSample) {
                dryL += sampleAudio[static_cast<size_t>(i)];
                if (channelDataR) dryR += sampleAudio[static_cast<size_t>(i)];
            }

            const float wetL = (i < readSize1L) ? mOutputBufferL[readStart1L + i] : mOutputBufferL[readStart2L + (i - readSize1L)];
            const float wetR = (i < readSize1R) ? mOutputBufferR[readStart1R + i] : mOutputBufferR[readStart2R + (i - readSize1R)];

            channelDataL[i] = (1.0f - dryWet) * dryL + dryWet * wetL;
            if (channelDataR) {
                channelDataR[i] = (1.0f - dryWet) * dryR + dryWet * wetR;
            }
        }

        mOutputFifoL.finishedRead(samplesToRead);
        mOutputFifoR.finishedRead(samplesToRead);

        for (int i = samplesToRead; i < numSamples; ++i) {
            float dryL = channelDataL[i];
            float dryR = channelDataR ? channelDataR[i] : dryL;
            if (hasSample) {
                dryL += sampleAudio[static_cast<size_t>(i)];
                if (channelDataR) dryR += sampleAudio[static_cast<size_t>(i)];
            }
            channelDataL[i] = (1.0f - dryWet) * dryL;
            if (channelDataR) channelDataR[i] = (1.0f - dryWet) * dryR;
        }
    } else {
        for (int i = 0; i < numSamples; ++i) {
            float dryL = channelDataL[i];
            float dryR = channelDataR ? channelDataR[i] : dryL;
            if (hasSample) {
                dryL += sampleAudio[static_cast<size_t>(i)];
                if (channelDataR) dryR += sampleAudio[static_cast<size_t>(i)];
            }
            channelDataL[i] = (1.0f - dryWet) * dryL;
            if (channelDataR) channelDataR[i] = (1.0f - dryWet) * dryR;
        }
    }
}

juce::AudioProcessorEditor* DDSPAudioProcessor::createEditor() {
    return new DDSPAudioProcessorEditor(*this);
}

void DDSPAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    auto seqXml = mSequencer.exportXml();
    xml->addChildElement(seqXml.release());
    xml->setAttribute("currentPresetIndex", mCurrentPresetIndex);
    copyXmlToBinary(*xml, destData);
}

void DDSPAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
        if (auto* seqXml = xmlState->getChildByName("SEQUENCER")) {
            mSequencer.importXml(seqXml);
        }
        mCurrentPresetIndex = xmlState->getIntAttribute("currentPresetIndex", 0);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DDSPAudioProcessor();
}