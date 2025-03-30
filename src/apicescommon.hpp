#pragma once

#include "peakiesconsts.hpp"

namespace apicesCommon {
    enum SwitchIndexes {
        SWITCH_TWIN_MODE,
        SWITCH_EXPERT,
        SWITCH_CHANNEL_SELECT,
        SWITCH_GATE_TRIG_1,
        SWITCH_GATE_TRIG_2
    };

    enum EditModes {
        EDIT_MODE_TWIN,
        EDIT_MODE_SPLIT,
        EDIT_MODE_FIRST,
        EDIT_MODE_SECOND,
        EDIT_MODE_LAST
    };

    struct Settings {
        uint8_t editMode;
        uint8_t processorFunctions[kChannelCount];
        uint8_t potValues[kKnobCount * 2];
        bool snapMode;
    };

    struct KnobLabels {
        std::string knob1;
        std::string knob2;
        std::string knob3;
        std::string knob4;
    };
}