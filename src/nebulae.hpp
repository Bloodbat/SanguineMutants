#pragma once

#include "cloudycommon.hpp"

namespace nebulae {
    static const std::vector<std::string> modeListDisplay{
        "GRANULAR",
        "STRETCH",
        "LOOPING DLY",
        "SPECTRAL"
    };

    static const std::vector<std::string> modeListMenu{
        "Granular mode",
        "Pitch shifter / time stretcher",
        "Looping delay",
        "Spectral madness"
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

#ifndef METAMODULE
    static const std::vector<ModeDisplay> modeDisplays{
        {"Freeze",  "Position",     "Density",          "Size",             "Texture",          "Pitch",     "Trigger"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP | HP",          "Pitch",     "Time"},
        {"Stutter", "Time / Start", "Diffusion",        "Overlap | Duratn", "LP | HP",          "Pitch",     "Time"},
        {"Freeze",  "Buffer",       "FFT Upd. | Merge", "Polynomial",       "Quantize | Parts", "Transpose", "Glitch"}
    };
#endif

    static const std::vector<ModeDisplay> modeTooltips{
        {"Freeze",  "Grain position",   "Grain density",                "Grain Size",                   "Grain texture",                        "Grain pitch",  ""},
        {"Stutter", "Scrub buffer",     "Diffusion",                    "Overlap",                      "LP ↓ | HP ↑",                          "Grain pitch",  ""},
        {"Stutter", "Head position",    "Granular diffusion",           "Overlap: Grainy ↓ | Smooth ↑", "LP ↓ | HP ↑",                          "Grain pitch",  ""},
        {"Freeze",  "Buffer select",    "F.F.T. Update ↓ | Merge ↑",    "Polynomial coefficients",      "Quantizer ↓ | Partial amplifier ↑",    "Transpose",    ""}
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeInputTooltips{
        {"Freeze",  "Grain position CV",    "Grain density CV",         "Grain Size CV",                "Grain texture CV", "Grain pitch (1V / oct)",   "Trigger",       "Dry / wet CV",    "Spread CV",    "Feedback CV",  "Reverb CV"},
        {"Stutter", "Scrub buffer CV",      "Diffusion CV",             "Overlap CV",                   "Filter CV",        "Grain pitch (1V / oct)",   "Time",          "Dry / wet CV",    "Spread CV",    "Feedback CV",  "Reverb CV"},
        {"Stutter", "Head position CV",     "Granular diffusion CV",    "Overlap CV",                   "Filter CV",        "Grain pitch (1V / oct)",   "Delay time",    "Dry / wet CV",    "Spread CV",    "Feedback CV",  "Reverb CV"},
        {"Freeze",  "Buffer select CV",     "F.F.T. CV",                "Polynomial coefficients CV",   "Quantizer CV",     "Transpose CV",             "Glitch audio",  "Dry / wet CV",    "Spread CV",    "Feedback CV",  "Reverb CV"}
    };
}