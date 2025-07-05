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
        {"Freeze",  "Position",     "Density",          "Size",             "Texture",          "Pitch",     "Trigger", "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP | HP",          "Pitch",     "Time",    "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Stutter", "Time | Start", "Diffusion",        "Overlap | Duratn", "LP | HP",          "Pitch",     "Time",    "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Freeze",  "Buffer",       "FFT Upd. | Merge", "Polynomial",       "Quantize | Parts", "Transpose", "Glitch",  "Blend",      "Spread",    "Feedback",   "Reverb"},
        {"Freeze",  "Pre-delay",    "Decay",            "Size",             "Dampen LP | HP",   "Pitch",     "Clock",   "Dry | Wet",  "Diffusion", "Mod. Speed", "Mod. Amount"},
        {"Voice",   "Timbre",       "Decay",            "Chord",            "Filter LP | BP",   "Pitch",     "Burst",   "Distortion", "Stereo",    "Harmonics",  "Scatter"}
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeTooltips{
        {"Freeze",          "Grain position",                   "Grain density",                "Grain Size",                   "Grain texture",                        "Grain pitch",  "Trigger",          "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"Stutter",         "Scrub buffer",                     "Diffusion",                    "Overlap",                      "Filter: LP ↓ | HP ↑",                  "Grain pitch",  "Time",             "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"Stutter",         "Head position",                    "Granular diffusion",           "Overlap: Grainy ↓ | Smooth ↑", "Filter: LP ↓ | HP ↑",                  "Grain pitch",  "Delay time",       "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"Freeze",          "Buffer select",                    "F.F.T. Update ↓ | Merge ↑",    "Polynomial coefficients",      "Quantizer ↓ | Partial amplifier ↑",    "Transpose",    "Glitch audio",     "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"Freeze",          "Pre-delay Multiply ↓ | Divide ↑",  "Decay",                        "Room size",                    "Dampening: LP ↓ | HP ↑",               "Pitch shift",  "Clock",            "Dry ↔ wet",    "Diffusion",    "Modulation speed", "Modulation amount"},
        {"Switch voice",    "Timbre",                           "Decay",                        "Chord",                        "Filter: LP ↓ | HP ↑",                  "Pitch",        "Burst",            "Distortion",   "Stereo mix",   "String harmonics", "Scatter"}
    };
}