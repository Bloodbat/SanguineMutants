#pragma once

#include "cloudycommon.hpp"

namespace nebulae {
    static const std::vector<cloudyCommon::ModeInfo> modeList{
        { "GRANULAR", "Granular mode" },
        { "STRETCH", "Pitch shifter/time stretcher" },
        { "LOOPING DLY", "Looping delay" },
        { "SPECTRAL", "Spectral madness" }
    };

    struct ModeDisplay {
        std::string labelFreeze;
        std::string labelPosition;
        std::string labelDensity;
        std::string labelSize;
        std::string labelTexture;
        std::string labelPitch;
        std::string labelTrigger;
    };

    static const std::vector<ModeDisplay> modeDisplays{
        {"Freeze",  "Position",     "Density",          "Size",             "Texture",          "Pitch",     "Trigger"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP/HP",            "Pitch",     "Time"},
        {"Stutter", "Time / Start", "Diffusion",        "Overlap / Duratn", "LP/HP",            "Pitch",     "Time"},
        {"Freeze",  "Buffer",       "FFT Upd. / Merge", "Polynomial",       "Quantize / Parts", "Transpose", "Glitch"}
    };

    static const std::vector<ModeDisplay> modeTooltips{
        {"Freeze",  "Position",     "Density",          "Size",               "Texture",          "Pitch",     "Trigger"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",            "LP/HP",            "Pitch",     "Time"},
        {"Stutter", "Time / Start", "Diffusion",        "Overlap / Duration", "LP/HP",            "Pitch",     "Time"},
        {"Freeze",  "Buffer",       "FFT Upd. / Merge", "Polynomial",         "Quantize / Parts", "Transpose", "Glitch"}
    };
}