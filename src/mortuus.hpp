#pragma once

#include "apicescommon.hpp"

enum MortuusProcessorFunction {
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
    FUNCTION_BYTE_BEATS,
    FUNCTION_DUAL_ATTACK_ENVELOPE,
    FUNCTION_REPEATING_ATTACK_ENVELOPE,
    FUNCTION_LOOPING_ENVELOPE,
    FUNCTION_RANDOMISED_ENVELOPE,
    FUNCTION_RANDOMISED_DRUMS,
    FUNCTION_TURING_MACHINE,
    FUNCTION_MOD_SEQUENCER,
    FUNCTION_FM_LFO,
    FUNCTION_RANDOM_FM_LFO,
    FUNCTION_VARYING_WAVE_LFO,
    FUNCTION_RANDOM_VARYING_WAVE_LFO,
    FUNCTION_PHASE_LOCKED_OSCILLATOR,
    FUNCTION_RANDOM_HI_HAT,
    FUNCTION_CYMBAL,
    FUNCTION_LAST
};

static const std::vector<std::string> mortuusModeList{
    "ENVELOPE",
    "LFO",
    "TAP LFO",
    "BASS/SNARE",
    "D. ATK. ENV#",
    "R. ATK. ENV#",
    "LOOPING ENV#",
    "RANDOM ENV#",
    "BOUNCE BALL#",
    "FM LFO#",
    "RND. FM LFO#",
    "V. WAVE LFO#",
    "R.V.W. LFO#",
    "P.L.O#",
    "SEQUENCER*",
    "MOD SEQ.#",
    "TRG. SHAPE*",
    "TRG. RANDOM*",
    "TURING#",
    "BYTE BEATS#",
    "DIGI DRUMS*",
    "CYMBAL#",
    "RANDOM DRUM#",
    "RAND. HIHAT#",
    "NUMBER STAT&"
};

static const std::vector<KnobLabels> mortuusKnobLabelsSplitMode{
    { "1. Attack", "1. Decay", "2. Attack",  "2. Decay" },
    { "1. Frequency", "1. Waveform", "2. Frequency", "2. Waveform" },
    { "1. Waveform", "1. Wave. Var.", "2. Waveform", "2. Wave. Var." },
    { "1. BD Tone", "1. BD Decay", "2. SD Tone", "2. SD Snappy" },
    { "1. Attack", "1. Decay", "2. Attack", "2. Decay" },
    { "1. Attack", "1. Decay", "2. Attack", "2. Decay" },
    { "1. Attack", "1. Decay", "2. Attack", "2. Decay" },
    { "1. Attack", "1. Decay", "2. Attack", "2. Decay" },
    { "1. Gravity", "1. Bounce", "2. Gravity", "2. Bounce" },
    { "1. Frequency", "1. Waveform", "2. Frequency", "2. Waveform" },
    { "1. Frequency", "1. Waveform", "2. Frequency", "2. Waveform" },
    { "1. Frequency", "1. Waveform", "2. Frequency", "2. Waveform" },
    { "1. Frequency", "1. Waveform", "2. Frequency", "2. Waveform" },
    { "1. Freq. Div.", "1. LFO Wave.", "2. Freq. Div.", "2. LFO Wave." },
    { "1. Step 1", "1. Step 2", "2. Step 1", "2. Step 2" },
    { "1. Step 1/3", "1. Step 2/4", "2. Step 1/3", "2. Step 2/4" },
    { "1. Delay", "1. Repeats #", "2. Delay", "2. Repeats #" },
    { "1. Acc/Rgn. Prob", "1. Delay", "2. Acc/Rgn. Prob", "2. Delay" },
    { "1. Probability", "1. Span", "2. Probability", "2. Span" },
    { "1. Pitch", "1. Parameter 0", "2. Pitch", "2. Parameter 0" },
    { "1. BD Morph", "1. BD Variation", "2. SD Morph", "2. SD Variation" },
    { "1. Crossfade", "1. Decay", "2. Crossfade", "2. Decay" },
    { "1. BD Pitch", "1. BD Tone/Decay", "2. SD Pitch", "2. SD Decay/Snap" },
    { "1. Pitch", "1. Tone", "2. Pitch", "2. Decay" },
    { "1. Frequency", "1. Var. Prob", "2. Frequency", "2. Var. Prob" }
};

