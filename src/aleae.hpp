#pragma once

namespace aleae {
    enum RollModes {
        ROLL_DIRECT,
        ROLL_TOGGLE
    };

    enum OutModes {
        OUT_MODE_TRIGGER,
        OUT_MODE_LATCH
    };

    enum RollResults {
        ROLL_HEADS,
        ROLL_TAILS
    };

    static const std::vector<std::string> rollModeLabels{
        "Direct",
        "Toggle"
    };

    static const std::vector<std::string> outModeLabels{
        "Trigger",
        "Latch"
    };

    static const int kLightsFrequency = 16;
    static const int kMaxModuleSections = 2;

    struct ActiveGates {
        bool gateA;
        bool gateB;
    };
}