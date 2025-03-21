#pragma once

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

    static const size_t kBlockCount = 2;
    static const size_t kChannelCount = 2;
    static const size_t kKnobCount = 4;
    static const size_t kFunctionLightCount = 4;
    static const size_t kBlockSize = 4;
    static const uint8_t kAdcChannelCount = 4;
    static const uint16_t kAdcThresholdUnlocked = 1 << (16 - 10);  // 10 bits
    static const uint16_t kAdcThresholdLocked = 1 << (16 - 8);  // 8 bits
    static const int kSampleRate = 48000;

    static const uint8_t kButtonCount = 3;

    struct Settings {
        uint8_t editMode;
        uint8_t processorFunctions[kChannelCount];
        uint8_t potValues[kKnobCount * 2];
        bool snap_mode;
    };

    struct KnobLabels {
        std::string knob1;
        std::string knob2;
        std::string knob3;
        std::string knob4;
    };
}