#pragma once

namespace vimina {
    static const std::vector<std::string> modeLabels = {
        "Clock multiplier / divider",
        "Clock swing"
    };

    static const std::vector<std::string> factorLabels = {
        "x1",
        "x2",
        "x3",
        "x4",
        "x5",
        "x6",
        "x7",
        "x8",
        "÷2",
        "÷3",
        "÷4",
        "÷5",
        "÷6",
        "÷7",
        "÷8"
    };

    static const std::string clockPrefix = "Clock: ";

    // LED constants
    static const int ledDurations[3] = { 0 , 256, 128 };

    static const int kTimingErrorCorrectionAmount = 12;

    static const int kLightsFrequency = 16;

    static const int kMaxModuleSections = 2;

    // Channel ids
    static const int kResetChannel = 0;
    static const int kClockChannel = 1;

    static const int kTriggerExtendCount = 64;

    // Factorer constants
    /*
    The number 15 represents the set:
     -8, -7, -6, -5, -4, -3, -2, 1, 2, 3, 4, 5, 6, 7, 8
    where negative numbers are multiplier factors; positive numbers are divider
    factors, and 1 is bypass.
    */
    static const int kFactorCount = 15;
    /*
    The index of 1 in the set above.
    This is the factor setting where factorer is neither dividing nor multiplying.
    */
    static const int kFactorerBypassIndex = 7;
    static const int kFactorerBypassValue = 1;

    static const int kPulseTrackerBufferSize = 2;

    // Swing constants
    static constexpr float kSwingFactorMin = 50.f;
    static constexpr float kSwingFactorMax = 70.f; // Maximum swing amount can be set up to 99.

    // Scaling constant
    static constexpr float kMaxParamValue = 1.f;

    static constexpr float kSwingConversionFactor = kMaxParamValue / (kSwingFactorMax - kSwingFactorMin);
    static constexpr float kFactorerConversionFactor = kMaxParamValue / (kFactorCount - 1.f);

    static constexpr float kEdgeVoltageThreshold = 2.f;

    typedef float arrayChannelsFloat[PORT_MAX_CHANNELS];
}