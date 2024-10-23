#pragma once

enum SwitchIndex {
    SWITCH_TWIN_MODE,
    SWITCH_EXPERT,
    SWITCH_CHANNEL_SELECT,
    SWITCH_GATE_TRIG_1,
    SWITCH_GATE_TRIG_2
};

enum EditMode {
    EDIT_MODE_TWIN,
    EDIT_MODE_SPLIT,
    EDIT_MODE_FIRST,
    EDIT_MODE_SECOND,
    EDIT_MODE_LAST
};

struct Settings {
    uint8_t editMode;
    uint8_t processorFunction[2];
    uint8_t potValue[8];
    bool snap_mode;
};

struct KnobLabels {
    std::string knob1;
    std::string knob2;
    std::string knob3;
    std::string knob4;
};

static const size_t kNumBlocks = 2;
static const size_t kNumChannels = 2;
static const size_t kNumKnobs = 4;
static const size_t kNumFunctionLights = 4;
static const size_t kBlockSize = 4;
static const uint8_t kNumAdcChannels = 4;
static const uint16_t kAdcThresholdUnlocked = 1 << (16 - 10);  // 10 bits
static const uint16_t kAdcThresholdLocked = 1 << (16 - 8);  // 8 bits

static const uint8_t kButtonCount = 3;