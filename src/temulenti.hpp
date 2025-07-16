#pragma once

namespace temulenti {
    static const std::vector<std::string> displayModels = {
        "T",
        "B",
        "D",
        "S"
    };

    static const std::vector<std::string> menuLabels = {
        "Tidal Modulator",
        "Two Bumps - Harmonic oscillator",
        "Two Drunks - Random walk",
        "Sheep - Wavetable synthesizer"
    };

    static const std::vector<std::string> quantizerLabels = {
        "Off",
        "Semitones",
        "Major",
        "Minor",
        "Whole tones",
        "Pentatonic Minor",
        "Poor pentatonic",
        "Fifths"
    };

    static const std::vector<std::string> bumpsModeLabels = {
        "Odd",
        "First 16",
        "Octaves"
    };

    static const std::vector<std::string> drunksModeLabels = {
        "Trigger",
        "Cycling",
        "Gate"
    };

    static const std::vector<std::string> modelRangeHeaders = {
    "Frequency range",
    "Sine quality"
    };
}