#include "plugin.hpp"
#include "scalaria/dsp/scalaria_modulator.h"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "warpiescommon.hpp"
#include "scalaria.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Scalaria : SanguineModule {
    enum ParamIds {
        PARAM_FREQUENCY,
        PARAM_RESONANCE,
        PARAM_INTERNAL_OSCILLATOR,
        PARAM_CHANNEL_1_LEVEL,
        PARAM_CHANNEL_2_LEVEL,
        PARAM_FREQUENCY_CV_ATTENUVERTER,
        PARAM_RESONANCE_CV_ATTENUVERTER,
        PARAMS_COUNT
    };
    enum InputIds {
        INPUT_CHANNEL_1_LEVEL,
        INPUT_CHANNEL_2_LEVEL,
        INPUT_FREQUENCY,
        INPUT_RESONANCE,
        INPUT_CHANNEL_1,
        INPUT_CHANNEL_2,
        INPUTS_COUNT
    };
    enum OutputIds {
        OUTPUT_CHANNEL_1_PLUS_2,
        OUTPUT_AUX,
        OUTPUTS_COUNT
    };
    enum LightIds {
        ENUMS(LIGHT_INTERNAL_OSCILLATOR_OFF, 3),
        ENUMS(LIGHT_INTERNAL_OSCILLATOR_TRIANGLE, 3),
        ENUMS(LIGHT_INTERNAL_OSCILLATOR_SAW, 3),
        ENUMS(LIGHT_INTERNAL_OSCILLATOR_SQUARE, 3),
        ENUMS(LIGHT_CHANNEL_1_FREQUENCY, 3),
        ENUMS(LIGHT_CHANNEL_1_LEVEL, 3),
        ENUMS(LIGHT_CHANNEL_1, 3),
        ENUMS(LIGHT_CHANNEL_2, 3),
        ENUMS(LIGHT_CHANNEL_3, 3),
        ENUMS(LIGHT_CHANNEL_4, 3),
        ENUMS(LIGHT_CHANNEL_5, 3),
        ENUMS(LIGHT_CHANNEL_6, 3),
        ENUMS(LIGHT_CHANNEL_7, 3),
        ENUMS(LIGHT_CHANNEL_8, 3),
        ENUMS(LIGHT_CHANNEL_9, 3),
        ENUMS(LIGHT_CHANNEL_10, 3),
        ENUMS(LIGHT_CHANNEL_11, 3),
        ENUMS(LIGHT_CHANNEL_12, 3),
        ENUMS(LIGHT_CHANNEL_13, 3),
        ENUMS(LIGHT_CHANNEL_14, 3),
        ENUMS(LIGHT_CHANNEL_15, 3),
        ENUMS(LIGHT_CHANNEL_16, 3),
        LIGHTS_COUNT
    };

    int frame[PORT_MAX_CHANNELS] = {};

    static const int kLightFrequency = 128;

    dsp::ClockDivider lightsDivider;
    scalaria::ScalariaModulator scalariaModulator[PORT_MAX_CHANNELS];
    scalaria::ShortFrame inputFrames[PORT_MAX_CHANNELS][kWarpsBlockSize];
    scalaria::ShortFrame outputFrames[PORT_MAX_CHANNELS][kWarpsBlockSize];

    scalaria::Parameters* scalariaParameters[PORT_MAX_CHANNELS];

    Scalaria() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

        configParam(PARAM_FREQUENCY, 0.f, 1.f, 0.f, "Cutoff frequency");

        configParam(PARAM_FREQUENCY_CV_ATTENUVERTER, -1.f, 1.f, 0.f, "Cutoff frequency CV");

        configParam(PARAM_RESONANCE, 0.f, 1.f, 0.5f, "Resonance", "%", 0, 100);

        configParam(PARAM_RESONANCE_CV_ATTENUVERTER, -1.f, 1.f, 0.f, "Resonance CV");

        configSwitch(PARAM_INTERNAL_OSCILLATOR, 0.f, 3.f, 0.f, "Internal oscillator", scalariaOscillatorNames);

        configParam(PARAM_CHANNEL_1_LEVEL, 0.f, 1.f, 0.66f, "External oscillator amplitude / internal oscillator frequency", "%", 0, 100);
        configParam(PARAM_CHANNEL_2_LEVEL, 0.f, 1.f, 0.66f, "Channel 2 amplitude", "%", 0, 100);

        configInput(INPUT_CHANNEL_1_LEVEL, "Channel 1 level");
        configInput(INPUT_CHANNEL_2_LEVEL, "Channel 2 level");
        configInput(INPUT_FREQUENCY, "Cutoff frequency");
        configInput(INPUT_RESONANCE, "Resonance");
        configInput(INPUT_CHANNEL_1, "Channel 1");
        configInput(INPUT_CHANNEL_2, "Channel 2");

        configOutput(OUTPUT_CHANNEL_1_PLUS_2, "Channel 1+2");
        configOutput(OUTPUT_AUX, "Auxiliary");

        configBypass(INPUT_CHANNEL_1, OUTPUT_CHANNEL_1_PLUS_2);

        for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
            memset(&scalariaModulator[channel], 0, sizeof(scalaria::ScalariaModulator));
            scalariaModulator[channel].Init(kScalariaSampleRate);
            scalariaParameters[channel] = scalariaModulator[channel].mutableParameters();
        }

        lightsDivider.setDivision(kLightFrequency);

        lights[LIGHT_INTERNAL_OSCILLATOR_OFF + 0].setBrightness(0.f);
        lights[LIGHT_INTERNAL_OSCILLATOR_OFF + 1].setBrightness(0.f);

        lights[LIGHT_INTERNAL_OSCILLATOR_TRIANGLE + 0].setBrightness(0.f);
        lights[LIGHT_INTERNAL_OSCILLATOR_TRIANGLE + 2].setBrightness(0.f);

        lights[LIGHT_INTERNAL_OSCILLATOR_SAW + 2].setBrightness(0.f);

        lights[LIGHT_INTERNAL_OSCILLATOR_SQUARE + 1].setBrightness(0.f);
        lights[LIGHT_INTERNAL_OSCILLATOR_SQUARE + 2].setBrightness(0.f);

        lights[LIGHT_CHANNEL_1_FREQUENCY + 0].setBrightness(0.f);
        lights[LIGHT_CHANNEL_1_FREQUENCY + 2].setBrightness(0.f);

        lights[LIGHT_CHANNEL_1_LEVEL + 0].setBrightness(0.f);
        lights[LIGHT_CHANNEL_1_LEVEL + 2].setBrightness(0.f);
    }

    void process(const ProcessArgs& args) override {
        using simd::float_4;

        int channelCount = std::max(std::max(inputs[INPUT_CHANNEL_1].getChannels(), inputs[INPUT_CHANNEL_2].getChannels()), 1);

        outputs[OUTPUT_CHANNEL_1_PLUS_2].setChannels(channelCount);
        outputs[OUTPUT_AUX].setChannels(channelCount);
        float frequencyValue = params[PARAM_FREQUENCY].getValue();

        float frequencyAttenuverterValue = params[PARAM_FREQUENCY_CV_ATTENUVERTER].getValue();

        float resonanceAttenuverterValue = params[PARAM_RESONANCE_CV_ATTENUVERTER].getValue();

        for (int channel = 0; channel < channelCount; ++channel) {
            scalariaParameters[channel]->oscillatorShape = params[PARAM_INTERNAL_OSCILLATOR].getValue();

            float_4 f4Voltages;

            // Buffer loop
            if (++frame[channel] >= kWarpsBlockSize) {
                frame[channel] = 0;

                // CHANNEL_1_LEVEL and CHANNEL_2_LEVEL are normalized values: from cv_scaler.cc and a PR by Brian Head to AI's repository.
                f4Voltages[0] = inputs[INPUT_CHANNEL_1_LEVEL].getNormalVoltage(5.f, channel);
                f4Voltages[1] = inputs[INPUT_CHANNEL_2_LEVEL].getNormalVoltage(5.f, channel);
                f4Voltages[2] = inputs[INPUT_FREQUENCY].getVoltage(channel);
                f4Voltages[3] = inputs[INPUT_RESONANCE].getVoltage(channel);

                f4Voltages /= 5.f;

                scalariaParameters[channel]->channel_drive[0] = clamp(params[PARAM_CHANNEL_1_LEVEL].getValue() * f4Voltages[0], 0.f, 1.f);
                scalariaParameters[channel]->channel_drive[1] = clamp(params[PARAM_CHANNEL_2_LEVEL].getValue() * f4Voltages[1], 0.f, 1.f);

                scalariaParameters[channel]->rawFrequency = clamp(frequencyValue +
                    (f4Voltages[2] * frequencyAttenuverterValue), 0.f, 1.f);

                scalariaParameters[channel]->rawResonance = clamp(params[PARAM_RESONANCE].getValue() +
                    (f4Voltages[3] * resonanceAttenuverterValue), 0.f, 1.f);

                scalariaParameters[channel]->note = 60.f * params[PARAM_CHANNEL_1_LEVEL].getValue() + 12.f
                    * inputs[INPUT_CHANNEL_1_LEVEL].getNormalVoltage(2.f, channel) + 12.f;
                scalariaParameters[channel]->note += log2f(kScalariaSampleRate * args.sampleTime) * 12.f;

                scalariaModulator[channel].Process(inputFrames[channel], outputFrames[channel], kWarpsBlockSize);
            }

            inputFrames[channel][frame[channel]].l = clamp(static_cast<int>(inputs[INPUT_CHANNEL_1].getVoltage(channel) / 8.f * 32768),
                -32768, 32767);
            inputFrames[channel][frame[channel]].r = clamp(static_cast<int>(inputs[INPUT_CHANNEL_2].getVoltage(channel) / 8.f * 32768),
                -32768, 32767);

            outputs[OUTPUT_CHANNEL_1_PLUS_2].setVoltage(static_cast<float>(outputFrames[channel][frame[channel]].l) / 32768 * 5.f, channel);
            outputs[OUTPUT_AUX].setVoltage(static_cast<float>(outputFrames[channel][frame[channel]].r) / 32768 * 5.f, channel);
        }

        if (lightsDivider.process()) {
            const float sampleTime = kLightFrequency * args.sampleTime;

            bool bHaveInternalOscillator = !scalariaParameters[0]->oscillatorShape == 0;

            lights[LIGHT_INTERNAL_OSCILLATOR_OFF + 2].setBrightnessSmooth(bHaveInternalOscillator ? 0.f : 0.5f, sampleTime);

            lights[LIGHT_INTERNAL_OSCILLATOR_TRIANGLE + 1].setBrightnessSmooth(scalariaParameters[0]->oscillatorShape == 1 ? 0.5f : 0.f, sampleTime);

            lights[LIGHT_INTERNAL_OSCILLATOR_SAW + 0].setBrightnessSmooth(scalariaParameters[0]->oscillatorShape == 2 ? 0.5f : 0.f, sampleTime);
            lights[LIGHT_INTERNAL_OSCILLATOR_SAW + 1].setBrightnessSmooth(scalariaParameters[0]->oscillatorShape == 2 ? 0.5f : 0.f, sampleTime);

            lights[LIGHT_INTERNAL_OSCILLATOR_SQUARE + 0].setBrightnessSmooth(scalariaParameters[0]->oscillatorShape == 3 ? 0.5f : 0.f, sampleTime);

            lights[LIGHT_CHANNEL_1_FREQUENCY + 1].setBrightnessSmooth(bHaveInternalOscillator ? 0.5f : 0.f, sampleTime);

            lights[LIGHT_CHANNEL_1_LEVEL + 1].setBrightnessSmooth(bHaveInternalOscillator ? 0.f : 0.5f, sampleTime);

            for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
                const int currentLight = LIGHT_CHANNEL_1 + channel * 3;

                if (channel < channelCount) {
                    const uint8_t(*palette)[3];
                    float zone;

                    palette = paletteScalaria;

                    zone = 8.f * scalariaParameters[channel]->rawFrequency;
                    MAKE_INTEGRAL_FRACTIONAL(zone);
                    int zone_fractional_i = static_cast<int>(zone_fractional * 256);
                    for (int rgbComponent = 0; rgbComponent < 3; ++rgbComponent) {
                        int a = palette[zone_integral][rgbComponent];
                        int b = palette[zone_integral + 1][rgbComponent];
                        const float lightValue = static_cast<float>(a + ((b - a) * zone_fractional_i >> 8)) / 255.f;
                        lights[currentLight + rgbComponent].setBrightness(rescale(lightValue, 0.f, 1.f, 0.f, 0.5f));
                    }
                } else {
                    for (int rgbComponent = 0; rgbComponent < 3; ++rgbComponent) {
                        lights[currentLight + rgbComponent].setBrightnessSmooth(0.f, sampleTime);
                    }
                }
            }
        }
    }
};

