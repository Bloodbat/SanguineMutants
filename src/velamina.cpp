#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinedsp.hpp"
#include "sanguinehelpers.hpp"

using simd::float_4;

using namespace sanguineCommonCode;

const float_4 SaturatorFloat_4::limit = 10.f;

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

	bool signalInputsConnected[kMaxChannels];
	bool cvInputsConnected[kMaxChannels];
	bool outputsConnected[kMaxChannels];

	Velamina() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		for (int channel = 0; channel < kMaxChannels; ++channel) {
			int channelNumber = channel + 1;
			configParam(PARAM_GAIN_1 + channel, 0.f, 1.f, 0.f,
				string::f("Channel %d gain", channelNumber), "%", 0.f, 100.f);
			configParam(PARAM_RESPONSE_1 + channel, 0.f, 1.f, 0.f,
				string::f("Channel %d response (Exponential <-> Linear)", channelNumber));
			configParam(PARAM_OFFSET_1 + channel, 0.f, 5.f, 0.f,
				string::f("Channel %d CV offset", channelNumber), "V");
			configInput(INPUT_IN_1 + channel, string::f("Channel %d", channelNumber));
			configInput(INPUT_CV_1 + channel, string::f("Channel %d CV", channelNumber));
			configOutput(OUTPUT_1 + channel, string::f("Channel %d", channelNumber));

			signalInputsConnected[channel] = false;
			cvInputsConnected[channel] = false;
			outputsConnected[channel] = false;
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

		if (signalInputsConnected[0] | signalInputsConnected[1] | signalInputsConnected[2] | signalInputsConnected[3]) {
			polyChannelCount = std::max(std::max(std::max(inputs[INPUT_IN_1].getChannels(), inputs[INPUT_IN_2].getChannels()),
				std::max(inputs[INPUT_IN_3].getChannels(), inputs[INPUT_IN_4].getChannels())), 1);
		}

		for (int channel = 0; channel < kMaxChannels; ++channel) {
			float_4 gains[4] = {};
			float_4 inVoltages[4] = {};

			float sliderGain = params[PARAM_GAIN_1 + channel].getValue();
			float knobOffset = params[PARAM_OFFSET_1 + channel].getValue();
			float knobResponse = params[PARAM_RESPONSE_1 + channel].getValue();

			for (int polyChannel = 0; polyChannel < polyChannelCount; polyChannel += 4) {
				uint8_t currentChannel = polyChannel >> 2;
				if (cvInputsConnected[channel]) {
					// From graph here: https://www.desmos.com/calculator/hfy87xjw7u referenced by the hardware's manual.
					gains[currentChannel] = simd::fmax(simd::clamp((inputs[INPUT_CV_1 + channel].getVoltageSimd<float_4>(polyChannel) *
						sliderGain + knobOffset), 0.f, 8.f) / 5.f, 0.f);
					gains[currentChannel] = simd::pow(gains[currentChannel], 1 / (0.1f + 0.9f * knobResponse));
				} else {
					gains[currentChannel] = sliderGain + knobOffset;
				}

				if (signalInputsConnected[channel]) {
					inVoltages[currentChannel] = inputs[INPUT_IN_1 + channel].getVoltageSimd<float_4>(polyChannel);

					float_4 voltages = inVoltages[currentChannel];
					voltages *= gains[currentChannel];

					outVoltages[currentChannel] += voltages;

					float_4 isAbove10 = simd::abs(outVoltages[currentChannel]) > 10.f;

					outVoltages[currentChannel] = simd::ifelse(isAbove10, saturator.next(outVoltages[currentChannel]), outVoltages[currentChannel]);
				}
				portVoltages[channel][currentChannel] = outVoltages[currentChannel];

				if (outputsConnected[channel]) {
					outputs[OUTPUT_1 + channel].setVoltageSimd(outVoltages[currentChannel], polyChannel);
					outVoltages[currentChannel] = 0.f;
				}
			}

			if (bIsLightsTurn) {
				int currentLight = LIGHT_OUT_1 + channel * 3;
				float voltageSum = 0.f;

				if (polyChannelCount < 2) {
					if (polyChannelCount > 0) {
						voltageSum = clamp(portVoltages[channel][0][0], -10.f, 10.f);
					}

					float rescaledLight = rescale(voltageSum, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight].setBrightnessSmooth(-rescaledLight, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);

					lights[LIGHT_GAIN_1 + channel * 2].setBrightnessSmooth(0.f, sampleTime);
					lights[(LIGHT_GAIN_1 + channel * 2) + 1].setBrightnessSmooth(rescale(gains[0][0], 0.f, 5.f, 0.f, 1.f), sampleTime);
				} else {
					float gainSum = 0.f;
					for (int offset = 0; offset < kMaxChannels; ++offset) {
						for (int polyChannel = 0; polyChannel < kMaxChannels; ++polyChannel) {
							voltageSum += portVoltages[polyChannel][offset][polyChannel];
							gainSum += gains[offset][polyChannel];
						}
					}

					voltageSum /= polyChannelCount;
					gainSum /= polyChannelCount;

					voltageSum = clamp(voltageSum, -10.f, 10.f);

					float rescaledLight = rescale(voltageSum, 0.f, 10.f, 0.f, 1.f);
					lights[currentLight].setBrightnessSmooth(-rescaledLight, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(voltageSum < 0 ? -rescaledLight : rescaledLight, sampleTime);

					rescaledLight = rescale(clamp(gainSum, 0.f, 5.f), 0.f, 5.f, 0.f, 1.f);
					lights[LIGHT_GAIN_1 + channel * 2].setBrightnessSmooth(rescaledLight, sampleTime);
					lights[(LIGHT_GAIN_1 + channel * 2) + 1].setBrightnessSmooth(rescaledLight, sampleTime);
				}
			}

			outputs[OUTPUT_1 + channel].setChannels(polyChannelCount);
		}
	}

	void onPortChange(const PortChangeEvent& e) override {
		switch (e.type) {
		case Port::INPUT:
			switch (e.portId) {
			case INPUT_IN_1:
				signalInputsConnected[0] = e.connecting;
				break;

			case INPUT_IN_2:
				signalInputsConnected[1] = e.connecting;
				break;

			case INPUT_IN_3:
				signalInputsConnected[2] = e.connecting;
				break;

			case INPUT_IN_4:
				signalInputsConnected[3] = e.connecting;
				break;

			case INPUT_CV_1:
				cvInputsConnected[0] = e.connecting;
				break;

			case INPUT_CV_2:
				cvInputsConnected[1] = e.connecting;
				break;

			case INPUT_CV_3:
				cvInputsConnected[2] = e.connecting;
				break;

			case INPUT_CV_4:
				cvInputsConnected[3] = e.connecting;
				break;
			}
			break;

		case Port::OUTPUT:
			switch (e.portId) {
			case OUTPUT_1:
				outputsConnected[0] = e.connecting;
				break;

			case OUTPUT_2:
				outputsConnected[1] = e.connecting;
				break;

			case OUTPUT_3:
				outputsConnected[2] = e.connecting;
				break;

			case OUTPUT_4:
				outputsConnected[3] = e.connecting;
				break;
			}
			break;
		}
	}
};


struct VelaminaWidget : SanguineModuleWidget {
	explicit VelaminaWidget(Velamina* module) {
		setModule(module);

		moduleName = "velamina";
		panelSize = sanguineThemes::SIZE_10;
		backplateColor = sanguineThemes::PLATE_PURPLE;

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