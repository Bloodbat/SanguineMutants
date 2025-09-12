#pragma once

namespace apicesCommon {
    static const size_t kBlockCount = 2;
    static const size_t kBlockSize = 4;
    static const uint8_t kButtonCount = 3;
    static const size_t kChannelCount = 2;
    static const size_t kFunctionLightCount = 4;
    static const size_t kKnobCount = 4;
    static const size_t kHardwareRate = 48000;

    static const uint8_t kAdcChannelCount = 4;

    static const uint16_t kAdcThresholdLocked = 1 << (16 - 8);  // 8 bits
    static const uint16_t kAdcThresholdUnlocked = 1 << (16 - 10);  // 10 bits

    static const int kChannel2Offset = 4;
}