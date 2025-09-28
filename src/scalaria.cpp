#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

#include "scalaria/dsp/scalaria_modulator.h"

#include "warpiescommon.hpp"
#include "scalaria.hpp"

using simd::float_4;

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
        LIGHT_INTERNAL_OSCILLATOR_OFF,
        LIGHT_INTERNAL_OSCILLATOR_TRIANGLE,
        LIGHT_INTERNAL_OSCILLATOR_SAW,
        LIGHT_INTERNAL_OSCILLATOR_SQUARE,
        LIGHT_CHANNEL_1_FREQUENCY,
        LIGHT_CHANNEL_1_LEVEL,
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

    short frames[PORT_MAX_CHANNELS] = {};

    static const int kLightsFrequency = 128;
    int jitteredLightsFrequency;

    int32_t internalOscillator = 0.f;

    dsp::ClockDivider lightsDivider;
    scalaria::ScalariaModulator modulators[PORT_MAX_CHANNELS];
    scalaria::ShortFrame inputFrames[PORT_MAX_CHANNELS][warpiescommon::kBlockSize];
    scalaria::ShortFrame outputFrames[PORT_MAX_CHANNELS][warpiescommon::kBlockSize];

    scalaria::Parameters* parameters[PORT_MAX_CHANNELS];

    float knobFrequency = 0.f;
    float knobResonance = 0.5f;

    float_4 f4KnobValues;

    Scalaria() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

        configParam(PARAM_FREQUENCY, 0.f, 1.f, 0.f, "Cutoff frequency");

        configParam(PARAM_FREQUENCY_CV_ATTENUVERTER, -1.f, 1.f, 0.f, "Cutoff frequency CV");

        configParam(PARAM_RESONANCE, 0.f, 1.f, 0.5f, "Resonance", "%", 0, 100);

        configParam(PARAM_RESONANCE_CV_ATTENUVERTER, -1.f, 1.f, 0.f, "Resonance CV");

        configSwitch(PARAM_INTERNAL_OSCILLATOR, 0.f, 3.f, 0.f, "Internal oscillator", scalaria::oscillatorNames);

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
            memset(&modulators[channel], 0, sizeof(scalaria::ScalariaModulator));
            modulators[channel].Init(scalaria::kHardwareRate);
            parameters[channel] = modulators[channel].mutableParameters();
        }

        f4KnobValues[0] = 0.66f;
        f4KnobValues[1] = 0.66f;
        f4KnobValues[2] = 0.f;
        f4KnobValues[3] = 0.f;
    }

    void process(const ProcessArgs& args) override {
        int channelCount = std::max(std::max(inputs[INPUT_CHANNEL_1].getChannels(),
            inputs[INPUT_CHANNEL_2].getChannels()), 1);

        float_4 f4Voltages;

        knobFrequency = params[PARAM_FREQUENCY].getValue();
        knobResonance = params[PARAM_RESONANCE].getValue();

        internalOscillator = params[PARAM_INTERNAL_OSCILLATOR].getValue();

        f4KnobValues[0] = params[PARAM_CHANNEL_1_LEVEL].getValue();
        f4KnobValues[1] = params[PARAM_CHANNEL_2_LEVEL].getValue();
        f4KnobValues[2] = params[PARAM_FREQUENCY_CV_ATTENUVERTER].getValue();
        f4KnobValues[3] = params[PARAM_RESONANCE_CV_ATTENUVERTER].getValue();

        for (int channel = 0; channel < channelCount; ++channel) {
            parameters[channel]->oscillatorShape = internalOscillator;

            // Buffer loop.
            if (++frames[channel] >= warpiescommon::kBlockSize) {
                frames[channel] = 0;

                // CHANNEL_1_LEVEL and CHANNEL_2_LEVEL are normalized values: from cv_scaler.cc and a PR by Brian Head to AI's repository.
                f4Voltages[0] = inputs[INPUT_CHANNEL_1_LEVEL].getNormalVoltage(5.f, channel);
                f4Voltages[1] = inputs[INPUT_CHANNEL_2_LEVEL].getNormalVoltage(5.f, channel);
                f4Voltages[2] = inputs[INPUT_FREQUENCY].getVoltage(channel);
                f4Voltages[3] = inputs[INPUT_RESONANCE].getVoltage(channel);

                f4Voltages /= 5.f;

                f4Voltages *= f4KnobValues;

                f4Voltages[2] += knobFrequency;
                f4Voltages[3] += knobResonance;

                f4Voltages = simd::clamp(f4Voltages, 0.f, 1.f);

                parameters[channel]->channel_drive[0] = f4Voltages[0];
                parameters[channel]->channel_drive[1] = f4Voltages[1];

                parameters[channel]->rawFrequency = f4Voltages[2];

                parameters[channel]->rawResonance = f4Voltages[3];

                parameters[channel]->note = 60.f * f4KnobValues[0] + 12.f *
                    inputs[INPUT_CHANNEL_1_LEVEL].getNormalVoltage(2.f, channel) + 12.f;
                parameters[channel]->note += log2f(scalaria::kHardwareRate * args.sampleTime) * 12.f;

                modulators[channel].Process(inputFrames[channel], outputFrames[channel], warpiescommon::kBlockSize);
            }
        }

        float_4 inVoltagesChannel1;
        float_4 inVoltagesChannel2;
        float_4 outVoltagesChannel1;
        float_4 outVoltagesAux;
        int currentChannel;
        for (int channel = 0; channel < channelCount; channel += 4) {
            inVoltagesChannel1 = inputs[INPUT_CHANNEL_1].getVoltageSimd<float_4>(channel);
            inVoltagesChannel2 = inputs[INPUT_CHANNEL_2].getVoltageSimd<float_4>(channel);

            inVoltagesChannel1 /= 8.f;
            inVoltagesChannel2 /= 8.f;
            inVoltagesChannel1 *= 32768.f;
            inVoltagesChannel2 *= 32768.f;

            inVoltagesChannel1 = simd::clamp(inVoltagesChannel1, -32768.f, 32767.f);
            inVoltagesChannel2 = simd::clamp(inVoltagesChannel2, -32768.f, 32767.f);

            inputFrames[channel][frames[channel]].l = static_cast<short>(inVoltagesChannel1[0]);
            inputFrames[channel][frames[channel]].r = static_cast<short>(inVoltagesChannel2[0]);
            outVoltagesChannel1[0] = static_cast<float>(outputFrames[channel][frames[channel]].l);
            outVoltagesAux[0] = static_cast<float>(outputFrames[channel][frames[channel]].r);

            currentChannel = channel + 1;
            inputFrames[currentChannel][frames[currentChannel]].l = static_cast<short>(inVoltagesChannel1[1]);
            inputFrames[currentChannel][frames[currentChannel]].r = static_cast<short>(inVoltagesChannel2[1]);
            outVoltagesChannel1[1] = static_cast<float>(outputFrames[currentChannel][frames[currentChannel]].l);
            outVoltagesAux[1] = static_cast<float>(outputFrames[currentChannel][frames[currentChannel]].r);

            currentChannel = channel + 2;
            inputFrames[currentChannel][frames[currentChannel]].l = static_cast<short>(inVoltagesChannel1[2]);
            inputFrames[currentChannel][frames[currentChannel]].r = static_cast<short>(inVoltagesChannel2[2]);
            outVoltagesChannel1[2] = static_cast<float>(outputFrames[currentChannel][currentChannel].l);
            outVoltagesAux[2] = static_cast<float>(outputFrames[currentChannel][frames[currentChannel]].r);

            currentChannel = channel + 3;
            inputFrames[currentChannel][frames[currentChannel]].l = static_cast<short>(inVoltagesChannel1[3]);
            inputFrames[currentChannel][frames[currentChannel]].r = static_cast<short>(inVoltagesChannel2[3]);
            outVoltagesChannel1[3] = static_cast<float>(outputFrames[currentChannel][currentChannel].l);
            outVoltagesAux[3] = static_cast<float>(outputFrames[currentChannel][frames[currentChannel]].r);

            outVoltagesChannel1 /= 32768.f;
            outVoltagesAux /= 32768.f;
            outVoltagesChannel1 *= 5.f;
            outVoltagesAux *= 5.f;

            outputs[OUTPUT_CHANNEL_1_PLUS_2].setVoltageSimd(outVoltagesChannel1, channel);
            outputs[OUTPUT_AUX].setVoltageSimd(outVoltagesAux, channel);
        }

        outputs[OUTPUT_CHANNEL_1_PLUS_2].setChannels(channelCount);
        outputs[OUTPUT_AUX].setChannels(channelCount);

        if (lightsDivider.process()) {
            const float sampleTime = jitteredLightsFrequency * args.sampleTime;

            bool bHaveInternalOscillator = internalOscillator != 0;

            lights[LIGHT_INTERNAL_OSCILLATOR_OFF].setBrightnessSmooth(!bHaveInternalOscillator * kSanguineButtonLightValue,
                sampleTime);

            lights[LIGHT_INTERNAL_OSCILLATOR_TRIANGLE].setBrightnessSmooth((internalOscillator == 1) *
                kSanguineButtonLightValue, sampleTime);

            lights[LIGHT_INTERNAL_OSCILLATOR_SAW].setBrightnessSmooth((internalOscillator == 2) *
                kSanguineButtonLightValue, sampleTime);

            lights[LIGHT_INTERNAL_OSCILLATOR_SQUARE].setBrightnessSmooth((internalOscillator == 3) *
                kSanguineButtonLightValue, sampleTime);

            lights[LIGHT_CHANNEL_1_FREQUENCY].setBrightnessSmooth(bHaveInternalOscillator *
                kSanguineButtonLightValue, sampleTime);

            lights[LIGHT_CHANNEL_1_LEVEL].setBrightnessSmooth(!bHaveInternalOscillator *
                kSanguineButtonLightValue, sampleTime);

            for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
                const int currentLight = LIGHT_CHANNEL_1 + channel * 3;

                if (channel < channelCount) {
                    float zone;

                    zone = 8.f * parameters[channel]->rawFrequency;
                    MAKE_INTEGRAL_FRACTIONAL(zone);
                    int integerZoneFractional = static_cast<int>(zone_fractional * 256);

                    int aRed = scalaria::paletteFrequencies[zone_integral][0];
                    int bRed = scalaria::paletteFrequencies[zone_integral + 1][0];

                    int aGreen = scalaria::paletteFrequencies[zone_integral][1];
                    int bGreen = scalaria::paletteFrequencies[zone_integral + 1][1];

                    int aBlue = scalaria::paletteFrequencies[zone_integral][2];
                    int bBlue = scalaria::paletteFrequencies[zone_integral + 1][2];

                    float_4 colorValues;

                    colorValues[0] = static_cast<float>(aRed + ((bRed - aRed) * integerZoneFractional >> 8));
                    colorValues[1] = static_cast<float>(aGreen + ((bGreen - aGreen) * integerZoneFractional >> 8));
                    colorValues[2] = static_cast<float>(aBlue + ((bBlue - aBlue) * integerZoneFractional >> 8));

                    colorValues /= 255.f;

                    colorValues = simd::rescale(colorValues, 0.f, 1.f, 0.f, kSanguineButtonLightValue);

                    lights[currentLight].setBrightnessSmooth(colorValues[0], sampleTime);
                    lights[currentLight + 1].setBrightnessSmooth(colorValues[1], sampleTime);
                    lights[currentLight + 2].setBrightnessSmooth(colorValues[2], sampleTime);
                } else {
                    lights[currentLight].setBrightnessSmooth(0, sampleTime);
                    lights[currentLight + 1].setBrightnessSmooth(0, sampleTime);
                    lights[currentLight + 2].setBrightnessSmooth(0, sampleTime);
                }
            }
        }
    }

    void onAdd(const AddEvent& e) override {
        jitteredLightsFrequency = kLightsFrequency + (getId() % kLightsFrequency);
        lightsDivider.setDivision(jitteredLightsFrequency);
    }
};

