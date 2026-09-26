#include "../source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

struct AudioStats {
    float peakLinear = 0.0f;
    float peakDb = -100.0f;
    float rmsLinear = 0.0f;
    float rmsDb = -100.0f;
};

AudioStats analyzeBuffer(const juce::AudioBuffer<float>& buffer) {
    AudioStats stats;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || numChannels == 0) return stats;

    double sumSquares = 0.0;
    float maxVal = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch) {
        const float* readPtr = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            const float val = std::abs(readPtr[i]);
            if (val > maxVal) maxVal = val;
            sumSquares += static_cast<double>(readPtr[i]) * static_cast<double>(readPtr[i]);
        }
    }

    stats.peakLinear = maxVal;
    stats.peakDb = (maxVal > 1e-5f) ? (20.0f * std::log10(maxVal)) : -100.0f;

    double meanSquare = sumSquares / (static_cast<double>(numChannels * numSamples));
    stats.rmsLinear = static_cast<float>(std::sqrt(meanSquare));
    stats.rmsDb = (stats.rmsLinear > 1e-5f) ? (20.0f * std::log10(stats.rmsLinear)) : -100.0f;

    return stats;
}

bool writeWavFile(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate) {
    if (file.exists()) file.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(new juce::FileOutputStream(file),
                                  sampleRate,
                                  static_cast<unsigned int>(buffer.getNumChannels()),
                                  16,
                                  {},
                                  0)
    );

    if (writer != nullptr) {
        return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    }
    return false;
}

bool testProcessFile(juce::AudioFormatManager& formatMgr,
                     const juce::File& inputFile,
                     const juce::File& outputFile,
                     const std::string& testTitle,
                     double sampleRate,
                     int blockSize) {
    std::cout << testTitle << "\n";
    if (!inputFile.existsAsFile()) {
        std::cout << "  [SKIP] Datei nicht gefunden: " << inputFile.getFullPathName() << "\n\n";
        return true;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(inputFile));
    if (!reader) {
        std::cout << "  [FAIL] Konnte AudioFormatReader nicht erstellen!\n\n";
        return false;
    }

    const int numSamples = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> inputSignal(2, numSamples);
    inputSignal.clear();
    reader->read(&inputSignal, 0, numSamples, 0, true, true);

    DDSPAudioProcessor processor;
    processor.setNonRealtime(true);
    processor.prepareToPlay(sampleRate, blockSize);

    if (auto* dw = processor.apvts.getParameter("dry_wet")) {
        dw->setValueNotifyingHost(dw->convertTo0to1(1.0f));
    }

    juce::AudioBuffer<float> outputSignal(2, numSamples);
    outputSignal.clear();
    juce::AudioBuffer<float> blockBuf(2, blockSize);
    juce::MidiBuffer midi;

    for (int offset = 0; offset + blockSize <= numSamples; offset += blockSize) {
        for (int ch = 0; ch < 2; ++ch) {
            blockBuf.copyFrom(ch, 0, inputSignal, ch, offset, blockSize);
        }
        processor.processBlock(blockBuf, midi);
        for (int ch = 0; ch < 2; ++ch) {
            outputSignal.copyFrom(ch, offset, blockBuf, ch, 0, blockSize);
        }
    }

    writeWavFile(outputFile, outputSignal, sampleRate);
    auto stats = analyzeBuffer(outputSignal);
    std::cout << "  -> Eingangsdatei: " << inputFile.getFileName() << " (" << std::fixed << std::setprecision(2) << (static_cast<double>(numSamples) / sampleRate) << " s)\n";
    std::cout << "  -> Gespeichert in: " << outputFile.getFullPathName() << "\n";
    std::cout << "  -> Resynthese Peak: " << stats.peakDb << " dBFS, RMS: " << stats.rmsDb << " dBFS\n";

    if (stats.rmsLinear > 0.005f) {
        std::cout << "  [PASS] Erfolgreich mit DDSP-Geigentimbre resynthetisiert.\n\n";
        processor.releaseResources();
        return true;
    } else {
        std::cout << "  [FAIL] Ausgangssignal zu leise!\n\n";
        processor.releaseResources();
        return false;
    }
}

