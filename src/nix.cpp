#include "nix.hpp"

Nix::Nix() {
    config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

    for (size_t parameter = 0; parameter < apicesCommon::kKnobCount; ++parameter) {
        int functionNumber = parameter + 1;
        configParam(PARAM_PARAM_CV_1 + parameter, -1.f, 1.f, 0.f, string::f("Parameter %d CV", functionNumber),
            "%", 0.f, 100.f);
        configInput(INPUT_PARAM_CV_1 + parameter, string::f("Parameter %d", functionNumber));

        configParam(PARAM_PARAM_CV_1 + parameter + apicesCommon::kChannel2Offset, -1.f, 1.f, 0.f,
            string::f("Expert channel 2 parameter %d CV", functionNumber), "%", 0.f, 100.f);
        configInput(INPUT_PARAM_CV_1 + parameter + apicesCommon::kChannel2Offset,
            string::f("Expert channel 2 parameter %d", functionNumber));
    }
}

void Nix::onExpanderChange(const ExpanderChangeEvent& e) {
    Module* apicesMaster = getLeftExpander().module;
    bool bHasMaster = (apicesMaster && apicesMaster->getModel() == modelApices);
    lights[LIGHT_MASTER_MODULE].setBrightness(bHasMaster ? kSanguineButtonLightValue : 0.f);
    lights[LIGHT_SPLIT_CHANNEL_2].setBrightness(0.f);

    for (int light = 0; light < 3; ++light) {
        lights[LIGHT_SPLIT_CHANNEL_1 + light].setBrightness(0.f);
    }

    if (!bHasMaster) {
        for (size_t light = 0; light < apicesCommon::kKnobCount; ++light) {
            int currentLight = LIGHT_PARAM_1 + light * 3;
            for (int lightColor = 0; lightColor < 3; ++lightColor) {
                lights[currentLight + lightColor].setBrightness(0.f);
            }
        }
    }
}

struct NixWidget : SanguineModuleWidget {
    explicit NixWidget(Nix* module) {
        setModule(module);

        moduleName = "nix";
        panelSize = sanguineThemes::SIZE_9;
        backplateColor = sanguineThemes::PLATE_PURPLE;

        makePanel();

        addScrews(SCREW_ALL);

        addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(2.8, 5.573), module, Nix::LIGHT_MASTER_MODULE));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 20.72), module, Nix::PARAM_PARAM_CV_1));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.653, 25.113), module, Nix::LIGHT_PARAM_1));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 30.518), module, Nix::INPUT_PARAM_CV_1));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.43, 20.72), module, Nix::PARAM_PARAM_CV_2));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(28.067, 25.113), module, Nix::LIGHT_PARAM_2));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.43, 30.518), module, Nix::INPUT_PARAM_CV_2));

        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(22.862, 36.706), module, Nix::LIGHT_SPLIT_CHANNEL_1));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 43.641), module, Nix::PARAM_PARAM_CV_3));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.653, 48.046), module, Nix::LIGHT_PARAM_3));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 53.45), module, Nix::INPUT_PARAM_CV_3));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.43, 43.641), module, Nix::PARAM_PARAM_CV_4));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(28.067, 48.046), module, Nix::LIGHT_PARAM_4));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.43, 53.45), module, Nix::INPUT_PARAM_CV_4));

        // Split Channel 2
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 83.001), module, Nix::PARAM_PARAM_CV_CHANNEL_2_1));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(17.653, 87.405), module, Nix::LIGHT_PARAM_CHANNEL_2_1));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 92.81), module, Nix::INPUT_PARAM_CV_CHANNEL_2_1));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.43, 83.001), module, Nix::PARAM_PARAM_CV_CHANNEL_2_2));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(28.067, 87.405), module, Nix::LIGHT_PARAM_CHANNEL_2_2));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.994, 92.81), module, Nix::INPUT_PARAM_CV_CHANNEL_2_2));

        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(22.862, 98.998), module, Nix::LIGHT_SPLIT_CHANNEL_2));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 105.933), module, Nix::PARAM_PARAM_CV_CHANNEL_2_3));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(17.653, 110.338), module, Nix::LIGHT_PARAM_CHANNEL_2_3));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 115.742), module, Nix::INPUT_PARAM_CV_CHANNEL_2_3));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(34.43, 105.933), module, Nix::PARAM_PARAM_CV_CHANNEL_2_4));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(28.067, 110.338), module, Nix::LIGHT_PARAM_CHANNEL_2_4));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(34.43, 115.742), module, Nix::INPUT_PARAM_CV_CHANNEL_2_4));
    }
};

Model* modelNix = createModel<Nix, NixWidget>("Sanguine-Nix");