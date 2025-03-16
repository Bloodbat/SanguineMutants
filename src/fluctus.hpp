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
        {"Freeze",  "Position",     "Density",          "Size",             "Texture",     "Pitch",         "Trigger",   "Blend",       "Spread",         "Feedback",   "Reverb"},
        {"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP/HP",       "Pitch",         "Time",      "Blend",       "Spread",         "Feedback",   "Reverb"},
        {"Stutter", "Time / Start", "Diffusion",        "Overlap / Duratn", "LP/HP",       "Pitch",         "Time",      "Blend",       "Spread",         "Feedback",   "Reverb"},
        {"Freeze",  "Fq. Bnd Prb.", "Flt. Smooth",      "Fq. Bnd. Div.",    "Flt. Text.",  "Pitch shift",   "Randomize", "Dry/wet",     "Rnd. Flt. Prob", "Warm dist.", "Reverb"},
        {"Freeze",  "Loop begin",   "Loop size mod.",   "Loop size",        "Slice step",  "Playback spd.", "Clock",     "Slice prob.", "Clock div.",     "Pitch mod.", "Feedback"}
    };

    static const std::vector<cloudyCommon::ParasiteModeDisplay> modeTooltips{
        {"Freeze",  "Position",                    "Density",              "Size",                    "Texture",        "Pitch",          "Trigger",   "Blend",             "Spread",                    "Feedback",         "Reverb"},
        {"Stutter", "Scrub",                       "Diffusion",            "Overlap",                 "LP/HP",          "Pitch",          "Time",      "Blend",             "Spread",                    "Feedback",         "Reverb"},
        {"Stutter", "Time / Start",                "Diffusion",            "Overlap / Duration",      "LP/HP",          "Pitch",          "Time",      "Blend",             "Spread",                    "Feedback",         "Reverb"},
        {"Freeze",  "Frequency band Probability",  "Filter Smoothing",     "Frequency band division", "Filter texture", "Pitch shift",    "Randomize", "Dry/wet",           "Random filter probability", "Warm distortion",  "Reverb"},
        {"Freeze",  "Loop begin",                  "Loop size modulation", "Loop size",               "Slice step",     "Playback speed", "Clock",     "Slice probability", "Clock division",            "Pitch modulation", "Feedback"}
    };
}