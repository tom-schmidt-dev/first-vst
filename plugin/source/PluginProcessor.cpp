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
    mDryWetParam      = apvts.getRawParameterValue("dry_wet");
    mLfoDepthParam    = apvts.getRawParameterValue("lfo_depth");
    mLfoWaveParam     = apvts.getRawParameterValue("lfo_wave");
    mLfoSyncParam     = apvts.getRawParameterValue("lfo_sync_mode");
    mLfoRateHzParam   = apvts.getRawParameterValue("lfo_rate_hz");
    mLfoRateSyncParam = apvts.getRawParameterValue("lfo_rate_sync");
}

DDSPAudioProcessor::~DDSPAudioProcessor() {
    releaseResources();
}

juce::AudioProcessorValueTreeState::ParameterLayout DDSPAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Sektion 1: Master, LFO & Timbre
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dry_wet", 1}, "Dry / Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.70f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"detune_cents", 1}, "Ensemble Detune",
        juce::NormalisableRange<float>(0.0f, 30.0f, 0.1f), 8.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"stereo_spread", 1}, "Stereo Spread",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.80f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"spectral_tilt", 1}, "Dynamic Tilt",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.60f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"formant_blend", 1}, "Formant Match",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"tracking_tolerance", 1}, "Tolerance",
        juce::NormalisableRange<float>(0.30f, 0.95f, 0.01f), 0.70f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"transient_track", 1}, "Attack Track",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"noise_gain", 1}, "Noise / Breath",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.20f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo_depth", 1}, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo_wave", 1}, "LFO Waveform",
        juce::StringArray{"Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo_sync_mode", 1}, "LFO Sync Mode",
        juce::StringArray{"Free (Hz)", "Tempo Sync"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"lfo_rate_hz", 1}, "LFO Rate Hz",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.35f), 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"lfo_rate_sync", 1}, "LFO Rate Sync",
        juce::StringArray{"1/32", "1/16", "1/8", "1/4", "1/2", "1/1", "2/1", "4/1", "1/8T", "1/4T"}, 3));

    // Sektion 2: Monophonic Pitch Modifiers
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pitch_quantize", 1}, "Pitch Quantize",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pitch_inertia", 1}, "Pitch Inertia (ms)",
        juce::NormalisableRange<float>(0.0f, 1500.0f, 1.0f, 0.4f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"pitch_freeze", 1}, "Pitch Freeze",
        juce::StringArray{"Tracking", "Freeze"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"pitch_inversion", 1}, "Pitch Invert",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"voice_drift", 1}, "Voice Drift (Cents)",
        juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 0.0f));

    // Sektion 3: Harmony Matrix & Warp Modes
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"harmony_balance", 1}, "Harmony Balance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"mix_mode", 1}, "Mix Mode",
        juce::StringArray{"Add", "Ring Mod", "Phase Mod (FM)"}, 0));

    juce::StringArray waveChoices{"Sine", "Saw", "Square", "Triangle"};

    // Sub Voice
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"sub_gain", 1}, "Sub Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"sub_octave", 1}, "Sub Oct", -3, 1, -1));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"sub_semitones", 1}, "Sub Semi", -12, 12, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"sub_wave", 1}, "Sub Wave", waveChoices, 0));

    // High 1 Voice
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"high_gain", 1}, "High 1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"high_octave", 1}, "High 1 Oct", -1, 3, 1));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"high_semitones", 1}, "High 1 Semi", -12, 12, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"high1_wave", 1}, "High 1 Wave", waveChoices, 0));

    // High 2 Voice
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"high2_gain", 1}, "High 2 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.00f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"high2_octave", 1}, "High 2 Oct", -1, 3, 2));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"high2_semitones", 1}, "High 2 Semi", -12, 12, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"high2_wave", 1}, "High 2 Wave", waveChoices, 0));

    // High 3 Voice
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"high3_gain", 1}, "High 3 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.00f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"high3_octave", 1}, "High 3 Oct", -1, 3, 1));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"high3_semitones", 1}, "High 3 Semi", -12, 12, 7));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"high3_wave", 1}, "High 3 Wave", waveChoices, 0));

    return { params.begin(), params.end() };
}

void DDSPAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    setLatencySamples(2048);
    mLfo.setSampleRate(static_cast<float>(sampleRate));
    mLfo.reset();

    juce::File currentBinary = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File modelFile = currentBinary.getSiblingFile("ddsp_decoder.onnx");

    if (!modelFile.existsAsFile()) {
        modelFile = currentBinary.getParentDirectory().getChildFile("ddsp_decoder.onnx");
    }

    mWorker = std::make_unique<InferenceWorker>(
        mInputFifo, mInputBuffer,
        mOutputFifoL, mOutputBufferL,
        mOutputFifoR, mOutputBufferR,
        apvts, modelFile, sampleRate
    );
    mWorker->startThread(juce::Thread::Priority::highest);

    reset();
}

