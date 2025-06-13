#pragma once

namespace reticula {
    enum ExternalClockResolutions {
        RESOLUTION_4_PPQN,
        RESOLUTION_8_PPQN,
        RESOLUTION_24_PPQN,
    };

    enum ClockOutputSources {
        CLOCK_SOURCE_FIRST_BEAT,
        CLOCK_SOURCE_BEAT,
        CLOCK_SOURCE_PATTERN
    };

    enum GateModes {
        MODE_TRIGGER,
        MODE_GATE
    };

    static const std::vector<std::string> gateModesLabels = {
        "Trigger",
        "Gate"
    };

    static const std::vector<std::string> sequencerModeLabels = {
        "Original",
        "Henri",
        "Euclidean"
    };

    static const std::vector<std::string> clockOutputSourceLabels = {
        "First beat",
        "Beat",
        "Pattern"
    };

    static const std::vector<std::string> clockResolutionLabels = {
        "4 PPQN",
        "8 PPQN",
        "24 PPQN"
    };

    // NOTE: note a typo! This way it looks better on the display!
    static const std::string externalClockDisplay = "ExT";
}