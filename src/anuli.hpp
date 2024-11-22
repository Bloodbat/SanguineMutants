#pragma once

static const std::vector<std::string> anuliModeLabels = {
    "Modal Reso.",
    "Sym. Strings",
    "M. I. String",
    "FM voice",
    "Q. Sym. Str.",
    "Rev. String",
    "Disas. Peace"
};

static const std::vector<std::string> anuliFxLabels = {
    "Formant filter",
    "Rolandish chorus",
    "Caveman reverb",
    "Formant filter (less abrasive variant)",
    "Rolandish chorus (Solinaish ensemble)",
    "Caveman reverb (shinier variant)"
};

static const std::vector<std::string> anuliMenuLabels = {
    "Modal resonator",
    "Sympathetic strings",
    "Modulated/inharmonic string",
    "FM voice",
    "Quantized sympathetic strings",
    "Reverb string",
    "Disastrous Peace"
};

static const int kAnuliMaxModes = 7;

static const LightModes anuliModeLights[kAnuliMaxModes][3] = {
    { LIGHT_OFF, LIGHT_ON, LIGHT_OFF },
    { LIGHT_ON, LIGHT_ON, LIGHT_OFF },
    { LIGHT_ON, LIGHT_OFF, LIGHT_OFF },
    { LIGHT_OFF, LIGHT_BLINK, LIGHT_OFF },
    { LIGHT_BLINK, LIGHT_BLINK, LIGHT_OFF },
    { LIGHT_BLINK, LIGHT_OFF, LIGHT_OFF },
    { LIGHT_OFF, LIGHT_OFF, LIGHT_BLINK }
};

static const int kAnuliMaxFxModes = 6;

static const LightModes anuliFxModeLights[kAnuliMaxFxModes][2] = {
    { LIGHT_ON, LIGHT_OFF },
    { LIGHT_ON, LIGHT_ON },
    { LIGHT_OFF, LIGHT_ON },
    { LIGHT_BLINK, LIGHT_OFF },
    { LIGHT_BLINK, LIGHT_BLINK },
    { LIGHT_OFF, LIGHT_BLINK }
};

static const std::vector<float> anuliFrequencyOffsets = {
    0.f,
    1.501f
};

static const int kAnuliBlockSize = 24;

static const float kAnuliVoltPerOctave = 1.f / 12.f;