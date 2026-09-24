#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include "PitchTracker.hpp"
#include "LoudnessExtractor.hpp"
#include "HarmonicSynthesizer.hpp"

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
    std::cout << "[Test 1] Loudness 440 Hz Sinus (0 dBFS Peak): " << loudness << " (Erwartet: ~0.96)\n";
    assert(loudness > 0.90f && loudness <= 1.0f);

    // 2. Test: Pitch Tracker (440 Hz erkennen)
    PitchTracker pitchTracker(1024, sampleRate, 0.15f);
    float detectedPitch = 0.0f;
    for (size_t i = 0; i < 2048; ++i) {
        float s = std::sin(twoPi * 440.0f * (static_cast<float>(i) / sampleRate));
        float p = pitchTracker.processSample(s);
        if (p > 0.0f) {
            detectedPitch = p;
        }
    }
    std::cout << "[Test 2] Erkannte Grundfrequenz: " << detectedPitch << " Hz (Erwartet: 440.0 Hz +/- 2 Hz)\n";
    assert(std::abs(detectedPitch - 440.0f) < 2.0f);

    // 3. Test: Harmonic Synthesizer
    HarmonicSynthesizer synth(60, sampleRate);
    std::vector<float> synthOutput(blockSize, 0.0f);
    std::vector<float> amps(60, 0.0f);
    amps[0] = 1.0f; // Nur Grundton aktiv

    synth.processBlock(440.0f, 440.0f, amps.data(), amps.data(), synthOutput.data(), blockSize);
    float synthLoudness = loudnessExtractor.processBlock(synthOutput.data(), blockSize);
    std::cout << "[Test 3] Synthese Grundton Loudness: " << synthLoudness << " (Signal vorhanden)\n";
    assert(synthLoudness > 0.90f);

    std::cout << "Alle DSP-Tests erfolgreich bestanden.\n";
    return 0;
}