struct AcrylicOff : SanguineShapedAcrylicLed {
    AcrylicOff() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_off.svg")));
    }
};

struct AcrylicTriangle : SanguineShapedAcrylicLed {
    AcrylicTriangle() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_triangle.svg")));
    }
};

struct AcrylicSaw : SanguineShapedAcrylicLed {
    AcrylicSaw() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_saw.svg")));
    }
};

struct AcrylicSquare : SanguineShapedAcrylicLed {
    AcrylicSquare() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_square.svg")));
    }
};

struct AcrylicFreq : SanguineShapedAcrylicLed {
    AcrylicFreq() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_freq.svg")));
    }
};

struct AcrylicLvl1 : SanguineShapedAcrylicLed {
    AcrylicLvl1() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_lvl1.svg")));
    }
};

struct AcrylicChannel1 : SanguineShapedAcrylicLed {
    AcrylicChannel1() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_1.svg")));
    }
};

struct AcrylicChannel2 : SanguineShapedAcrylicLed {
    AcrylicChannel2() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_2.svg")));
    }
};

struct AcrylicChannel3 : SanguineShapedAcrylicLed {
    AcrylicChannel3() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_3.svg")));
    }
};

struct AcrylicChannel4 : SanguineShapedAcrylicLed {
    AcrylicChannel4() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_4.svg")));
    }
};

