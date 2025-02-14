#pragma once

#include "sanguinecomponents.hpp"

enum DejaVuLockModes {
    DEJA_VU_LOCK_ON,
    DEJA_VU_LOCK_OFF,
    DEJA_VU_SUPER_LOCK
};

static const int kBlockSize = 5;
static const int kMaxTModes = 7;
static const int kMaxScales = 6;
static const int kLightColorPins = 2;
static const int kMaxClockSourceTypes = 4;

static const marbles::Scale presetScales[kMaxScales] = {
    // C major
    {
        1.0f,
        12,
        {
            { 0.0000f, 255 },  // C
            { 0.0833f, 16 },   // C#
            { 0.1667f, 96 },   // D
            { 0.2500f, 24 },   // D#
            { 0.3333f, 128 },  // E
            { 0.4167f, 64 },   // F
            { 0.5000f, 8 },    // F#
            { 0.5833f, 192 },  // G
            { 0.6667f, 16 },   // G#
            { 0.7500f, 96 },   // A
            { 0.8333f, 24 },   // A#
            { 0.9167f, 128 },  // B
        }
    },
    // C minor
    {
        1.0f,
        12,
        {
            { 0.0000f, 255 },  // C
            { 0.0833f, 16 },   // C#
            { 0.1667f, 96 },   // D
            { 0.2500f, 128 },  // Eb
            { 0.3333f, 8 },    // E
            { 0.4167f, 64 },   // F
            { 0.5000f, 4 },    // F#
            { 0.5833f, 192 },  // G
            { 0.6667f, 96 },   // G#
            { 0.7500f, 16 },   // A
            { 0.8333f, 128 },  // Bb
            { 0.9167f, 16 },   // B
        }
    },

    // Pentatonic
    {
        1.0f,
        12,
        {
            { 0.0000f, 255 },  // C
            { 0.0833f, 4 },    // C#
            { 0.1667f, 96 },   // D
            { 0.2500f, 4 },    // Eb
            { 0.3333f, 4 },    // E
            { 0.4167f, 140 },  // F
            { 0.5000f, 4 },    // F#
            { 0.5833f, 192 },  // G
            { 0.6667f, 4 },    // G#
            { 0.7500f, 96 },   // A
            { 0.8333f, 4 },    // Bb
            { 0.9167f, 4 },    // B
        }
    },

    // Pelog
    {
        1.0f,
        7,
        {
            { 0.0000f, 255 },  // C
            { 0.1275f, 128 },  // Db+
            { 0.2625f, 32 },  // Eb-
            { 0.4600f, 8 },    // F#-
            { 0.5883f, 192 },  // G
            { 0.7067f, 64 },  // Ab
            { 0.8817f, 16 },    // Bb+
        }
    },

    // Raag Bhairav That
    {
        1.0f,
        12,
        {
            { 0.0000f, 255 }, // ** Sa
            { 0.0752f, 128 }, // ** Komal Re
            { 0.1699f, 4 },   //    Re
            { 0.2630f, 4 },   //    Komal Ga
            { 0.3219f, 128 }, // ** Ga
            { 0.4150f, 64 },  // ** Ma
            { 0.4918f, 4 },   //    Tivre Ma
            { 0.5850f, 192 }, // ** Pa
            { 0.6601f, 64 },  // ** Komal Dha
            { 0.7549f, 4 },   //    Dha
            { 0.8479f, 4 },   //    Komal Ni
            { 0.9069f, 64 },  // ** Ni
        }
    },

    // Raag Shri
    {
        1.0f,
        12,
        {
            { 0.0000f, 255 }, // ** Sa
            { 0.0752f, 4 },   //    Komal Re
            { 0.1699f, 128 }, // ** Re
            { 0.2630f, 64 },  // ** Komal Ga
            { 0.3219f, 4 },   //    Ga
            { 0.4150f, 128 }, // ** Ma
            { 0.4918f, 4 },   //    Tivre Ma
            { 0.5850f, 192 }, // ** Pa
            { 0.6601f, 4 },   //    Komal Dha
            { 0.7549f, 64 },  // ** Dha
            { 0.8479f, 128 }, // ** Komal Ni
            { 0.9069f, 4 },   //    Ni
        }
    },
};