static const std::vector<KnobLabels> mortuusKnobLabelsTwinMode{
    { "Attack", "Decay", "Sustain", "Release" },
    { "Frequency", "Waveform", "Wave. Var", "Phase" },
    { "Amplitude", "Waveform", "Wave. Var", "Phase" },
    { "Base Freq", "Freq. Mod", "High Freq.", "Decay" },
    { "Attack", "Decay", "Sustain", "Release" },
    { "Attack", "Decay", "Sustain", "Release" },
    { "Attack", "Decay", "Sustain", "Release" },
    { "Attack", "Decay", "Attack Var.", "Decay Var." },
    { "Gravity", "Bounce", "Amplitude", "Velocity" },
    { "Frequency", "Waveform", "FM Freq.", "FM Depth" },
    { "Frequency", "Waveform", "FM. Freq.", "FM. Depth" },
    { "Frequency", "Waveform", "WS. Freq.", "WS. Depth" },
    { "Frequency", "Waveform", "WS. Freq.", "WS. Depth" },
    { "Freq. Div", "LFO Wave", "WS. Freq.", "WS. Depth" },
    { "Step 1", "Step 2", "Step 3", "Step 4" },
    { "Step 1/5", "Step 2/6", "Step 3/7", "Step 4/8" },
    { "Pre-delay", "Gate time", "Delay", "Repeats #" },
    { "Trg. Prob.", "Regen Prob.", "Delay time", "Jitter" },
    { "Probability", "Span", "Length", "Clock Div." },
    { "Pitch", "Parameter 0", "Parameter 1", "Equation" },
    { "Frequency", "FM Intens", "Env. Decay", "Color" },
    { "Pitch", "Clip", "Crossfade", "Decay" },
    { "Pitch", "Tone/Decay", "Rand. Pitch", "Rand. Hit" },
    { "Pitch", "Decay", "Rand. Pitch", "Rand. Decay"},
    { "Frequency", "Var. Prob.", "Noise", "Distortion" }
};

static const LightModes lightStates[FUNCTION_LAST][4]{
    { LIGHT_ON,  LIGHT_OFF, LIGHT_OFF, LIGHT_OFF }, // Envelope
    { LIGHT_OFF, LIGHT_ON, LIGHT_OFF, LIGHT_OFF }, // LFO
    { LIGHT_OFF, LIGHT_OFF, LIGHT_ON, LIGHT_OFF }, // TAP LFO
    { LIGHT_OFF, LIGHT_OFF, LIGHT_OFF, LIGHT_ON }, // DRUM GENERAT
    { LIGHT_ON, LIGHT_BLINK, LIGHT_OFF, LIGHT_OFF }, // D. ATK. ENV#
    { LIGHT_ON, LIGHT_OFF, LIGHT_BLINK, LIGHT_OFF }, // R. ATK. ENV#
    { LIGHT_ON, LIGHT_OFF, LIGHT_OFF, LIGHT_BLINK }, // LOOPING ENV#
    { LIGHT_ON, LIGHT_BLINK, LIGHT_BLINK, LIGHT_OFF }, // RANDOM ENV#
    { LIGHT_ON, LIGHT_OFF, LIGHT_BLINK, LIGHT_BLINK }, // BOUNCE BALL#
    { LIGHT_OFF, LIGHT_ON, LIGHT_BLINK, LIGHT_OFF }, // FM LFO#
    { LIGHT_BLINK, LIGHT_ON, LIGHT_BLINK, LIGHT_OFF }, // RND. FM LFO#
    { LIGHT_OFF, LIGHT_ON, LIGHT_OFF, LIGHT_BLINK }, // V. WAVE LFO#
    { LIGHT_BLINK, LIGHT_ON, LIGHT_OFF, LIGHT_BLINK }, // R.V.W. LFO#
    { LIGHT_OFF, LIGHT_ON, LIGHT_BLINK, LIGHT_BLINK }, // P.L.O#
    { LIGHT_BLINK, LIGHT_OFF, LIGHT_ON, LIGHT_OFF }, // SEQUENCER*
    { LIGHT_OFF, LIGHT_BLINK, LIGHT_ON, LIGHT_OFF }, // MOD SEQ.#
    { LIGHT_OFF, LIGHT_OFF, LIGHT_ON, LIGHT_BLINK }, // TRG. SHAPE#
    { LIGHT_OFF, LIGHT_BLINK, LIGHT_ON, LIGHT_BLINK }, // TRG. RANDOM#
    { LIGHT_BLINK, LIGHT_OFF, LIGHT_ON, LIGHT_BLINK }, // TURING#
    { LIGHT_BLINK, LIGHT_BLINK, LIGHT_ON, LIGHT_BLINK }, // BYTE BEATS#
    { LIGHT_BLINK, LIGHT_OFF, LIGHT_OFF, LIGHT_ON }, // DIGI DRUMS*
    { LIGHT_OFF, LIGHT_BLINK, LIGHT_OFF, LIGHT_ON }, // CYMBAL#
    { LIGHT_BLINK, LIGHT_BLINK, LIGHT_OFF, LIGHT_ON }, // RANDOM DRUM#
    { LIGHT_OFF, LIGHT_OFF, LIGHT_BLINK, LIGHT_ON }, // RAND. HIHAT#
    { LIGHT_OFF, LIGHT_OFF, LIGHT_OFF, LIGHT_OFF } // NUMBER STAT&
};