void DDSPAudioProcessor::reset() {
    mInputFifo.reset();
    mOutputFifoL.reset();
    mOutputFifoR.reset();
    mLfo.reset();

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

void DDSPAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    if (totalNumInputChannels == 0 || numSamples == 0) return;

    // 1. Mono-Signal in Eingangs-FIFO schreiben
    const float* inL = buffer.getReadPointer(0);
    const float* inR = totalNumInputChannels > 1 ? buffer.getReadPointer(1) : inL;

    int start1, size1, start2, size2;
    mInputFifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i) {
        mInputBuffer[start1 + i] = 0.5f * (inL[i] + inR[i]);
    }
    for (int i = 0; i < size2; ++i) {
        mInputBuffer[start2 + i] = 0.5f * (inL[size1 + i] + inR[size1 + i]);
    }
    mInputFifo.finishedWrite(numSamples);

    if (mWorker != nullptr) mWorker->notify();

    if (isNonRealtime() && mWorker != nullptr) {
        while (mOutputFifoL.getNumReady() < numSamples && mWorker->isThreadRunning()) {
            mWorker->notify();
            juce::Thread::sleep(1);
        }
    }

    // 2. LFO-Frequenz ermitteln
    const float lfoDepth       = mLfoDepthParam ? mLfoDepthParam->load() : 0.0f;
    const int lfoWaveIdx       = mLfoWaveParam ? static_cast<int>(mLfoWaveParam->load()) : 0;
    const int lfoSyncIdx       = mLfoSyncParam ? static_cast<int>(mLfoSyncParam->load()) : 0;
    const float lfoRateHz      = mLfoRateHzParam ? mLfoRateHzParam->load() : 2.0f;
    const int lfoRateSyncIdx   = mLfoRateSyncParam ? static_cast<int>(mLfoRateSyncParam->load()) : 3;

    float effectiveLfoFreq = lfoRateHz;
    if (lfoSyncIdx == 1) {
        double bpm = 120.0;
        if (auto* playHead = getPlayHead()) {
            if (auto pos = playHead->getPosition()) {
                if (pos->getBpm().hasValue()) {
                    bpm = *pos->getBpm();
                }
            }
        }
        static const float beatDivisions[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 0.33333f, 0.66667f };
        const float beats = beatDivisions[std::clamp(lfoRateSyncIdx, 0, 9)];
        effectiveLfoFreq = static_cast<float>((bpm / 60.0) / beats);
    }

    const auto lfoWaveform = static_cast<LFOWaveform>(std::clamp(lfoWaveIdx, 0, 5));

    // 3. Ausgangsdaten lesen und LFO auf Wet-Pfad anwenden
    const float dryWet = mDryWetParam ? mDryWetParam->load() : 0.7f;
    float* channelDataL = buffer.getWritePointer(0);
    float* channelDataR = totalNumOutputChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    const int readyL = mOutputFifoL.getNumReady();
    const int readyR = mOutputFifoR.getNumReady();
    const int samplesToRead = std::min({ numSamples, readyL, readyR });

    if (samplesToRead > 0) {
        int readStart1L, readSize1L, readStart2L, readSize2L;
        int readStart1R, readSize1R, readStart2R, readSize2R;

        mOutputFifoL.prepareToRead(samplesToRead, readStart1L, readSize1L, readStart2L, readSize2L);
        mOutputFifoR.prepareToRead(samplesToRead, readStart1R, readSize1R, readStart2R, readSize2R);

        for (int i = 0; i < samplesToRead; ++i) {
            const float dryL = channelDataL[i];
            const float dryR = channelDataR ? channelDataR[i] : dryL;

            const float rawWetL = (i < readSize1L) ? mOutputBufferL[readStart1L + i] : mOutputBufferL[readStart2L + (i - readSize1L)];
            const float rawWetR = (i < readSize1R) ? mOutputBufferR[readStart1R + i] : mOutputBufferR[readStart2R + (i - readSize1R)];

            const float lfoVal = mLfo.processSample(effectiveLfoFreq, lfoWaveform);
            const float lfoGain = 1.0f - lfoDepth * 0.5f * (1.0f - lfoVal);

            const float wetL = rawWetL * lfoGain;
            const float wetR = rawWetR * lfoGain;

            channelDataL[i] = (1.0f - dryWet) * dryL + dryWet * wetL;
            if (channelDataR) {
                channelDataR[i] = (1.0f - dryWet) * dryR + dryWet * wetR;
            }
        }

        mOutputFifoL.finishedRead(samplesToRead);
        mOutputFifoR.finishedRead(samplesToRead);

        for (int i = samplesToRead; i < numSamples; ++i) {
            channelDataL[i] *= (1.0f - dryWet);
            if (channelDataR) channelDataR[i] *= (1.0f - dryWet);
        }
    } else {
        for (int i = 0; i < numSamples; ++i) {
            channelDataL[i] *= (1.0f - dryWet);
            if (channelDataR) channelDataR[i] *= (1.0f - dryWet);
        }
    }
}

juce::AudioProcessorEditor* DDSPAudioProcessor::createEditor() {
    return new DDSPAudioProcessorEditor(*this);
}

void DDSPAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DDSPAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new DDSPAudioProcessor();
}