struct AcrylicChannel5 : SanguineShapedAcrylicLed {
    AcrylicChannel5() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_5.svg")));
    }
};

struct AcrylicChannel6 : SanguineShapedAcrylicLed {
    AcrylicChannel6() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_6.svg")));
    }
};

struct AcrylicChannel7 : SanguineShapedAcrylicLed {
    AcrylicChannel7() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_7.svg")));
    }
};

struct AcrylicChannel8 : SanguineShapedAcrylicLed {
    AcrylicChannel8() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_8.svg")));
    }
};

struct AcrylicChannel9 : SanguineShapedAcrylicLed {
    AcrylicChannel9() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_9.svg")));
    }
};

struct AcrylicChannel10 : SanguineShapedAcrylicLed {
    AcrylicChannel10() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_10.svg")));
    }
};

struct AcrylicChannel11 : SanguineShapedAcrylicLed {
    AcrylicChannel11() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_11.svg")));
    }
};

struct AcrylicChannel12 : SanguineShapedAcrylicLed {
    AcrylicChannel12() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_12.svg")));
    }
};

struct AcrylicChannel13 : SanguineShapedAcrylicLed {
    AcrylicChannel13() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_13.svg")));
    }
};

struct AcrylicChannel14 : SanguineShapedAcrylicLed {
    AcrylicChannel14() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_14.svg")));
    }
};

