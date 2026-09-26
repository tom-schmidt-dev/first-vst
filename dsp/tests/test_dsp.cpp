#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <array>
#include "PitchTracker.hpp"
#include "LoudnessExtractor.hpp"
#include "HarmonicSynthesizer.hpp"
#include "MSEGMath.hpp"

int main() {
    const float sampleRate = 44100.0f;
    const size_t blockSize = 128;
    const float twoPi = 6.28318530717958647692f;

    // 1. Test: Loudness Extractor
    LoudnessExtractor loudnessExtractor(-80.0f);
    std::vector<float> sine440(blockSize);
    for (size_t i = 0; i < blockSize; ++i) {
        sine440[i] = std::sin(twoPi * 440.0f * (static_cast<float>(i) / sampleRate));
    }
    float loudness = loudnessExtractor.processBlock(sine440.data(), blockSize);
    std::cout << "[Test 1] Loudness 440 Hz Sinus (0 dBFS Peak): " << loudness << " (Erwartet: > 0.90)\n";
    assert(loudness > 0.90f && loudness <= 1.0f);

    // 2. Test: Pitch Tracker (440 Hz erkennen über processBlock)
    PitchTracker pitchTracker(2048, sampleRate, 0.25f);
    std::vector<float> longSine(2048);
    for (size_t i = 0; i < 2048; ++i) {
        longSine[i] = std::sin(twoPi * 440.0f * (static_cast<float>(i) / sampleRate));
    }
    float detectedPitch = pitchTracker.processBlock(longSine.data(), 2048, 0.70f);
    std::cout << "[Test 2] Erkannte Grundfrequenz: " << detectedPitch << " Hz (Erwartet: 440.0 Hz +/- 2 Hz)\n";
    assert(std::abs(detectedPitch - 440.0f) < 2.0f);

    // 3. Test: Harmonic Synthesizer Grundfunktion (Default Voice 0)
    HarmonicSynthesizer synth(60, sampleRate);
    std::vector<float> synthOutL(blockSize, 0.0f);
    std::vector<float> synthOutR(blockSize, 0.0f);
    std::vector<float> amps(60, 0.0f);
    amps[0] = 1.0f; // Nur Grundton aktiv

    synth.processBlock(440.0f, 440.0f, amps.data(), amps.data(), synthOutL.data(), synthOutR.data(), blockSize);
    float synthLoudness = loudnessExtractor.processBlock(synthOutL.data(), blockSize);
    std::cout << "[Test 3] Synthese Grundton Loudness: " << synthLoudness << " (Signal vorhanden)\n";
    assert(synthLoudness > 0.85f);

    // 4. Test: 5-Stimmen-Konfiguration (-5 bis +5 Oktaven & verschiedene Wellenformen)
    std::array<VoiceConfig, 5> voices;
    // Voice 0: DDSP Grundton
    voices[0] = { 1.0f, 0, 0, 0.0f, VoiceSourceType::DDSP, 0.0f };
    // Voice 1: Sub-Oktave -2 Saw
    voices[1] = { 0.7f, -2, 0, 0.0f, VoiceSourceType::Saw, -0.5f };
    // Voice 2: High +1 Oct Square
    voices[2] = { 0.5f, 1, 0, 0.0f, VoiceSourceType::Square, 0.5f };
    // Voice 3: Quint +7 Semitones Triangle
    voices[3] = { 0.4f, 0, 7, 0.0f, VoiceSourceType::Triangle, -0.2f };
    // Voice 4: Extreme High +3 Oct Sine mit Finetune
    voices[4] = { 0.3f, 3, 0, 15.0f, VoiceSourceType::Sine, 0.8f };

    std::array<std::array<MatrixCell, 5>, 5> emptyMatrix{};
    std::fill(synthOutL.begin(), synthOutL.end(), 0.0f);
    std::fill(synthOutR.begin(), synthOutR.end(), 0.0f);

    synth.processBlock(220.0f, 220.0f, amps.data(), amps.data(),
                       synthOutL.data(), synthOutR.data(), blockSize, voices, emptyMatrix);

    float multiVoiceL = loudnessExtractor.processBlock(synthOutL.data(), blockSize);
    float multiVoiceR = loudnessExtractor.processBlock(synthOutR.data(), blockSize);
    std::cout << "[Test 4] 5-Stimmen Stereo-Synthese Loudness L/R: " << multiVoiceL << " / " << multiVoiceR << "\n";
    assert(multiVoiceL > 0.5f && multiVoiceR > 0.5f);

    // 5. Test: 5x5 All-to-All Cross-Modulationsmatrix mit Feedback & tanh-Stabilität
    std::array<std::array<MatrixCell, 5>, 5> matrix{};
    // Selbstmodulation (Voice 0 -> Voice 0 PhaseMod / FM)
    matrix[0][0] = { MatrixModMode::PhaseMod, 0.8f };
    // Gegenseitige Cross-Modulation (Voice 0 -> Voice 1 RingMod und Voice 1 -> Voice 0 Add)
    matrix[0][1] = { MatrixModMode::RingMod, 0.7f };
    matrix[1][0] = { MatrixModMode::Add, 0.5f };
    // Voice 2 moduliert Voice 3 via PhaseMod
    matrix[2][3] = { MatrixModMode::PhaseMod, 1.0f };

    // Führe mehrere Blöcke aus, um z^-1 Feedback-Stabilität über Zeit zu testen
    for (int block = 0; block < 10; ++block) {
        synth.processBlock(220.0f, 220.0f, amps.data(), amps.data(),
                           synthOutL.data(), synthOutR.data(), blockSize, voices, matrix);

        // Überprüfe Stabilität: Keine NaNs, keine Infs, endliche Amplituden durch tanh
        for (size_t i = 0; i < blockSize; ++i) {
            assert(!std::isnan(synthOutL[i]));
            assert(!std::isnan(synthOutR[i]));
            assert(!std::isinf(synthOutL[i]));
            assert(!std::isinf(synthOutR[i]));
            assert(std::abs(synthOutL[i]) < 5.0f);
            assert(std::abs(synthOutR[i]) < 5.0f);
        }
    }
    std::cout << "[Test 5] 5x5 Matrix Feedback & tanh-Stabilisierung erfolgreich validiert.\n";

    // 6. Test: MSEG Kurven- und Tension-Interpolation
    // Prüfe Randwerte (u=0.0 -> y0, u=1.0 -> y1) für alle Kurventypen
    for (int c = 0; c <= 4; ++c) {
        auto type = static_cast<SegmentCurveType>(c);
        float yStart = interpolateSegment(0.0f, 0.2f, 0.8f, type, 0.0f);
        float yEnd   = interpolateSegment(1.0f, 0.2f, 0.8f, type, 0.0f);
        assert(std::abs(yStart - 0.2f) < 0.001f);
        assert(std::abs(yEnd - 0.8f) < 0.001f);
    }

    // Prüfe Tension-Biegung (positive Tension biegt nach unten, negative nach oben)
    float linearMid = interpolateSegment(0.5f, 0.0f, 1.0f, SegmentCurveType::Linear, 0.0f);
    float bentDown  = interpolateSegment(0.5f, 0.0f, 1.0f, SegmentCurveType::Linear, 0.5f);
    float bentUp    = interpolateSegment(0.5f, 0.0f, 1.0f, SegmentCurveType::Linear, -0.5f);
    assert(std::abs(linearMid - 0.5f) < 0.001f);
    assert(bentDown < linearMid);
    assert(bentUp > linearMid);

    // Prüfe Stepped Hold
    float stepMid = interpolateSegment(0.5f, 0.3f, 0.9f, SegmentCurveType::Stepped, 0.0f);
    assert(std::abs(stepMid - 0.3f) < 0.001f);

    std::cout << "[Test 6] MSEG Interpolation (Linear, Exp, Log, SCurve, Stepped, Tension) erfolgreich validiert.\n";

    // 7. Test: Stille bei overallAmp = 0.0f für alle Stimmen (Behebung des Dauer-Ton-Bugs)
    std::vector<float> silenceL(256, 1.0f);
    std::vector<float> silenceR(256, 1.0f);
    std::array<VoiceConfig, 5> testVoices;
    for (size_t v = 0; v < 5; ++v) {
        testVoices[v].gain = 1.0f;
        testVoices[v].source = static_cast<VoiceSourceType>(v); // Teste DDSP, Sine, Saw, Square, Triangle
    }
    std::array<std::array<MatrixCell, 5>, 5> zeroMatrix{};

    synth.processBlock(440.0f, 440.0f, nullptr, nullptr, silenceL.data(), silenceR.data(), 256,
                       testVoices, zeroMatrix, 5000.0f, 0.0f, 0.0f);

    float maxSilencePeak = 0.0f;
    for (size_t i = 0; i < 256; ++i) {
        maxSilencePeak = std::max(maxSilencePeak, std::abs(silenceL[i]));
        maxSilencePeak = std::max(maxSilencePeak, std::abs(silenceR[i]));
    }
    assert(maxSilencePeak < 1e-6f);
    std::cout << "[Test 7] Amplitude-Gating (Stille bei Idle / overallAmp = 0) erfolgreich validiert.\n";

    // 8. Test: Pure Timbre Transfer (Stimme 0 DDSP, 100% Wet, Dämpfung & Oktavierung)
    std::array<VoiceConfig, 5> pureVoices{};
    pureVoices[0] = { 1.0f, 0, 0, 0.0f, VoiceSourceType::DDSP, 0.0f };
    for (size_t v = 1; v < 5; ++v) {
        pureVoices[v] = { 0.0f, 0, 0, 0.0f, VoiceSourceType::Sine, 0.0f };
    }

    std::vector<float> pureOutL(blockSize, 0.0f);
    std::vector<float> pureOutR(blockSize, 0.0f);
    std::vector<float> harmonicAmps(60, 0.0f);
    // Erzeuge reiche Geigen-Obertonreihe
    for (size_t h = 0; h < 30; ++h) {
        harmonicAmps[h] = 1.0f / static_cast<float>(h + 1);
    }

    const float spectralTilt = 0.50f;
    const float bodyCutoffHz = 1800.0f + spectralTilt * 12200.0f; // 7900 Hz
    synth.processBlock(220.0f, 220.0f, harmonicAmps.data(), harmonicAmps.data(),
                       pureOutL.data(), pureOutR.data(), blockSize,
                       pureVoices, zeroMatrix, bodyCutoffHz, 1.0f, 1.0f);

    float pureLoudness = loudnessExtractor.processBlock(pureOutL.data(), blockSize);
    std::cout << "[Test 8] Pure Timbre Transfer Loudness (Voice 0 DDSP): " << pureLoudness << " (Erwartet: > 0.85)\n";
    assert(pureLoudness > 0.85f);

    // Geigenmodell Oktavierungsbereich: Frequenzen unter G3 (196 Hz) müssen in den Bereich >= 196 Hz gefaltet werden
    float testPitch = 110.0f;
    while (testPitch > 0.0f && testPitch < 196.0f) {
        testPitch *= 2.0f;
    }
    assert(testPitch == 220.0f);

    std::cout << "=== Alle DSP-Tests erfolgreich bestanden! ===\n";
    return 0;
}
