#pragma once

#include "cloudycommon.hpp"

namespace fluctus {
    static const std::vector<cloudyCommon::ModeInfo> modeList{
        { "GRANULAR", "Granular mode" },
        { "STRETCH", "Pitch shifter/time stretcher" },
        { "LOOPING DLY", "Looping delay" },
        { "SPCT. CLOUDS", "Spectral clouds" },
        { "BEAT-REPEAT", "Beat-repeat " },
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeDisplays{
        {"Freeze",  "Position",     "Density",          "Size",             "Texture",      "Pitch",         "Trigger",   "Blend",          "Spread",         "Feedback",   "Reverb"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP | HP",      "Pitch",         "Time",      "Blend",          "Spread",         "Feedback",   "Reverb"},
        {"Stutter", "Time | Start", "Diffusion",        "Overlap | Duratn", "LP | HP",      "Pitch",         "Time",      "Blend",          "Spread",         "Feedback",   "Reverb"},
        {"Freeze",  "Fq. Bnd Prb.", "Flt. Smooth",      "Fq. Bnd. Div.",    "Flt. Text.",   "Pitch shift",   "Randomize", "Dry | wet",      "Rnd. Flt. Prob", "Warm dist.", "Reverb"},
        {"Freeze",  "Loop begin",   "Loop size mod.",   "Loop size",        "Slice step",   "Playback spd.", "Clock",     "Slice prob.",    "Clock div.",     "Pitch mod.", "Feedback"}
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeTooltips{
        {"Freeze",  "Grain position",               "Grain density",        "Grain Size",                   "Grain texture",        "Grain pitch",      "Trigger",      "Dry ↔ wet",                "Spread",                       "Feedback",                 "Reverb"},
        {"Stutter", "Scrub buffer",                 "Diffusion",            "Overlap",                      "Filter: LP ↓ | HP ↑",  "Grain pitch",      "Time",         "Dry ↔ wet",                "Spread",                       "Feedback",                 "Reverb"},
        {"Stutter", "Head position",                "Granular diffusion",   "Overlap: Grainy ↓ | Smooth ↑", "Filter: LP ↓ | HP ↑",  "Grain pitch",      "Delay time",   "Dry ↔ wet",                "Spread",                       "Feedback",                 "Reverb"},
        {"Freeze",  "Frequency band probability",   "Filter smoothing",     "Frequency band division",      "Filter texture",       "Pitch shift",      "Randomize",    "Dry ↔ wet",                "Random filter probability",    "Warm distortion",          "Reverb"},
        {"Freeze",  "Loop start",                   "Loop size modulation", "Loop size",                    "Slice step",           "Playback speed",   "Clock",        "Slice probability",        "Clock division",               "Pitch modulation mode",    "Feedback"}
    };
}