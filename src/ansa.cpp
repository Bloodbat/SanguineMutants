#include "ansa.hpp"

Ansa::Ansa() {
    config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

    for (int function = 0; function < kMaxFunctions; ++function) {
        int functionNumber = function + 1;
        configParam(PARAM_PARAM_CV_1 + function, -1.f, 1.f, 0.f, string::f("Function %d CV", functionNumber));
        configInput(INPUT_PARAM_CV_1 + function, string::f("Function %d", functionNumber));

        configParam(PARAM_PARAM_CV_1 + function + kChannel2Offset, -1.f, 1.f, 0.f,
            string::f("Expert channel 2 function %d CV", functionNumber));
        configInput(INPUT_PARAM_CV_1 + function + kChannel2Offset,
            string::f("Expert channel 2 function %d", functionNumber));
    }
}

void Ansa::onExpanderChange(const ExpanderChangeEvent& e) {
    Module* mortuusMaster = getLeftExpander().module;
    bool bHasMaster = (mortuusMaster && mortuusMaster->getModel() == modelMortuus);
    lights[LIGHT_MASTER_MODULE].setBrightness(bHasMaster ? 0.5f : 0.f);
    lights[LIGHT_SPLIT_CHANNEL_2].setBrightness(0.f);

    for (int light = 0; light < 3; ++light) {
        lights[LIGHT_SPLIT_CHANNEL_1 + light].setBrightness(0.f);
    }

    if (!bHasMaster) {
        for (int light = 0; light < kMaxFunctions; ++light) {
            int currentLight = LIGHT_PARAM_1 + light * 3;
            for (int lightColor = 0; lightColor < 3; ++lightColor) {
                lights[currentLight + lightColor].setBrightness(0.f);
            }
        }
    }
}

struct AnsaWidget : SanguineModuleWidget {
    AnsaWidget(Ansa* module) {
        setModule(module);

        moduleName = "ansa";
        panelSize = SIZE_9;
        backplateColor = PLATE_RED;

        makePanel();

        addScrews(SCREW_ALL);

        addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(2.8, 5.573), module, Ansa::LIGHT_MASTER_MODULE));

        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(11.295, 11.455), module, Ansa::LIGHT_SPLIT_CHANNEL_1));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(34.43, 11.455), module, Ansa::LIGHT_SPLIT_CHANNEL_2));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 20.72), module, Ansa::PARAM_PARAM_CV_1));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.653, 25.113), module, Ansa::LIGHT_PARAM_1));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 30.518), module, Ansa::INPUT_PARAM_CV_1));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 43.641), module, Ansa::PARAM_PARAM_CV_2));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.653, 48.046), module, Ansa::LIGHT_PARAM_2));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 53.45), module, Ansa::INPUT_PARAM_CV_2));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 83.001), module, Ansa::PARAM_PARAM_CV_3));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.653, 87.405), module, Ansa::LIGHT_PARAM_3));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 92.81), module, Ansa::INPUT_PARAM_CV_3));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(10.859, 105.933), module, Ansa::PARAM_PARAM_CV_4));
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(17.653, 110.338), module, Ansa::LIGHT_PARAM_4));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(10.859, 115.742), module, Ansa::INPUT_PARAM_CV_4));

        // Split Channel 2
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(33.994, 20.72), module, Ansa::PARAM_PARAM_CV_CHANNEL_2_1));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(40.789, 25.113), module, Ansa::LIGHT_PARAM_CHANNEL_2_1));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.994, 30.518), module, Ansa::INPUT_PARAM_CV_CHANNEL_2_1));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(33.994, 43.641), module, Ansa::PARAM_PARAM_CV_CHANNEL_2_2));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(40.789, 48.046), module, Ansa::LIGHT_PARAM_CHANNEL_2_2));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.994, 53.45), module, Ansa::INPUT_PARAM_CV_CHANNEL_2_2));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(33.994, 83.001), module, Ansa::PARAM_PARAM_CV_CHANNEL_2_3));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(40.789, 87.405), module, Ansa::LIGHT_PARAM_CHANNEL_2_3));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.994, 92.81), module, Ansa::INPUT_PARAM_CV_CHANNEL_2_3));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(33.994, 105.933), module, Ansa::PARAM_PARAM_CV_CHANNEL_2_4));
        addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(40.789, 110.338), module, Ansa::LIGHT_PARAM_CHANNEL_2_4));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.994, 115.742), module, Ansa::INPUT_PARAM_CV_CHANNEL_2_4));
    }
};

Model* modelAnsa = createModel<Ansa, AnsaWidget>("Sanguine-Ansa");