int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    const double sampleRate = 44100.0;
    const int blockSize = 128;
    const double twoPi = 6.28318530717958647692;

    juce::AudioFormatManager formatMgr;
    formatMgr.registerBasicFormats();

    juce::File outputDir = juce::File::getCurrentWorkingDirectory().getChildFile("test_renders");
    outputDir.createDirectory();

    std::cout << "\n======================================================\n";
    std::cout << "  DDSP Timbre Transfer - Automatisierter Audio-Test    \n";
    std::cout << "======================================================\n\n";

    // -------------------------------------------------------------------------
    // Test 1: Idle Silence Test (Kein Eingangssignal / Null-Signal)
    // -------------------------------------------------------------------------
    std::cout << "[Test 1] Leerlauf-Stille Test (Idle Silence)...\n";
    {
        DDSPAudioProcessor processor;
        processor.setNonRealtime(true);
        processor.prepareToPlay(sampleRate, blockSize);

        const int numSeconds = 1;
        const int totalSamples = static_cast<int>(numSeconds * sampleRate);
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> recorded(2, totalSamples);
        recorded.clear();

        for (int offset = 0; offset + blockSize <= totalSamples; offset += blockSize) {
            buffer.clear();
            processor.processBlock(buffer, midi);
            float blockMax = 0.0f;
            for (int ch = 0; ch < 2; ++ch) {
                for (int s = 0; s < blockSize; ++s) {
                    blockMax = std::max(blockMax, std::abs(buffer.getSample(ch, s)));
                }
                recorded.copyFrom(ch, offset, buffer, ch, 0, blockSize);
            }
            if (blockMax > 1e-4f) {
                std::cout << "    [Non-zero block at offset " << offset << "]: peak = " << blockMax << "\n";
            }
        }

        auto stats = analyzeBuffer(recorded);
        std::cout << "  -> Peak: " << stats.peakDb << " dBFS, RMS: " << stats.rmsDb << " dBFS\n";
        if (stats.peakLinear < 1e-4f) {
            std::cout << "  [PASS] Plugin ist bei Inaktivitaet absolut still (kein Dauerton).\n\n";
        } else {
            std::cout << "  [FAIL] Unerwarteter Ton im Leerlauf erkannt!\n\n";
            return 1;
        }

        processor.releaseResources();
    }

    // -------------------------------------------------------------------------
    // Test 2: Sinus-Glissando mit Vibrato (Vocal/Pfeifton Simulation) -> 100% Wet
    // -------------------------------------------------------------------------
    std::cout << "[Test 2] Sinus-Glissando (220 Hz bis 440 Hz mit Vibrato) -> 100% Wet...\n";
    {
        DDSPAudioProcessor processor;
        processor.setNonRealtime(true);
        processor.prepareToPlay(sampleRate, blockSize);

        if (auto* dw = processor.apvts.getParameter("dry_wet")) {
            dw->setValueNotifyingHost(dw->convertTo0to1(1.0f));
        }

        const int numSeconds = 3;
        const int totalSamples = static_cast<int>(numSeconds * sampleRate);
        juce::AudioBuffer<float> inputSignal(2, totalSamples);
        inputSignal.clear();
        juce::AudioBuffer<float> outputSignal(2, totalSamples);
        outputSignal.clear();

        double phase = 0.0;
        for (int i = 0; i < totalSamples; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            double baseFreq = 220.0 + (440.0 - 220.0) * (t / static_cast<double>(numSeconds));
            double vibrato = std::sin(twoPi * 5.0 * t) * 15.0;
            double freq = baseFreq + vibrato;

            phase += twoPi * freq / sampleRate;
            if (phase >= twoPi) phase -= twoPi;

            float env = 0.8f;
            if (t < 0.1) env = static_cast<float>(t / 0.1) * 0.8f;
            if (t > numSeconds - 0.2) env = static_cast<float>((numSeconds - t) / 0.2) * 0.8f;

            float s = static_cast<float>(std::sin(phase)) * env;
            inputSignal.setSample(0, i, s);
            inputSignal.setSample(1, i, s);
        }

        juce::AudioBuffer<float> blockBuf(2, blockSize);
        juce::MidiBuffer midi;

        for (int offset = 0; offset + blockSize <= totalSamples; offset += blockSize) {
            for (int ch = 0; ch < 2; ++ch) {
                blockBuf.copyFrom(ch, 0, inputSignal, ch, offset, blockSize);
            }
            processor.processBlock(blockBuf, midi);
            for (int ch = 0; ch < 2; ++ch) {
                outputSignal.copyFrom(ch, offset, blockBuf, ch, 0, blockSize);
            }
        }

        juce::File wavFile = outputDir.getChildFile("test_2_sine_glissando_to_violin.wav");
        writeWavFile(wavFile, outputSignal, sampleRate);

        auto stats = analyzeBuffer(outputSignal);
        std::cout << "  -> Gespeichert in: " << wavFile.getFullPathName() << "\n";
        std::cout << "  -> Resynthese Peak: " << stats.peakDb << " dBFS, RMS: " << stats.rmsDb << " dBFS\n";

        if (stats.rmsLinear > 0.05f) {
            std::cout << "  [PASS] Geigen-Resynthese erzeugt kraeftiges, dynamisches Ausgangssignal.\n\n";
        } else {
            std::cout << "  [FAIL] Signal zu leise oder nicht resynthetisiert!\n\n";
            return 1;
        }

        processor.releaseResources();
    }

    // -------------------------------------------------------------------------
    // Test 3: Reales Audio-Sample als Input (data/raw/violin/...)
    // -------------------------------------------------------------------------
    std::cout << "[Test 3] Reales Audio-Sample als Input-Signal (100% Wet Resynthese)...\n";
    {
        juce::File sampleFile("/home/tom/Projekte/ddsp-timbre-vst/data/raw/violin/ordinario/Vn-ord-A4-ff-2c-N.wav");
        if (!sampleFile.existsAsFile()) {
            sampleFile = juce::File("/home/tom/Projekte/ddsp-timbre-vst/data/raw/violin/ordinario/Vn-ord-A#4-ff-2c-N.wav");
        }

        if (sampleFile.existsAsFile()) {
            std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(sampleFile));
            if (reader != nullptr) {
                const int numSamples = static_cast<int>(reader->lengthInSamples);
                juce::AudioBuffer<float> inputSignal(2, numSamples);
                reader->read(&inputSignal, 0, numSamples, 0, true, true);

                DDSPAudioProcessor processor;
                processor.setNonRealtime(true);
                processor.prepareToPlay(sampleRate, blockSize);

                if (auto* dw = processor.apvts.getParameter("dry_wet")) {
                    dw->setValueNotifyingHost(dw->convertTo0to1(1.0f));
                }

                juce::AudioBuffer<float> outputSignal(2, numSamples);
                outputSignal.clear();
                juce::AudioBuffer<float> blockBuf(2, blockSize);
                juce::MidiBuffer midi;

                for (int offset = 0; offset + blockSize <= numSamples; offset += blockSize) {
                    for (int ch = 0; ch < 2; ++ch) {
                        blockBuf.copyFrom(ch, 0, inputSignal, ch, offset, blockSize);
                    }
                    processor.processBlock(blockBuf, midi);
                    for (int ch = 0; ch < 2; ++ch) {
                        outputSignal.copyFrom(ch, offset, blockBuf, ch, 0, blockSize);
                    }
                }

                juce::File wavFile = outputDir.getChildFile("test_3_real_sample_to_violin.wav");
                writeWavFile(wavFile, outputSignal, sampleRate);

                auto stats = analyzeBuffer(outputSignal);
                std::cout << "  -> Eingangsdatei: " << sampleFile.getFileName() << " (" << numSamples << " Samples)\n";
                std::cout << "  -> Gespeichert in: " << wavFile.getFullPathName() << "\n";
                std::cout << "  -> Resynthese Peak: " << stats.peakDb << " dBFS, RMS: " << stats.rmsDb << " dBFS\n";

                if (stats.rmsLinear > 0.05f) {
                    std::cout << "  [PASS] Echtes Eingangssignal erfolgreich mit Geigentimbre resynthetisiert.\n\n";
                } else {
                    std::cout << "  [FAIL] Resynthese des echten Samples fehlgeschlagen!\n\n";
                    return 1;
                }

                processor.releaseResources();
            }
        } else {
            std::cout << "  [SKIP] Sample-Datei nicht gefunden.\n\n";
        }
    }

    // -------------------------------------------------------------------------
    // Test 4: Sample Drag & Drop Playback (Sequencer loadSampleFile + MIDI Note)
    // -------------------------------------------------------------------------
    std::cout << "[Test 4] Drag & Drop Sample-Resynthese (Sequencer + MIDI Note)...\n";
    {
        juce::File sampleFile("/home/tom/Projekte/ddsp-timbre-vst/data/raw/violin/ordinario/Vn-ord-A4-ff-2c-N.wav");
        if (sampleFile.existsAsFile()) {
            DDSPAudioProcessor processor;
            processor.setNonRealtime(true);
            processor.prepareToPlay(sampleRate, blockSize);

            bool loaded = processor.getSequencer().loadSampleFile(sampleFile, formatMgr);
            std::cout << "  -> Sample geladen: " << (loaded ? "Erfolgreich" : "Fehlgeschlagen") << "\n";

            if (auto* dw = processor.apvts.getParameter("dry_wet")) {
                dw->setValueNotifyingHost(dw->convertTo0to1(1.0f));
            }

            const int totalSamples = static_cast<int>(2.0 * sampleRate);
            juce::AudioBuffer<float> outputSignal(2, totalSamples);
            outputSignal.clear();
            juce::AudioBuffer<float> blockBuf(2, blockSize);
            juce::MidiBuffer midi;

            midi.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);

            for (int offset = 0; offset + blockSize <= totalSamples; offset += blockSize) {
                blockBuf.clear();
                processor.processBlock(blockBuf, midi);
                midi.clear();
                for (int ch = 0; ch < 2; ++ch) {
                    outputSignal.copyFrom(ch, offset, blockBuf, ch, 0, blockSize);
                }
            }

            juce::File wavFile = outputDir.getChildFile("test_4_drag_drop_sample_playback.wav");
            writeWavFile(wavFile, outputSignal, sampleRate);

            auto stats = analyzeBuffer(outputSignal);
            std::cout << "  -> Gespeichert in: " << wavFile.getFullPathName() << "\n";
            std::cout << "  -> Drag&Drop Resynthese Peak: " << stats.peakDb << " dBFS, RMS: " << stats.rmsDb << " dBFS\n";

            if (stats.rmsLinear > 0.05f) {
                std::cout << "  [PASS] Gedropptes Sample wird via MIDI getriggert und sauber resynthetisiert.\n\n";
            } else {
                std::cout << "  [FAIL] Drag & Drop Playback bleibt stumm!\n\n";
                return 1;
            }

            processor.releaseResources();
        }
    }

    // -------------------------------------------------------------------------
    // Test 5: Reale Gesangsspur (Vocal Hook aus Benutzer-Sample-Ordner)
    // -------------------------------------------------------------------------
    {
        juce::File vocalFile("/run/media/tom/Samsung QVO 1TB/Projekte/Mukke/Samples/unsortier/Vocal Roads Arabian Vocal Hooks/VR_VOCAL_STEMS/VR_110_Em_Lil_Boy/VR_AVH_110_Em_Lil_Boy_Vocal_01.wav");
        juce::File outWav = outputDir.getChildFile("user_vocal_to_violin.wav");
        if (!testProcessFile(formatMgr, vocalFile, outWav,
                             "[Test 5] Benutzer-Vocal-Hook (Acapella Gesang -> Geigentimbre)...",
                             sampleRate, blockSize)) {
            return 1;
        }
    }

    // -------------------------------------------------------------------------
    // Test 6: Reale Gitarrenmelodie (Guitar Loop aus Benutzer-Sample-Ordner)
    // -------------------------------------------------------------------------
    {
        juce::File guitarFile("/run/media/tom/Samsung QVO 1TB/Projekte/Mukke/Samples/unsortier/Cymatics - Millenium Sample Pack/Melody Loops/Dry Guitar Loops/Cymatics - Millenium Guitar Loop 1 - 135 BPM D Maj.wav");
        juce::File outWav = outputDir.getChildFile("user_guitar_to_violin.wav");
        if (!testProcessFile(formatMgr, guitarFile, outWav,
                             "[Test 6] Benutzer-Gitarrenloop (Akustik/E-Gitarre -> Geigentimbre)...",
                             sampleRate, blockSize)) {
            return 1;
        }
    }

    // -------------------------------------------------------------------------
    // Test 7: Elektronischer Synth-Lead (Hard Trap Synth Lead Loop)
    // -------------------------------------------------------------------------
    {
        juce::File synthFile("/run/media/tom/Samsung QVO 1TB/Projekte/Mukke/Samples/unsortier/Audentity Records - Hard Trap 5/kit4_Gminor_140bpm/Synth Loops/HT5_Kit4_lead_loop1_140_Gminor.wav");
        juce::File outWav = outputDir.getChildFile("user_synth_to_violin.wav");
        if (!testProcessFile(formatMgr, synthFile, outWav,
                             "[Test 7] Benutzer-Synth-Lead (Synthesizer Melodie -> Geigentimbre)...",
                             sampleRate, blockSize)) {
            return 1;
        }
    }

    std::cout << "======================================================\n";
    std::cout << "  ALLE AUDIO-TESTS ERFOLGREICH BESTANDEN!              \n";
    std::cout << "  Alle gerenderten Test-Audio-Dateien befinden sich in:\n";
    std::cout << "  " << outputDir.getFullPathName() << "\n";
    std::cout << "======================================================\n\n";

    return 0;
}
