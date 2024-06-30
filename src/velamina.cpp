#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinedsp.hpp"
#include "sanguinehelpers.hpp"
using simd::float_4;

struct Velamina : Module {
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
		LIGHT_GAIN_1,
		LIGHT_GAIN_2,
		LIGHT_GAIN_3,
		LIGHT_GAIN_4,
		ENUMS(LIGHT_OUT_1, 3),
		ENUMS(LIGHT_OUT_2, 3),
		ENUMS(LIGHT_OUT_3, 3),
		ENUMS(LIGHT_OUT_4, 3),
		LIGHTS_COUNT
	};

	dsp::ClockDivider lightsDivider;
	SaturatorFloat_4 saturator;

	const int kLightsDivider = 64;

	Velamina() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int i = 0; i < 4; i++) {
			configParam(PARAM_GAIN_1 + i, 0.f, 1.f, 0.f, string::f("Channel %d gain", i + 1), "%", 0, 100);
			configParam(PARAM_RESPONSE_1 + i, 0.f, 1.f, 0.f, string::f("Channel %d response", i + 1));
			configParam(PARAM_OFFSET_1 + i, 0.f, 5.f, 0.f, string::f("Channel %d CV offset", i + 1), "V");
			configInput(INPUT_IN_1 + i, string::f("Channel %d", i + 1));
			configInput(INPUT_CV_1 + i, string::f("Channel %d CV", i + 1));
			configOutput(OUTPUT_1 + i, string::f("Channel %d", i + 1));
		}

		lightsDivider.setDivision(kLightsDivider);
	}

	void process(const ProcessArgs& args) override {
		float_4 outVoltages[4] = {};
		float_4 portVoltages[4][4] = {};
		int channelCount = 1;

		bool bIsLightsTurn = lightsDivider.process();

		if (inputs[INPUT_IN_1].isConnected() || inputs[INPUT_IN_2].isConnected() || inputs[INPUT_IN_3].isConnected() || inputs[INPUT_IN_4].isConnected()) {
			channelCount = std::max(std::max(std::max(inputs[INPUT_IN_1].getChannels(), inputs[INPUT_IN_2].getChannels()),
				std::max(inputs[INPUT_IN_3].getChannels(), inputs[INPUT_IN_4].getChannels())), 1);
		}

		for (int i = 0; i < 4; i++) {
			outputs[OUTPUT_1 + i].setChannels(channelCount);

			float_4 linear[4] = {};
			float_4 exponential[4] = {};
			float_4 modulationCV[4] = {};
			float_4 gain[4] = {};
			float_4 inVoltages[4] = {};

			for (int channel = 0; channel < channelCount; channel += 4) {
				uint8_t currentChannel = channel >> 2;

				if (inputs[INPUT_CV_1 + i].isConnected()) {
					linear[currentChannel] = inputs[INPUT_CV_1 + i].getNormalVoltageSimd<float_4>(5.f, channel) / 5.f;
					linear[currentChannel] = simd::clamp(linear[currentChannel], 0.f, 2.f);
					const float_4 base = 200.f;
					exponential[currentChannel] = simd::rescale(simd::pow(base, linear[currentChannel] / 2.f), 1.f, base, 0.f, 10.f);
					modulationCV[currentChannel] = simd::crossfade(exponential[currentChannel], linear[currentChannel], params[PARAM_RESPONSE_1 + i].getValue());

					gain[currentChannel] = modulationCV[currentChannel] * params[PARAM_GAIN_1 + i].getValue() + params[PARAM_OFFSET_1 + i].getValue();
				}
				else {
					gain[currentChannel] = params[PARAM_GAIN_1 + i].getValue() + params[PARAM_OFFSET_1 + i].getValue();
				}

				if (inputs[INPUT_IN_1 + i].isConnected()) {
					inVoltages[currentChannel] = inputs[INPUT_IN_1 + i].getVoltageSimd<float_4>(channel);

					float_4 voltages = gain[currentChannel] * inVoltages[currentChannel];

					outVoltages[currentChannel] += voltages;

					float_4 isAbove10 = simd::abs(outVoltages[currentChannel]) > 10.f;

					outVoltages[currentChannel] = simd::ifelse(isAbove10, saturator.next(outVoltages[currentChannel]), outVoltages[currentChannel]);
					portVoltages[i][currentChannel] = outVoltages[currentChannel];
				}
				else {
					portVoltages[i][currentChannel] = outVoltages[currentChannel];
				}


				if (outputs[OUTPUT_1 + i].isConnected()) {
					outputs[OUTPUT_1 + i].setVoltageSimd(outVoltages[currentChannel], channel);
					outVoltages[currentChannel] = 0.f;
				}
			}

			if (bIsLightsTurn) {
				const float sampleTime = kLightsDivider * args.sampleTime;

				int currentLight = LIGHT_OUT_1 + i * 3;
				float voltageSum = 0.f;

				if (channelCount < 2) {
					if (channelCount > 0) {
						voltageSum = voltageSum = portVoltages[i][0][0];

						voltageSum = clamp(voltageSum, -10.f, 10.f);
					}

					lights[currentLight + 0].setBrightnessSmooth(rescale(-voltageSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(rescale(voltageSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
				}
				else {
					for (int offset = 0; offset < 4; offset++) {
						for (int channel = 0; channel < 4; channel++) {
							voltageSum += portVoltages[i][offset][channel];
						}
					}

					voltageSum = voltageSum / channelCount;

					voltageSum = clamp(voltageSum, -10.f, 10.f);

					float redValue = rescale(-voltageSum, 0.f, 10.f, 0.f, 1.f);
					float greenValue = rescale(voltageSum, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(voltageSum < 0 ? redValue : greenValue, sampleTime);
				}

				lights[LIGHT_GAIN_1 + i].setBrightnessSmooth(rescale(gain[0][0], 0.f, 5.f, 0.f, 1.f), sampleTime);
			}
		}
	}
};


struct VelaminaWidget : ModuleWidget {
	VelaminaWidget(Velamina* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_10hp_purple.svg", "res/velamina_faceplate.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(8.326, 19.457), module, Velamina::PARAM_RESPONSE_1));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(19.82, 19.457), module, Velamina::PARAM_RESPONSE_2));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(31.323, 19.457), module, Velamina::PARAM_RESPONSE_3));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(42.837, 19.457), module, Velamina::PARAM_RESPONSE_4));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(8.326, 31.814), module, Velamina::PARAM_OFFSET_1));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(19.82, 31.814), module, Velamina::PARAM_OFFSET_2));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(31.323, 31.814), module, Velamina::PARAM_OFFSET_3));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(42.837, 31.814), module, Velamina::PARAM_OFFSET_4));

		addParam(createLightParamCentered<VCVLightSlider<RedLight>>(millimetersToPixelsVec(8.326, 52.697),
			module, Velamina::PARAM_GAIN_1, Velamina::LIGHT_GAIN_1));
		addParam(createLightParamCentered<VCVLightSlider<RedLight>>(millimetersToPixelsVec(19.82, 52.697),
			module, Velamina::PARAM_GAIN_2, Velamina::LIGHT_GAIN_2));
		addParam(createLightParamCentered<VCVLightSlider<RedLight>>(millimetersToPixelsVec(31.323, 52.697),
			module, Velamina::PARAM_GAIN_3, Velamina::LIGHT_GAIN_3));
		addParam(createLightParamCentered<VCVLightSlider<RedLight>>(millimetersToPixelsVec(42.837, 52.697),
			module, Velamina::PARAM_GAIN_4, Velamina::LIGHT_GAIN_4));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(8.326, 80.55), module, Velamina::INPUT_CV_1));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(19.82, 80.55), module, Velamina::INPUT_CV_2));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(31.323, 80.55), module, Velamina::INPUT_CV_3));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(42.837, 80.55), module, Velamina::INPUT_CV_4));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(8.326, 96.59), module, Velamina::INPUT_IN_1));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(19.82, 96.59), module, Velamina::INPUT_IN_2));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(31.323, 96.59), module, Velamina::INPUT_IN_3));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(42.837, 96.59), module, Velamina::INPUT_IN_4));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(4.626, 105.454),
			module, Velamina::LIGHT_OUT_1));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.142, 105.454),
			module, Velamina::LIGHT_OUT_2));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(27.649, 105.454),
			module, Velamina::LIGHT_OUT_3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(39.145, 105.454),
			module, Velamina::LIGHT_OUT_4));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(8.326, 112.956), module, Velamina::OUTPUT_1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(19.82, 112.956), module, Velamina::OUTPUT_2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(31.323, 112.956), module, Velamina::OUTPUT_3));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(42.837, 112.956), module, Velamina::OUTPUT_4));
	}
};

Model* modelVelamina = createModel<Velamina, VelaminaWidget>("Sanguine-Velamina");