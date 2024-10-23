#pragma once

#include "apicescommon.hpp"

enum ApicesProcessorFunction {
    FUNCTION_ENVELOPE,
    FUNCTION_LFO,
    FUNCTION_TAP_LFO,
    FUNCTION_DRUM_GENERATOR,
    FUNCTION_MINI_SEQUENCER,
    FUNCTION_PULSE_SHAPER,
    FUNCTION_PULSE_RANDOMIZER,
    FUNCTION_FM_DRUM_GENERATOR,
    FUNCTION_NUMBER_STATION,
    FUNCTION_BOUNCING_BALL,
    FUNCTION_LAST
};

static const std::vector<std::string> apicesModeList{
    "ENVELOPE",
    "LFO",
    "TAP LFO",
    "DRUM GENERAT",
    "SEQUENCER*",
    "TRG. SHAPE*",
    "TRG. RANDOM*",
    "DIGI DRUMS*",
    "NUMBER STAT&",
    "BOUNCE BALL@"
};

static const std::vector<KnobLabels> apicesKnobLabelsSplitMode{
    { "1. Attack", "1. Decay", "2. Attack",  "2. Decay" },
    { "1. Frequency", "1. Waveform", "2. Frequency", "2. Waveform" },
    { "1. Waveform", "1. Wave. Var.", "2. Waveform", "2. Wave. Var." },
    { "1. BD Tone", "1. BD Decay", "2. SD Tone", "2. SD Snappy" },
    { "1. Step 1", "1. Step 2", "2. Step 1", "2. Step 2" },
    { "1. Delay", "1. Repeats #", "2. Delay", "2. Repeats #" },
    { "1. Acc/Rgn. Prob", "1. Delay", "2. Acc/Rgn. Prob", "2. Delay" },
    { "1. BD Morph", "1. BD Variation", "2. SD Morph", "2. SD Variation" },
    { "1. Frequency", "1. Var. Prob", "2. Frequency", "2. Var. Prob" },
    { "1. Gravity", "1. Bounce", "2. Gravity", "2. Bounce" }
};

static const std::vector<KnobLabels> apicesKnobLabelsTwinMode{
    { "Attack", "Decay", "Sustain", "Release" },
    { "Frequency", "Waveform", "Wave. Var", "Phase" },
    { "Amplitude", "Waveform", "Wave. Var", "Phase" },
    { "Base Freq", "Freq. Mod", "High Freq.", "Decay" },
    { "Step 1", "Step 2", "Step 3", "Step 4" },
    { "Pre-delay", "Gate time", "Delay", "Repeats #" },
    { "Trg. Prob.", "Regen Prob.", "Delay time", "Jitter" },
    { "Frequency", "FM Intens", "Env. Decay", "Color" },
    { "Frequency", "Var. Prob.", "Noise", "Distortion" },
    { "Gravity", "Bounce", "Amplitude", "Velocity" }
};

static const LightModes lightStates[FUNCTION_LAST][kNumFunctionLights]{
    { LIGHT_ON,  LIGHT_OFF, LIGHT_OFF, LIGHT_OFF }, // Envelope
    { LIGHT_OFF, LIGHT_ON, LIGHT_OFF, LIGHT_OFF }, // LFO
    { LIGHT_OFF, LIGHT_OFF, LIGHT_ON, LIGHT_OFF }, // TAP LFO
    { LIGHT_OFF, LIGHT_OFF, LIGHT_OFF, LIGHT_ON }, // DRUM GENERAT
    { LIGHT_BLINK, LIGHT_OFF, LIGHT_OFF, LIGHT_OFF }, // SEQUENCER
    { LIGHT_OFF, LIGHT_BLINK, LIGHT_OFF, LIGHT_OFF }, // TRG. SHAPE*
    { LIGHT_OFF, LIGHT_OFF, LIGHT_BLINK, LIGHT_OFF }, // TRG. RANDOM*
    { LIGHT_OFF, LIGHT_OFF, LIGHT_OFF, LIGHT_BLINK }, // DIGI DRUMS*
    { LIGHT_OFF, LIGHT_OFF, LIGHT_OFF, LIGHT_OFF }, // NUMBER STAT&
    { LIGHT_ON, LIGHT_OFF, LIGHT_BLINK, LIGHT_BLINK } // BOUNCE BALL@
};