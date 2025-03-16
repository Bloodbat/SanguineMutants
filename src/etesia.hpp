#pragma once

#include "cloudycommon.hpp"

namespace etesia {
    static const std::vector<cloudyCommon::ModeInfo> modeList{
        { "GRANULAR", "Granular mode" },
        { "STRETCH", "Pitch shifter/time stretcher" },
        { "LOOPING DLY", "Looping delay" },
        { "SPECTRAL", "Spectral madness" },
        { "OLIVERB", "Oliverb" },
        { "RESONESTOR", "Resonestor" }
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeDisplays{
        {"Freeze",  "Position",     "Density",          "Size",             "Texture",           "Pitch",     "Trigger", "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP/HP",             "Pitch",     "Time",    "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Stutter", "Time / Start", "Diffusion",        "Overlap / Duratn", "LP/HP",             "Pitch",     "Time",    "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Freeze",  "Buffer",       "FFT Upd. / Merge", "Polynomial",       "Quantize / Parts",  "Transpose", "Glitch",  "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Freeze",  "Pre-delay",    "Decay",            "Size",             "Dampen LP-V ?-HP",  "Pitch",     "Clock",   "Dry/Wet",    "Diffusion", "Mod. Speed", "Mod. Amount"},
        {"Voice",   "Timbre",       "Decay",            "Chord",            "Filter LP-V ?-BP",  "Pitch",     "Burst",   "Distortion", "Stereo",    "Harmonics",  "Scatter"}
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeTooltips{
        {"Freeze",  "Position",     "Density",            "Size",               "Texture",          "Pitch",     "Trigger", "Blend",      "Spread",    "Feedback",         "Reverb"},
        {"Stutter", "Scrub",        "Diffusion",          "Overlap",            "LP/HP",            "Pitch",     "Time",    "Blend",      "Spread",    "Feedback",         "Reverb"},
        {"Stutter", "Time / Start", "Diffusion",          "Overlap / Duration", "LP/HP",            "Pitch",     "Time",    "Blend",      "Spread",    "Feedback",         "Reverb"},
        {"Freeze",  "Buffer",       "FFT Update / Merge", "Polynomial",         "Quantize / Parts", "Transpose", "Glitch",  "Blend",      "Spread",    "Feedback",         "Reverb"},
        {"Freeze",  "Pre-delay",    "Decay",              "Size",               "Dampening",        "Pitch",     "Clock",   "Dry/Wet",    "Diffusion", "Modulation speed", "Modulation amount"},
        {"Voice",   "Timbre",       "Decay",              "Chord",              "Filter",           "Pitch",     "Burst",   "Distortion", "Stereo",    "Harmonics",        "Scatter"}
    };
}