#ifndef METAMODULE
struct AcrylicOff : SanguineShapedAcrylicLed<BlueLight> {
    AcrylicOff() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_off.svg")));
    }
};

struct AcrylicTriangle : SanguineShapedAcrylicLed<GreenLight> {
    AcrylicTriangle() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_triangle.svg")));
    }
};

struct AcrylicSaw : SanguineShapedAcrylicLed<YellowLight> {
    AcrylicSaw() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_saw.svg")));
    }
};

struct AcrylicSquare : SanguineShapedAcrylicLed<RedLight> {
    AcrylicSquare() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_square.svg")));
    }
};

struct AcrylicFreq : SanguineShapedAcrylicLed<GreenLight> {
    AcrylicFreq() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_freq.svg")));
    }
};

struct AcrylicLvl1 : SanguineShapedAcrylicLed<GreenLight> {
    AcrylicLvl1() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_lvl1.svg")));
    }
};

struct AcrylicChannel1 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel1() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_1.svg")));
    }
};

struct AcrylicChannel2 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel2() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_2.svg")));
    }
};

struct AcrylicChannel3 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel3() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_3.svg")));
    }
};

struct AcrylicChannel4 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel4() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_4.svg")));
    }
};

struct AcrylicChannel5 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel5() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_5.svg")));
    }
};

