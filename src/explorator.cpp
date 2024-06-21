#include "plugin.hpp"
#include "sanguinecomponents.hpp"

using simd::float_4;

struct Explorator : Module {
	enum ParamIds {
		PARAM_HARDWARE,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_1_TO_3,
		INPUT_2_TO_2_A,
		INPUT_2_TO_2_B,
		INPUT_3_TO_1_A,
		INPUT_3_TO_1_B,
		INPUT_3_TO_1_C,
		INPUT_SIGN,
		INPUT_LOGIC_A,
		INPUT_LOGIC_B,
		INPUT_SH_VOLTAGE,
		INPUT_SH_TRIGGER,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_1_TO_3_A,
		OUTPUT_1_TO_3_B,
		OUTPUT_1_TO_3_C,
		OUTPUT_2_TO_2_A,
		OUTPUT_2_TO_2_B,
		OUTPUT_3_TO_1,
		OUTPUT_SIGN_INVERTED,
		OUTPUT_SIGN_HALF_RECTIFIER,
		OUTPUT_SIGN_FULL_RECTIFIER,
		OUTPUT_LOGIC_MIN,
		OUTPUT_LOGIC_MAX,
		OUTPUT_SH_NOISE,
		OUTPUT_SH_VOLTAGE,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHT_1_TO_3, 3),
		ENUMS(LIGHT_2_TO_2, 3),
		ENUMS(LIGHT_3_TO_1, 3),
		ENUMS(LIGHT_SIGN, 3),
		ENUMS(LIGHT_LOGIC, 3),
		ENUMS(LIGHT_SH, 3),
		LIGHT_HARDWARE,
		LIGHTS_COUNT
	};

	const int kLightFrequency = 128;
	dsp::ClockDivider lightDivider;
	dsp::SchmittTrigger stSampleAndHold[16];
	float voltagesSampleAndHold[16] = {};
	int lastSampleAndHoldChannels = 0;

	Explorator() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configInput(INPUT_1_TO_3, "1 to 3");
		configInput(INPUT_2_TO_2_A, "2 to 2 A");
		configInput(INPUT_2_TO_2_B, "2 to 2 B");
		configInput(INPUT_3_TO_1_A, "3 to 1 A");
		configInput(INPUT_3_TO_1_B, "3 to 1 B");
		configInput(INPUT_3_TO_1_C, "3 to 1 C");

		configInput(INPUT_SIGN, "Sign");
		configInput(INPUT_LOGIC_A, "Logic A");
		configInput(INPUT_LOGIC_B, "Logic B");
		configInput(INPUT_SH_VOLTAGE, "Sample and hold voltage");
		configInput(INPUT_SH_TRIGGER, "Sample and hold trigger");

		configOutput(OUTPUT_1_TO_3_A, "1 to 3 A");
		configOutput(OUTPUT_1_TO_3_B, "1 to 3 B");
		configOutput(OUTPUT_1_TO_3_C, "1 to 3 C");
		configOutput(OUTPUT_2_TO_2_A, "2 to 2 A");
		configOutput(OUTPUT_2_TO_2_B, "2 to 2 B");
		configOutput(OUTPUT_3_TO_1, "3 to 1");

		configOutput(OUTPUT_SIGN_INVERTED, "Inverted");
		configOutput(OUTPUT_SIGN_HALF_RECTIFIER, "Half rectified");
		configOutput(OUTPUT_SIGN_FULL_RECTIFIER, "Full rectified");

		configOutput(OUTPUT_LOGIC_MIN, "Minimum");
		configOutput(OUTPUT_LOGIC_MAX, "Maximum");

		configOutput(OUTPUT_SH_NOISE, "Noise");
		configOutput(OUTPUT_SH_VOLTAGE, "Sample and hold voltage");

		configButton(PARAM_HARDWARE, "3:1 hardware behavior (averager)");

		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		bool attenuate3to1 = params[PARAM_HARDWARE].getValue();

		// 1:3
		int channels1to3 = inputs[INPUT_1_TO_3].getChannels();

		outputs[OUTPUT_1_TO_3_A].setChannels(channels1to3);
		outputs[OUTPUT_1_TO_3_B].setChannels(channels1to3);
		outputs[OUTPUT_1_TO_3_C].setChannels(channels1to3);

		if (inputs[INPUT_1_TO_3].isConnected()) {
			float_4 voltages1to3;
			for (int channel = 0; channel < channels1to3; channel += 4) {
				voltages1to3 = inputs[INPUT_1_TO_3].getVoltageSimd<float_4>(channel);
				outputs[OUTPUT_1_TO_3_A].setVoltageSimd(voltages1to3, channel);
				outputs[OUTPUT_1_TO_3_B].setVoltageSimd(voltages1to3, channel);
				outputs[OUTPUT_1_TO_3_C].setVoltageSimd(voltages1to3, channel);
			}
		}

		// Sign
		int channelsSign = inputs[INPUT_SIGN].getChannels();

		outputs[OUTPUT_SIGN_INVERTED].setChannels(channelsSign);
		outputs[OUTPUT_SIGN_HALF_RECTIFIER].setChannels(channelsSign);
		outputs[OUTPUT_SIGN_FULL_RECTIFIER].setChannels(channelsSign);

		if (inputs[INPUT_SIGN].isConnected()) {
			float_4 voltagesSign;
			for (int channel = 0; channel < channelsSign; channel += 4) {
				voltagesSign = inputs[INPUT_SIGN].getVoltageSimd<float_4>(channel);
				outputs[OUTPUT_SIGN_INVERTED].setVoltageSimd(-voltagesSign, channel);
				outputs[OUTPUT_SIGN_HALF_RECTIFIER].setVoltageSimd(fmax(0.f, voltagesSign), channel);
				outputs[OUTPUT_SIGN_FULL_RECTIFIER].setVoltageSimd(fabs(voltagesSign), channel);
			}
		}

		// 2:2
		int channels2to2 = std::max(std::max(inputs[INPUT_2_TO_2_A].getChannels(), inputs[INPUT_2_TO_2_B].getChannels()), 0);

		outputs[OUTPUT_2_TO_2_A].setChannels(channels2to2);
		outputs[OUTPUT_2_TO_2_B].setChannels(channels2to2);

		if (inputs[INPUT_2_TO_2_A].isConnected() || inputs[INPUT_2_TO_2_B].isConnected()) {
			float_4 voltages2to2;
			for (int channel = 0; channel < channels2to2; channel += 4) {
				voltages2to2 = inputs[INPUT_2_TO_2_A].getVoltageSimd<float_4>(channel) + inputs[INPUT_2_TO_2_B].getVoltageSimd<float_4>(channel);
				outputs[OUTPUT_2_TO_2_A].setVoltageSimd(voltages2to2, channel);
				outputs[OUTPUT_2_TO_2_B].setVoltageSimd(voltages2to2, channel);
			}
		}

		// Logic
		int channelsLogic = std::max(std::max(inputs[INPUT_LOGIC_A].getChannels(), inputs[INPUT_LOGIC_B].getChannels()), 0);

		outputs[OUTPUT_LOGIC_MIN].setChannels(channelsLogic);
		outputs[OUTPUT_LOGIC_MAX].setChannels(channelsLogic);

		if (inputs[INPUT_LOGIC_A].isConnected() || inputs[INPUT_LOGIC_B].isConnected()) {
			for (int channel = 0; channel < channelsLogic; channel += 4) {
				float_4 voltagesLogicA = inputs[INPUT_LOGIC_A].getVoltageSimd<float_4>(channel);
				float_4 voltagesLogicB = inputs[INPUT_LOGIC_B].getVoltageSimd<float_4>(channel);
				outputs[OUTPUT_LOGIC_MIN].setVoltageSimd(fmin(voltagesLogicA, voltagesLogicB), channel);
				outputs[OUTPUT_LOGIC_MAX].setVoltageSimd(fmax(voltagesLogicA, voltagesLogicB), channel);
			}
		}

		// 3:1
		int channels3to1 = std::max(std::max(std::max(inputs[INPUT_3_TO_1_A].getChannels(), inputs[INPUT_3_TO_1_B].getChannels()), inputs[INPUT_3_TO_1_C].getChannels()), 0);

		outputs[OUTPUT_3_TO_1].setChannels(channels3to1);

		if (inputs[INPUT_3_TO_1_A].isConnected() || inputs[INPUT_3_TO_1_B].isConnected() || inputs[INPUT_3_TO_1_C].isConnected()) {
			for (int channel = 0; channel < channels3to1; channel += 4) {
				float_4 voltages3to1A = inputs[INPUT_3_TO_1_A].getVoltageSimd<float_4>(channel);
				float_4 voltages3to1B = inputs[INPUT_3_TO_1_B].getVoltageSimd<float_4>(channel);
				float_4 voltages3to1C = inputs[INPUT_3_TO_1_C].getVoltageSimd<float_4>(channel);

				float_4 voltages3to1 = voltages3to1A + voltages3to1B + voltages3to1C;

				if (attenuate3to1) {
					voltages3to1 /= 3;
				}

				outputs[OUTPUT_3_TO_1].setVoltageSimd(voltages3to1, channel);
			}
		}

		// Sample & hold
		float noise = 0.f;
		bool triggerConnected = inputs[INPUT_SH_TRIGGER].isConnected();
		bool noiseConnected = outputs[OUTPUT_SH_NOISE].isConnected();
		bool inputVoltageConnected = inputs[INPUT_SH_VOLTAGE].isConnected();

		int channelsSampleAndHold = inputs[INPUT_SH_TRIGGER].getChannels();

		if (channelsSampleAndHold > 0) {
			lastSampleAndHoldChannels = channelsSampleAndHold;
		}

		outputs[OUTPUT_SH_VOLTAGE].setChannels(lastSampleAndHoldChannels);
		outputs[OUTPUT_SH_NOISE].setChannels(channelsSampleAndHold > 0 ? channelsSampleAndHold : 1);

		if (noiseConnected || (triggerConnected && !inputVoltageConnected)) {
			noise = 2.f * random::normal();
		}

		if (triggerConnected) {
			for (int channel = 0; channel < lastSampleAndHoldChannels; channel++) {
				if (stSampleAndHold[channel].process(inputs[INPUT_SH_TRIGGER].getVoltage(channel))) {
					if (inputVoltageConnected) {
						voltagesSampleAndHold[channel] = inputs[INPUT_SH_VOLTAGE].getVoltage(channel);
					}
					else {
						voltagesSampleAndHold[channel] = noise;
					}
				}
			}
		}

		if (outputs[OUTPUT_SH_VOLTAGE].isConnected() && lastSampleAndHoldChannels > 0) {
			outputs[OUTPUT_SH_VOLTAGE].writeVoltages(voltagesSampleAndHold);
		}

		if (noiseConnected) {
			int channelsNoise = outputs[OUTPUT_SH_NOISE].getChannels();

			for (int channel = 0; channel < channelsNoise; channel += 4) {
				float_4 noiseVoltages = noise;
				outputs[OUTPUT_SH_NOISE].setVoltageSimd(noiseVoltages, channel);
			}
		}


		// Lights

		if (lightDivider.process()) {
			const float sampleTime = args.sampleTime * kLightFrequency;

			// 1:3
			lights[LIGHT_1_TO_3 + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_1_TO_3 + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_1_TO_3 + 2].setBrightnessSmooth(0.f, sampleTime);

			if (channels1to3 > 0) {
				float voltage1to3Sum = inputs[INPUT_1_TO_3].getVoltageSum();

				if (channels1to3 == 1) {
					lights[LIGHT_1_TO_3 + 0].setBrightnessSmooth(math::rescale(-voltage1to3Sum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_1_TO_3 + 1].setBrightnessSmooth(math::rescale(voltage1to3Sum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_1_TO_3 + 2].setBrightnessSmooth(0.0f, sampleTime);
				}
				else {
					float redValue = math::rescale(-voltage1to3Sum, 0.f, 10.f, 0.f, 1.f);
					float greenValue = math::rescale(voltage1to3Sum, 0.f, 10.f, 0.f, 1.f);
					lights[LIGHT_1_TO_3 + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[LIGHT_1_TO_3 + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[LIGHT_1_TO_3 + 2].setBrightnessSmooth(voltage1to3Sum < 0 ? redValue : greenValue, sampleTime);
				}
			}

			// Sign
			lights[LIGHT_SIGN + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_SIGN + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_SIGN + 2].setBrightnessSmooth(0.f, sampleTime);

			if (channelsSign > 0) {
				float voltageSignSum = inputs[INPUT_SIGN].getVoltageSum();

				if (channels1to3 == 1) {
					lights[LIGHT_SIGN + 0].setBrightnessSmooth(math::rescale(-voltageSignSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_SIGN + 1].setBrightnessSmooth(math::rescale(voltageSignSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_SIGN + 2].setBrightnessSmooth(0.0f, sampleTime);
				}
				else {
					float redValue = math::rescale(-voltageSignSum, 0.f, 10.f, 0.f, 1.f);
					float greenValue = math::rescale(voltageSignSum, 0.f, 10.f, 0.f, 1.f);
					lights[LIGHT_SIGN + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[LIGHT_SIGN + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[LIGHT_SIGN + 2].setBrightnessSmooth(voltageSignSum < 0 ? redValue : greenValue, sampleTime);
				}
			}

			// 2:2
			lights[LIGHT_2_TO_2 + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_2_TO_2 + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_2_TO_2 + 2].setBrightnessSmooth(0.f, sampleTime);

			if (channels2to2 > 0) {
				float voltageBSum = clamp(inputs[INPUT_2_TO_2_A].getVoltageSum() + inputs[INPUT_2_TO_2_B].getVoltageSum(), -10.f, 10.f);

				if (channels2to2 == 1) {
					lights[LIGHT_2_TO_2 + 0].setBrightnessSmooth(math::rescale(-voltageBSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_2_TO_2 + 1].setBrightnessSmooth(math::rescale(voltageBSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_2_TO_2 + 2].setBrightnessSmooth(0.0f, sampleTime);
				}
				else {
					float redValue = math::rescale(-voltageBSum, 0.f, 10.f, 0.f, 1.f);
					float greenValue = math::rescale(voltageBSum, 0.f, 10.f, 0.f, 1.f);
					lights[LIGHT_2_TO_2 + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[LIGHT_2_TO_2 + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[LIGHT_2_TO_2 + 2].setBrightnessSmooth(voltageBSum < 0 ? redValue : greenValue, sampleTime);
				}
			}

			// Logic
			lights[LIGHT_LOGIC + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_LOGIC + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_LOGIC + 2].setBrightnessSmooth(0.f, sampleTime);

			if (channelsLogic > 0) {
				float voltageLogicSum = clamp(inputs[INPUT_LOGIC_A].getVoltageSum() + inputs[INPUT_LOGIC_B].getVoltageSum(), -10.f, 10.f);

				if (channelsLogic == 1) {
					lights[LIGHT_LOGIC + 0].setBrightnessSmooth(math::rescale(-voltageLogicSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_LOGIC + 1].setBrightnessSmooth(math::rescale(voltageLogicSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_LOGIC + 2].setBrightnessSmooth(0.0f, sampleTime);
				}
				else {
					float redValue = math::rescale(-voltageLogicSum, 0.f, 10.f, 0.f, 1.f);
					float greenValue = math::rescale(voltageLogicSum, 0.f, 10.f, 0.f, 1.f);
					lights[LIGHT_LOGIC + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[LIGHT_LOGIC + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[LIGHT_LOGIC + 2].setBrightnessSmooth(voltageLogicSum < 0 ? redValue : greenValue, sampleTime);
				}
			}

			// 3:1
			lights[LIGHT_3_TO_1 + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_3_TO_1 + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_3_TO_1 + 2].setBrightnessSmooth(0.f, sampleTime);

			if (channels3to1 > 0) {
				float voltage3to1Out;
				if (!attenuate3to1) {
					voltage3to1Out = clamp(inputs[INPUT_3_TO_1_A].getVoltageSum() + inputs[INPUT_3_TO_1_B].getVoltageSum() + inputs[INPUT_3_TO_1_C].getVoltageSum(), -10.f, 10.f);
				}
				else {
					voltage3to1Out = clamp((inputs[INPUT_3_TO_1_A].getVoltageSum() + inputs[INPUT_3_TO_1_B].getVoltageSum() + inputs[INPUT_3_TO_1_C].getVoltageSum()) / 3, -10.f, 10.f);
				}

				if (channels3to1 == 1) {
					lights[LIGHT_3_TO_1 + 0].setBrightnessSmooth(math::rescale(-voltage3to1Out, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_3_TO_1 + 1].setBrightnessSmooth(math::rescale(voltage3to1Out, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_3_TO_1 + 2].setBrightnessSmooth(0.0f, sampleTime);
				}
				else {
					float redValue = math::rescale(-voltage3to1Out, 0.f, 10.f, 0.f, 1.f);
					float greenValue = math::rescale(voltage3to1Out, 0.f, 10.f, 0.f, 1.f);
					lights[LIGHT_3_TO_1 + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[LIGHT_3_TO_1 + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[LIGHT_3_TO_1 + 2].setBrightnessSmooth(voltage3to1Out < 0 ? redValue : greenValue, sampleTime);
				}
			}

			// Sample and hold
			lights[LIGHT_SH + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_SH + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_SH + 2].setBrightnessSmooth(0.f, sampleTime);

			if (lastSampleAndHoldChannels > 0) {
				float sampleAndHoldVoltageSum = 0;
				for (int i = 0; i < lastSampleAndHoldChannels; i++) {
					sampleAndHoldVoltageSum += voltagesSampleAndHold[i];
				}

				sampleAndHoldVoltageSum = clamp(sampleAndHoldVoltageSum, -10.f, 10.f);

				if (channels3to1 == 1) {
					lights[LIGHT_SH + 0].setBrightnessSmooth(math::rescale(-sampleAndHoldVoltageSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_SH + 1].setBrightnessSmooth(math::rescale(sampleAndHoldVoltageSum, 0.f, 10.f, 0.f, 1.f), sampleTime);
					lights[LIGHT_SH + 2].setBrightnessSmooth(0.0f, sampleAndHoldVoltageSum);
				}
				else {
					float redValue = math::rescale(-sampleAndHoldVoltageSum, 0.f, 10.f, 0.f, 1.f);
					float greenValue = math::rescale(sampleAndHoldVoltageSum, 0.f, 10.f, 0.f, 1.f);
					lights[LIGHT_SH + 0].setBrightnessSmooth(redValue, sampleTime);
					lights[LIGHT_SH + 1].setBrightnessSmooth(greenValue, sampleTime);
					lights[LIGHT_SH + 2].setBrightnessSmooth(sampleAndHoldVoltageSum < 0 ? redValue : greenValue, sampleTime);
				}
			}

			lights[LIGHT_HARDWARE].setBrightnessSmooth(attenuate3to1, sampleTime);
		} // Light Divider
	}
};


struct ExploratorWidget : ModuleWidget {
	ExploratorWidget(Explorator* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/explorator_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// 1:3
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(5.49, 23.491)), module, Explorator::INPUT_1_TO_3));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(18.897, 23.491)), module, Explorator::OUTPUT_1_TO_3_A));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(12.194, 31.498)), module, Explorator::LIGHT_1_TO_3));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(5.49, 39.504)), module, Explorator::OUTPUT_1_TO_3_B));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(18.897, 39.504)), module, Explorator::OUTPUT_1_TO_3_C));

		// Sign
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(31.871, 23.491)), module, Explorator::INPUT_SIGN));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(45.277, 23.491)), module, Explorator::OUTPUT_SIGN_INVERTED));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(38.574, 31.498)), module, Explorator::LIGHT_SIGN));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(31.871, 39.504)), module, Explorator::OUTPUT_SIGN_HALF_RECTIFIER));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(45.277, 39.504)), module, Explorator::OUTPUT_SIGN_FULL_RECTIFIER));

		// 2:2
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(5.49, 57.521)), module, Explorator::INPUT_2_TO_2_A));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(18.897, 57.521)), module, Explorator::INPUT_2_TO_2_B));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(12.194, 65.527)), module, Explorator::LIGHT_2_TO_2));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(5.49, 73.534)), module, Explorator::OUTPUT_2_TO_2_A));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(18.897, 73.534)), module, Explorator::OUTPUT_2_TO_2_B));

		// Logic
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(31.871, 57.521)), module, Explorator::INPUT_LOGIC_A));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(45.277, 57.521)), module, Explorator::INPUT_LOGIC_B));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(38.212, 65.527)), module, Explorator::LIGHT_LOGIC));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(31.871, 73.534)), module, Explorator::OUTPUT_LOGIC_MIN));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(45.277, 73.534)), module, Explorator::OUTPUT_LOGIC_MAX));

		// 3:1
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(5.49, 91.55)), module, Explorator::INPUT_3_TO_1_A));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(18.897, 91.55)), module, Explorator::INPUT_3_TO_1_B));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(12.194, 99.557)), module, Explorator::LIGHT_3_TO_1));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(5.49, 107.563)), module, Explorator::INPUT_3_TO_1_C));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(18.897, 107.563)), module, Explorator::OUTPUT_3_TO_1));

		// Sample and hold
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(31.871, 91.55)), module, Explorator::INPUT_SH_VOLTAGE));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(45.277, 91.55)), module, Explorator::INPUT_SH_TRIGGER));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(38.212, 99.557)), module, Explorator::LIGHT_SH));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(31.871, 107.563)), module, Explorator::OUTPUT_SH_NOISE));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(45.277, 107.563)), module, Explorator::OUTPUT_SH_VOLTAGE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(15.273, 119.127)), module,
			Explorator::PARAM_HARDWARE, Explorator::LIGHT_HARDWARE));
	}
};

Model* modelExplorator = createModel<Explorator, ExploratorWidget>("Sanguine-Explorator");