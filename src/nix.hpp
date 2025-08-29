#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

#include "peakiesconsts.hpp"

using namespace sanguineCommonCode;

struct Nix : SanguineModule {
    enum ParamIds {
        PARAM_PARAM_CV_1,
        PARAM_PARAM_CV_2,
        PARAM_PARAM_CV_3,
        PARAM_PARAM_CV_4,
        PARAM_PARAM_CV_CHANNEL_2_1,
        PARAM_PARAM_CV_CHANNEL_2_2,
        PARAM_PARAM_CV_CHANNEL_2_3,
        PARAM_PARAM_CV_CHANNEL_2_4,
        PARAMS_COUNT
    };

    enum InputIds {
        INPUT_PARAM_CV_1,
        INPUT_PARAM_CV_2,
        INPUT_PARAM_CV_3,
        INPUT_PARAM_CV_4,
        INPUT_PARAM_CV_CHANNEL_2_1,
        INPUT_PARAM_CV_CHANNEL_2_2,
        INPUT_PARAM_CV_CHANNEL_2_3,
        INPUT_PARAM_CV_CHANNEL_2_4,
        INPUTS_COUNT
    };

    enum OutputIds {
        OUTPUTS_COUNT
    };

    enum LightIds {
        LIGHT_MASTER_MODULE,
        ENUMS(LIGHT_PARAM_1, 3),
        ENUMS(LIGHT_PARAM_2, 3),
        ENUMS(LIGHT_PARAM_3, 3),
        ENUMS(LIGHT_PARAM_4, 3),
        LIGHT_PARAM_CHANNEL_2_1,
        LIGHT_PARAM_CHANNEL_2_2,
        LIGHT_PARAM_CHANNEL_2_3,
        LIGHT_PARAM_CHANNEL_2_4,
        ENUMS(LIGHT_SPLIT_CHANNEL_1, 3),
        LIGHT_SPLIT_CHANNEL_2,
        LIGHTS_COUNT
    };

    Nix();

    void onExpanderChange(const ExpanderChangeEvent& e) override;
    void onPortChange(const PortChangeEvent& e) override;

    inline bool getChannel1PortChanged(const int portNumber) const {
        return expanderPorts1Changed[portNumber];
    }

    inline bool getChannel2PortChanged(const int portNumber) const {
        return expanderPorts2Changed[portNumber];
    }

    inline bool getChannel1PortConnected(const int portNumber) const {
        return expanderPorts1Connected[portNumber];
    }

    inline bool getChannel2PortConnected(const int portNumber) const {
        return expanderPorts2Connected[portNumber];
    }

    inline void setChannel1PortChanged(const int portNumber, const bool value) {
        expanderPorts1Changed[portNumber] = value;
    }

    inline void setChannel2PortChanged(const int portNumber, const bool value) {
        expanderPorts2Changed[portNumber] = value;
    }

private:
    bool expanderPorts1Changed[apicesCommon::kKnobCount];
    bool expanderPorts2Changed[apicesCommon::kKnobCount];

    bool expanderPorts1Connected[apicesCommon::kKnobCount];
    bool expanderPorts2Connected[apicesCommon::kKnobCount];
};