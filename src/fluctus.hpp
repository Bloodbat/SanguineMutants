#pragma once

#include "cloudycommon.hpp"

namespace fluctus {
    static const std::vector<std::string> modeListDisplay{
         "GRANULAR",
         "STRETCH",
         "LOOPING DLY",
         "SPCT. CLOUDS",
         "BEAT-REPEAT"
    };

    static const std::vector<std::string> modeListMenu{
        "Granular mode",
        "Pitch shifter / time stretcher",
        "Looping delay",
        "Spectral clouds",
        "Beat-repeat"
    };

#ifndef METAMODULE
    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeDisplays{
        {"Freeze",  "Position",     "Density",          "Size",             "Texture",      "Pitch",         "Trigger",   "Blend",          "Spread",         "Feedback",   "Reverb"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP | HP",      "Pitch",         "Time",      "Blend",          "Spread",         "Feedback",   "Reverb"},
        {"Stutter", "Time | Start", "Diffusion",        "Overlap | Duratn", "LP | HP",      "Pitch",         "Time",      "Blend",          "Spread",         "Feedback",   "Reverb"},
        {"Freeze",  "Fq. Bnd Prb.", "Flt. Smooth",      "Fq. Bnd. Div.",    "Flt. Text.",   "Pitch shift",   "Randomize", "Dry | wet",      "Rnd. Flt. Prob", "Warm dist.", "Reverb"},
        {"Freeze",  "Loop begin",   "Loop size mod.",   "Loop size",        "Slice step",   "Playback spd.", "Clock",     "Slice prob.",    "Clock div.",     "Pitch mod.", "Feedback"}
    };
#endif

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeTooltips{
        {"Freeze",  "Grain position",               "Grain density",        "Grain Size",                           "Grain texture",                        "Grain pitch",                  "", "Dry ↔ wet",            "Spread",                       "Feedback",                 "Reverb"},
        {"Stutter", "Scrub buffer",                 "Diffusion",            "Overlap",                              "Filter: LP ↓ | HP ↑",                  "Grain pitch",                  "", "Dry ↔ wet",            "Spread",                       "Feedback",                 "Reverb"},
        {"Stutter", "Head position",                "Granular diffusion",   "Overlap: Grainy ↓ | Smooth ↑",         "Filter: LP ↓ | HP ↑",                  "Grain pitch",                  "", "Dry ↔ wet",            "Spread",                       "Feedback",                 "Reverb"},
        {"Freeze",  "Frequency band probability",   "Filter smoothing",     "Frequency band division: 4 ↓ | 128 ↑", "Filter texture",                       "Pitch shift",                  "", "Dry ↔ wet",            "Random filter probability",    "Warm distortion",          "Reverb"},
        {"Freeze",  "Loop start",                   "Loop size modulation", "Loop size: Regular ↓ | Alternating ↑", "Slice step: Disabled ↓ | Random ↑",    "Playback speed: 0 ↔ Original", "", "Slice probability",    "Clock division",               "Pitch modulation mode",    "Feedback"}
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeInputTooltips{
       {"Freeze",  "Grain position CV",             "Grain density CV",        "Grain Size CV",                 "Grain texture CV",     "Grain pitch (1V / oct)",   "Trigger",      "Dry / wet CV",         "Spread CV",                    "Feedback CV",              "Reverb CV"},
       {"Stutter", "Scrub buffer CV",               "Diffusion CV",            "Overlap CV",                    "Filter CV",            "Grain pitch (1V / oct)",   "Time",         "Dry / wet CV",         "Spread CV",                    "Feedback CV",              "Reverb CV"},
       {"Stutter", "Head position CV",              "Granular diffusion CV",   "Overlap CV",                    "Filter CV",            "Grain pitch (1V / oct)",   "Delay time",   "Dry / wet CV",         "Spread CV",                    "Feedback CV",              "Reverb CV"},
       {"Freeze",  "Frequency band probability CV", "Filter smoothing CV",     "Frequency band division CV",    "Filter texture CV",    "Pitch shift CV",           "Randomize",    "Dry / wet CV",         "Random filter probability CV", "Warm distortion CV",       "Reverb CV"},
       {"Freeze",  "Loop start CV",                 "Loop size modulation CV", "Loop size CV",                  "Slice step CV",        "Playback speed CV",        "Clock",        "Slice probability CV", "Clock division CV",            "Pitch modulation mode CV", "Feedback CV"}
    };
}