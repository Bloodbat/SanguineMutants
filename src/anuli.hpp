#pragma once

using namespace sanguineCommonCode;

namespace anuli {
    static const std::vector<std::string> modeLabels = {
        "Modal Reso.",
        "Sym. Strings",
        "M. I. String",
        "FM voice",
        "Q. Sym. Str.",
        "Rev. String",
        "Disas. Peace"
    };

    static const std::vector<std::string> fxLabels = {
        "Formant filter",
        "Rolandish chorus",
        "Caveman reverb",
        "Formant filter (less abrasive variant)",
        "Solinaish ensemble",
        "Caveman reverb (shinier variant)"
    };

    static const std::vector<std::string> menuModeLabels = {
        "Modal resonator",
        "Sympathetic strings",
        "Modulated/inharmonic string",
        "FM voice",
        "Quantized sympathetic strings",
        "Reverb string",
        "Disastrous Peace"
    };

    static const std::vector<std::string> polyphonyLabels = {
        "1",
        "2",
        "3",
        "4"
    };

    static const int kMaxModes = 7;

    static const LightModes modeLights[kMaxModes][3] = {
        { LIGHT_OFF, LIGHT_ON, LIGHT_OFF },
        { LIGHT_ON, LIGHT_ON, LIGHT_OFF },
        { LIGHT_ON, LIGHT_OFF, LIGHT_OFF },
        { LIGHT_OFF, LIGHT_BLINK, LIGHT_OFF },
        { LIGHT_BLINK, LIGHT_BLINK, LIGHT_OFF },
        { LIGHT_BLINK, LIGHT_OFF, LIGHT_OFF },
        { LIGHT_OFF, LIGHT_OFF, LIGHT_BLINK }
    };

    static const int kMaxFxModes = 6;

    static const LightModes fxModeLights[kMaxFxModes][2] = {
        { LIGHT_ON, LIGHT_OFF },
        { LIGHT_ON, LIGHT_ON },
        { LIGHT_OFF, LIGHT_ON },
        { LIGHT_BLINK, LIGHT_OFF },
        { LIGHT_BLINK, LIGHT_BLINK },
        { LIGHT_OFF, LIGHT_BLINK }
    };

    static const std::vector<float> frequencyOffsets = {
        0.f,
        1.501f
    };

    static const int kBlockSize = 24;

    static const int kHardwareRate = 48000;

    static constexpr float kVoltPerOctave = 1.f / 12.f;
}