static const marbles::Ratio y_divider_ratios[] = {
    { 1, 64 },
    { 1, 48 },
    { 1, 32 },
    { 1, 24 },
    { 1, 16 },
    { 1, 12 },
    { 1, 8 },
    { 1, 6 },
    { 1, 4 },
    { 1, 3 },
    { 1, 2 },
    { 1, 1 },
};

static const LightModes tModeLights[kMaxTModes][kLightColorPins]{
    { LIGHT_ON,  LIGHT_OFF }, // T_GENERATOR_MODEL_COMPLEMENTARY_BERNOULLI
    { LIGHT_ON, LIGHT_ON }, // T_GENERATOR_MODEL_CLUSTERS
    { LIGHT_OFF, LIGHT_ON }, // T_GENERATOR_MODEL_DRUMS
    { LIGHT_BLINK_SLOW, LIGHT_OFF }, // T_GENERATOR_MODEL_INDEPENDENT_BERNOULLI
    { LIGHT_BLINK_SLOW, LIGHT_BLINK_SLOW }, // T_GENERATOR_MODEL_DIVIDER
    { LIGHT_OFF, LIGHT_BLINK_SLOW }, // T_GENERATOR_MODEL_THREE_STATES
    { LIGHT_OFF, LIGHT_BLINK_FAST }, // T_GENERATOR_MODEL_MARKOV
};

static const LightModes scaleLights[kMaxScales][kLightColorPins]{
    { LIGHT_BLINK_SLOW,  LIGHT_OFF }, // Major
    { LIGHT_BLINK_SLOW, LIGHT_BLINK_SLOW }, // Minor
    { LIGHT_OFF, LIGHT_BLINK_SLOW }, // Pentatonic
    { LIGHT_BLINK_FAST, LIGHT_OFF }, // Pelog
    { LIGHT_BLINK_FAST, LIGHT_BLINK_FAST }, // Raag Bhairav That
    { LIGHT_OFF, LIGHT_BLINK_FAST } // Raag Shri
};

static const RGBLightColor paletteMarmoraClockSource[kMaxClockSourceTypes]{
    { 0.f, 0.f, 0.5f },
    { 0.f, 0.5f, 0.f },
    { 0.5f, 0.5f, 0.f },
    { 0.5f, 0.f, 0.f }
};

static const std::vector<std::string> marmoraTModeLabels = {
    "Complementary Bernoulli",
    "Clusters",
    "Drums",
    "Independent Bernoulli",
    "Divider",
    "Three states",
    "Markov"
};

static const std::vector<std::string> marmoraTRangeLabels = {
    "1/4x",
    "1x",
    "4x"
};

static const std::vector<std::string> marmoraXModeLabels = {
    "Identical",
    "Bump",
    "Tilt"
};

static const std::vector<std::string> marmoraXRangeLabels = {
    "Narrow (0V - 2V)",
    "Positive (0V - +5V)",
    "Full (-5V - +5V)"
};

static const std::vector<std::string> marmoraScaleLabels = {
    "Major",
    "Minor",
    "Pentatonic",
    "Gamelan (Pelog)",
    "Raag Bhairav That",
    "Raag Shri"
};

static const std::vector<std::string> marmoraInternalClockLabels = {
    "t₁ → X₁, T₂ → X₂, T₃ → X₃",
    "t₁ → X₁, X₂, X₃",
    "t₂ → X₁, X₂, X₃",
    "t₃ → X₁, X₂, X₃",
};

static const std::vector<std::string> marmoraLockLabels = {
    "Unlocked",
    "Locked"
};

static const int marmoraLoopLength[] = {
            1, 1, 1, 2, 2,
            2, 2, 2, 3, 3,
            3, 3, 4, 4, 4,
            4, 4, 5, 5, 6,
            6, 6, 7, 7, 8,
            8, 8, 10, 10, 12,
            12, 12, 14, 14, 16,
            16
};