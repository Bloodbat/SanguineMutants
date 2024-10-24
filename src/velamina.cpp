#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinedsp.hpp"
#include "sanguinehelpers.hpp"
using simd::float_4;

struct Velamina : SanguineModule {
	enum ParamIds {
		PARAM_GAIN_1,
		PARAM_GAIN_2,
		PARAM_GAIN_3,
		PARAM_GAIN_4,
		PARAM_RESPONSE_1,
		PARAM_RESPONSE_2,
		PARAM_RESPONSE_3,
		PARAM_RESPONSE_4,
		PARAM_OFFSET_1,
		PARAM_OFFSET_2,
		PARAM_OFFSET_3,
		PARAM_OFFSET_4,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_IN_1,
		INPUT_IN_2,
		INPUT_IN_3,
		INPUT_IN_4,
		INPUT_CV_1,
		INPUT_CV_2,
		INPUT_CV_3,
		INPUT_CV_4,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_1,
		OUTPUT_2,
		OUTPUT_3,
		OUTPUT_4,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHT_GAIN_1, 2),
		ENUMS(LIGHT_GAIN_2, 2),
		ENUMS(LIGHT_GAIN_3, 2),
		ENUMS(LIGHT_GAIN_4, 2),
		ENUMS(LIGHT_OUT_1, 3),
		ENUMS(LIGHT_OUT_2, 3),
		ENUMS(LIGHT_OUT_3, 3),
		ENUMS(LIGHT_OUT_4, 3),
		LIGHTS_COUNT
	};

	dsp::ClockDivider lightsDivider;
	SaturatorFloat_4 saturator;

	static const int kLightsDivider = 64;
	static const int kMaxChannels = 4;

	Velamina() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int channel = 0; channel < kMaxChannels; ++channel) {
			configParam(PARAM_GAIN_1 + channel, 0.f, 1.f, 0.f, string::f("Channel %d gain", channel + 1), "%", 0.f, 100.f);
			configParam(PARAM_RESPONSE_1 + channel, 0.f, 1.f, 0.f, string::f("Channel %d response (Exponential <-> Linear)", channel + 1));
			configParam(PARAM_OFFSET_1 + channel, 0.f, 5.f, 0.f, string::f("Channel %d CV offset", channel + 1), "V");
			configInput(INPUT_IN_1 + channel, string::f("Channel %d", channel + 1));
			configInput(INPUT_CV_1 + channel, string::f("Channel %d CV", channel + 1));
			configOutput(OUTPUT_1 + channel, string::f("Channel %d", channel + 1));
		}

		lightsDivider.setDivision(kLightsDivider);
	}

	void process(const ProcessArgs& args) override {
		float_4 outVoltages[kMaxChannels] = {};
		float_4 portVoltages[kMaxChannels][4] = {};
		float sampleTime;
		int polyChannelCount = 1;

		bool bIsLightsTurn = lightsDivider.process();

		if (bIsLightsTurn) {
			sampleTime = kLightsDivider * args.sampleTime;
		}

		if (inputs[INPUT_IN_1].isConnected() || inputs[INPUT_IN_2].isConnected() || inputs[INPUT_IN_3].isConnected() || inputs[INPUT_IN_4].isConnected()) {
			polyChannelCount = std::max(std::max(std::max(inputs[INPUT_IN_1].getChannels(), inputs[INPUT_IN_2].getChannels()),
				std::max(inputs[INPUT_IN_3].getChannels(), inputs[INPUT_IN_4].getChannels())), 1);
		}

		for (int channel = 0; channel < kMaxChannels; ++channel) {
			outputs[OUTPUT_1 + channel].setChannels(polyChannelCount);

			float_4 gain[4] = {};
			float_4 inVoltages[4] = {};

			for (int polyChannel = 0; polyChannel < polyChannelCount; polyChannel += 4) {
				uint8_t currentChannel = polyChannel >> 2;
				if (inputs[INPUT_CV_1 + channel].isConnected()) {
					// From graph here: https://www.desmos.com/calculator/hfy87xjw7u referenced by the hardware's manual.
					gain[currentChannel] = simd::fmax(simd::clamp((inputs[INPUT_CV_1 + channel].getVoltageSimd<float_4>(polyChannel) *
						params[PARAM_GAIN_1 + channel].getValue() + params[PARAM_OFFSET_1 + channel].getValue()), 0.f, 8.f) / 5.f, 0.f);
					gain[currentChannel] = simd::pow(gain[currentChannel], 1 / (0.1f + 0.9f * params[PARAM_RESPONSE_1 + channel].getValue()));
				} else {
					gain[currentChannel] = params[PARAM_GAIN_1 + channel].getValue() + params[PARAM_OFFSET_1 + channel].getValue();
				}

				if (inputs[INPUT_IN_1 + channel].isConnected()) {
					inVoltages[currentChannel] = inputs[INPUT_IN_1 + channel].getVoltageSimd<float_4>(polyChannel);

					float_4 voltages = gain[currentChannel] * inVoltages[currentChannel];

					outVoltages[currentChannel] += voltages;

					float_4 isAbove10 = simd::abs(outVoltages[currentChannel]) > 10.f;

					outVoltages[currentChannel] = simd::ifelse(isAbove10, saturator.next(outVoltages[currentChannel]), outVoltages[currentChannel]);
				}
				portVoltages[channel][currentChannel] = outVoltages[currentChannel];


				if (outputs[OUTPUT_1 + channel].isConnected()) {
					outputs[OUTPUT_1 + channel].setVoltageSimd(outVoltages[currentChannel], polyChannel);
					outVoltages[currentChannel] = 0.f;
				}
			}

			if (bIsLightsTurn) {
				int currentLight = LIGHT_OUT_1 + channel * 3;
				float voltageSum = 0.f;
				float gainSum = 0.f;

				if (polyChannelCount < 2) {
					if (polyChannelCount > 0) {
						voltageSum = clamp(portVoltages[channel][0][0], -10.f, 10.f);
					}

					float rescaledLight = rescale(voltageSum, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight + 0].setBrightnessSmooth(-rescaledLight, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);

					lights[(LIGHT_GAIN_1 + channel * 2) + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[(LIGHT_GAIN_1 + channel * 2) + 1].setBrightnessSmooth(rescale(gain[0][0], 0.f, 5.f, 0.f, 1.f), sampleTime);
				} else {
					for (int offset = 0; offset < kMaxChannels; ++offset) {
						for (int channel = 0; channel < kMaxChannels; ++channel) {
							voltageSum += portVoltages[channel][offset][channel];
							gainSum += gain[offset][channel];
						}
					}

					voltageSum /= polyChannelCount;
					gainSum /= polyChannelCount;

					voltageSum = clamp(voltageSum, -10.f, 10.f);

					float rescaledLight = rescale(voltageSum, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight + 0].setBrightnessSmooth(-rescaledLight, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(voltageSum < 0 ? -rescaledLight : rescaledLight, sampleTime);

					rescaledLight = rescale(clamp(gainSum, 0.f, 5.f), 0.f, 5.f, 0.f, 5.f);
					lights[(LIGHT_GAIN_1 + channel * 2) + 0].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[(LIGHT_GAIN_1 + channel * 2) + 1].setBrightnessSmooth(rescaledLight, sampleTime);
				}
			}
		}
	}
};


struct VelaminaWidget : SanguineModuleWidget {
	VelaminaWidget(Velamina* module) {
		setModule(module);

		moduleName = "velamina";
		panelSize = SIZE_10;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(8.326, 19.457), module, Velamina::PARAM_RESPONSE_1));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(19.82, 19.457), module, Velamina::PARAM_RESPONSE_2));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(31.323, 19.457), module, Velamina::PARAM_RESPONSE_3));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(42.837, 19.457), module, Velamina::PARAM_RESPONSE_4));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(8.326, 31.814), module, Velamina::PARAM_OFFSET_1));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(19.82, 31.814), module, Velamina::PARAM_OFFSET_2));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(31.323, 31.814), module, Velamina::PARAM_OFFSET_3));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(42.837, 31.814), module, Velamina::PARAM_OFFSET_4));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(8.326, 52.697),
			module, Velamina::PARAM_GAIN_1, Velamina::LIGHT_GAIN_1));
		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(19.82, 52.697),
			module, Velamina::PARAM_GAIN_2, Velamina::LIGHT_GAIN_2));
		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(31.323, 52.697),
			module, Velamina::PARAM_GAIN_3, Velamina::LIGHT_GAIN_3));
		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(42.837, 52.697),
			module, Velamina::PARAM_GAIN_4, Velamina::LIGHT_GAIN_4));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.326, 80.55), module, Velamina::INPUT_CV_1));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(19.82, 80.55), module, Velamina::INPUT_CV_2));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(31.323, 80.55), module, Velamina::INPUT_CV_3));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(42.837, 80.55), module, Velamina::INPUT_CV_4));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(8.326, 96.59), module, Velamina::INPUT_IN_1));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(19.82, 96.59), module, Velamina::INPUT_IN_2));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(31.323, 96.59), module, Velamina::INPUT_IN_3));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(42.837, 96.59), module, Velamina::INPUT_IN_4));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(4.626, 105.454),
			module, Velamina::LIGHT_OUT_1));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.142, 105.454),
			module, Velamina::LIGHT_OUT_2));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(27.649, 105.454),
			module, Velamina::LIGHT_OUT_3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(39.145, 105.454),
			module, Velamina::LIGHT_OUT_4));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(8.326, 112.956), module, Velamina::OUTPUT_1));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(19.82, 112.956), module, Velamina::OUTPUT_2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(31.323, 112.956), module, Velamina::OUTPUT_3));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(42.837, 112.956), module, Velamina::OUTPUT_4));
	}
};

Model* modelVelamina = createModel<Velamina, VelaminaWidget>("Sanguine-Velamina");