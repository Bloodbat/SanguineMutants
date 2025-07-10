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

    // TODO: remove the tooltips for inputs without parameters!
    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeTooltips{
        {"",    "Grain position",                   "Grain density",                "Grain Size",                   "Grain texture",                        "Grain pitch",  "", "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"",    "Scrub buffer",                     "Diffusion",                    "Overlap",                      "Filter: LP ↓ | HP ↑",                  "Grain pitch",  "", "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"",    "Head position",                    "Granular diffusion",           "Overlap: Grainy ↓ | Smooth ↑", "Filter: LP ↓ | HP ↑",                  "Grain pitch",  "", "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"",    "Buffer select",                    "F.F.T. Update ↓ | Merge ↑",    "Polynomial coefficients",      "Quantizer ↓ | Partial amplifier ↑",    "Transpose",    "", "Dry ↔ wet",    "Spread",       "Feedback",         "Reverb"},
        {"",    "Pre-delay Multiply ↓ | Divide ↑",  "Decay",                        "Room size",                    "Dampening: LP ↓ | HP ↑",               "Pitch shift",  "", "Dry ↔ wet",    "Diffusion",    "Modulation speed", "Modulation amount"},
        {"",    "Timbre",                           "Decay",                        "Chord",                        "Filter: LP ↓ | HP ↑",                  "Pitch",        "", "Distortion",   "Stereo mix",   "String harmonics", "Scatter"}
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeInputTooltips{
        {"Freeze",          "Grain position CV",    "Grain density CV",         "Grain Size CV",                "Grain texture CV", "Grain pitch (1V / oct)",   "Trigger",       "Dry / wet CV",     "Spread CV",        "Feedback CV",         "Reverb CV"},
        {"Stutter",         "Scrub buffer CV",      "Diffusion CV",             "Overlap CV",                   "Filter CV",        "Grain pitch (1V / oct)",   "Time",          "Dry / wet CV",     "Spread CV",        "Feedback CV",         "Reverb CV"},
        {"Stutter",         "Head position CV",     "Granular diffusion CV",    "Overlap CV",                   "Filter CV",        "Grain pitch (1V / oct)",   "Delay time",    "Dry / wet CV",     "Spread CV",        "Feedback CV",         "Reverb CV"},
        {"Freeze",          "Buffer select CV",     "F.F.T. CV",                "Polynomial coefficients CV",   "Quantizer CV",     "Transpose CV",             "Glitch audio",  "Dry / wet CV",     "Spread CV",        "Feedback CV",         "Reverb CV"},
        {"Freeze",          "Pre-delay CV",         "Decay CV",                 "Room size CV",                 "Dampening CV",     "Pitch shift CV",           "Clock",         "Dry / wet CV",     "Diffusion CV",     "Modulation speed CV", "Modulation amount CV"},
        {"Switch voice",    "Timbre CV",            "Decay CV",                 "Chord CV",                     "Filter CV",        "Pitch (1V / oct)",         "Burst",         "Distortion CV",    "Stereo mix CV",    "String harmonics CV", "Scatter CV"}
    };
}