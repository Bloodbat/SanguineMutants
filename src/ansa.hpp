#pragma once

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "peakiesconsts.hpp"

struct Ansa : SanguineModule {

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

    Ansa();

    void onExpanderChange(const ExpanderChangeEvent& e) override;
};