struct AcrylicChannel6 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel6() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_6.svg")));
    }
};

struct AcrylicChannel7 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel7() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_7.svg")));
    }
};

struct AcrylicChannel8 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel8() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_8.svg")));
    }
};

struct AcrylicChannel9 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel9() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_9.svg")));
    }
};

struct AcrylicChannel10 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel10() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_10.svg")));
    }
};

struct AcrylicChannel11 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel11() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_11.svg")));
    }
};

struct AcrylicChannel12 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel12() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_12.svg")));
    }
};

struct AcrylicChannel13 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel13() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_13.svg")));
    }
};

struct AcrylicChannel14 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel14() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_14.svg")));
    }
};

struct AcrylicChannel15 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel15() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_15.svg")));
    }
};

struct AcrylicChannel16 : SanguineShapedAcrylicLed<RedGreenBlueLight> {
    AcrylicChannel16() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/sanguine_acrylic_channel_16.svg")));
    }
};
#endif

struct ScalariaWidget : SanguineModuleWidget {
    explicit ScalariaWidget(Scalaria* module) {
        setModule(module);

        moduleName = "scalaria";
        panelSize = sanguineThemes::SIZE_9;
        backplateColor = sanguineThemes::PLATE_PURPLE;

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

#ifndef METAMODULE
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
#else
        addChild(createLightCentered<TinyLight<BlueLight>>(millimetersToPixelsVec(13.087, 75.676), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_OFF));

        addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(13.087, 63.997), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_TRIANGLE));

        addChild(createLightCentered<TinyLight<YellowLight>>(millimetersToPixelsVec(32.644, 63.997), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_SAW));

        addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(32.644, 75.676), module, Scalaria::LIGHT_INTERNAL_OSCILLATOR_SQUARE));

        addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(6.172, 93.944), module, Scalaria::LIGHT_CHANNEL_1_FREQUENCY));

        addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(17.061, 93.944), module, Scalaria::LIGHT_CHANNEL_1_LEVEL));

        int lightY = 70.927;

        for (int light = 0; light < PORT_MAX_CHANNELS; ++light) {
            addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(40.759, lightY),
                module, Scalaria::LIGHT_CHANNEL_1 + light * 3));
            ++lightY;
        }
#endif

#ifndef METAMODULE
        SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 22.86, 91.457);
        addChild(bloodLogo);
#endif
    }
};

Model* modelScalaria = createModel<Scalaria, ScalariaWidget>("Sanguine-Scalaria");