struct AcrylicChannel15 : SanguineShapedAcrylicLed {
    AcrylicChannel15() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_15.svg")));
    }
};

struct AcrylicChannel16 : SanguineShapedAcrylicLed {
    AcrylicChannel16() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_16.svg")));
    }
};

struct ScalariaWidget : SanguineModuleWidget {
    explicit ScalariaWidget(Scalaria* module) {
        setModule(module);

        moduleName = "scalaria";
        panelSize = SIZE_9;
        backplateColor = PLATE_PURPLE;

        makePanel();

        addScrews(SCREW_ALL);

        addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(22.86, 21.855), module, Scalaria::PARAM_FREQUENCY));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(5.856, 18.858), module, Scalaria::PARAM_FREQUENCY_CV_ATTENUVERTER));

        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.042, 27.417), module, Scalaria::INPUT_FREQUENCY));

        addParam(createParamCentered<Sanguine2PSBlue>(millimetersToPixelsVec(22.86, 48.638), module, Scalaria::PARAM_RESONANCE));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(5.856, 43.544), module, Scalaria::PARAM_RESONANCE_CV_ATTENUVERTER));

        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.042, 52.123), module, Scalaria::INPUT_RESONANCE));

        addParam(createParamCentered<Sanguine1PGrayCap>(millimetersToPixelsVec(22.86, 69.587), module, Scalaria::PARAM_INTERNAL_OSCILLATOR));

        addParam(createParamCentered<Sanguine1PGreen>(millimetersToPixelsVec(11.605, 86.612), module, Scalaria::PARAM_CHANNEL_1_LEVEL));

        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(11.605, 100.148), module, Scalaria::INPUT_CHANNEL_1_LEVEL));

        addParam(createParamCentered<Sanguine1PGreen>(millimetersToPixelsVec(34.119, 86.612), module, Scalaria::PARAM_CHANNEL_2_LEVEL));

        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(34.119, 100.148), module, Scalaria::INPUT_CHANNEL_2_LEVEL));

        addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.185, 116.272), module, Scalaria::INPUT_CHANNEL_1));

        addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(17.037, 116.272), module, Scalaria::INPUT_CHANNEL_2));

        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(28.704, 116.272), module, Scalaria::OUTPUT_CHANNEL_1_PLUS_2));

        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(39.556, 116.272), module, Scalaria::OUTPUT_AUX));


        addChild(createLightCentered<AcrylicOff>(millimetersToPixelsVec(16.385, 75.676), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_OFF));

        addChild(createLightCentered<AcrylicTriangle>(millimetersToPixelsVec(16.385, 63.997), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_TRIANGLE));

        addChild(createLightCentered<AcrylicSaw>(millimetersToPixelsVec(29.335, 63.997), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_SAW));

        addChild(createLightCentered<AcrylicSquare>(millimetersToPixelsVec(29.335, 75.676), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_SQUARE));

        addChild(createLightCentered<AcrylicFreq>(millimetersToPixelsVec(9.494, 93.944), module, Scalaria::LIGHT_CHANNEL_1_FREQUENCY));

        addChild(createLightCentered<AcrylicLvl1>(millimetersToPixelsVec(13.761, 93.944), module, Scalaria::LIGHT_CHANNEL_1_LEVEL));

        addChild(createLightCentered<AcrylicChannel1>(millimetersToPixelsVec(39.77, 70.927), module, Scalaria::LIGHT_CHANNEL_1));

        addChild(createLightCentered<AcrylicChannel2>(millimetersToPixelsVec(39.77, 67.927), module, Scalaria::LIGHT_CHANNEL_2));

        addChild(createLightCentered<AcrylicChannel3>(millimetersToPixelsVec(39.77, 64.927), module, Scalaria::LIGHT_CHANNEL_3));

        addChild(createLightCentered<AcrylicChannel4>(millimetersToPixelsVec(39.77, 61.927), module, Scalaria::LIGHT_CHANNEL_4));

        addChild(createLightCentered<AcrylicChannel5>(millimetersToPixelsVec(39.77, 58.927), module, Scalaria::LIGHT_CHANNEL_5));

        addChild(createLightCentered<AcrylicChannel6>(millimetersToPixelsVec(39.77, 55.927), module, Scalaria::LIGHT_CHANNEL_6));

        addChild(createLightCentered<AcrylicChannel7>(millimetersToPixelsVec(39.77, 52.927), module, Scalaria::LIGHT_CHANNEL_7));

        addChild(createLightCentered<AcrylicChannel8>(millimetersToPixelsVec(39.77, 49.927), module, Scalaria::LIGHT_CHANNEL_8));

        addChild(createLightCentered<AcrylicChannel9>(millimetersToPixelsVec(39.77, 46.927), module, Scalaria::LIGHT_CHANNEL_9));

        addChild(createLightCentered<AcrylicChannel10>(millimetersToPixelsVec(39.77, 43.927), module, Scalaria::LIGHT_CHANNEL_10));

        addChild(createLightCentered<AcrylicChannel11>(millimetersToPixelsVec(39.77, 40.927), module, Scalaria::LIGHT_CHANNEL_11));

        addChild(createLightCentered<AcrylicChannel12>(millimetersToPixelsVec(39.77, 37.927), module, Scalaria::LIGHT_CHANNEL_12));

        addChild(createLightCentered<AcrylicChannel13>(millimetersToPixelsVec(39.77, 34.927), module, Scalaria::LIGHT_CHANNEL_13));

        addChild(createLightCentered<AcrylicChannel14>(millimetersToPixelsVec(39.77, 31.927), module, Scalaria::LIGHT_CHANNEL_14));

        addChild(createLightCentered<AcrylicChannel15>(millimetersToPixelsVec(39.77, 28.927), module, Scalaria::LIGHT_CHANNEL_15));

        addChild(createLightCentered<AcrylicChannel16>(millimetersToPixelsVec(39.77, 25.927), module, Scalaria::LIGHT_CHANNEL_16));

        SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 22.86, 91.457);
        addChild(bloodLogo);
    }
};

Model* modelScalaria = createModel<Scalaria, ScalariaWidget>("Sanguine-Scalaria");