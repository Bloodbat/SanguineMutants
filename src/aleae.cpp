#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "aleae.hpp"

struct Aleae : SanguineModule {
	enum ParamIds {
		PARAM_THRESHOLD_1,
		PARAM_THRESHOLD_2,
		PARAM_ROLL_MODE_1,
		PARAM_ROLL_MODE_2,
		PARAM_OUT_MODE_1,
		PARAM_OUT_MODE_2,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_IN_1,
		INPUT_IN_2,
		INPUT_P_1,
		INPUT_P_2,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_OUT_1A,
		OUTPUT_OUT_2A,
		OUTPUT_OUT_1B,
		OUTPUT_OUT_2B,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHTS_STATE, 2 * 2),
		ENUMS(LIGHTS_ROLL_MODE, 2 * 2),
		ENUMS(LIGHTS_OUT_MODE, 2),
		LIGHTS_COUNT
	};

	static const int kLightFrequency = 16;
	static const int kMaxModuleSections = 2;
	int ledsChannel = 0;
	int channelCount = 0;


	dsp::BooleanTrigger btGateTriggers[kMaxModuleSections][PORT_MAX_CHANNELS];
	dsp::ClockDivider lightsDivider;

	RollResults rollResults[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	RollResults lastRollResults[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	RollModes rollModes[kMaxModuleSections] = { ROLL_DIRECT, ROLL_DIRECT };
	OutModes outModes[kMaxModuleSections] = { OUT_MODE_TRIGGER, OUT_MODE_TRIGGER };
	bool bOutputsConnected[OUTPUTS_COUNT] = {};

	Aleae() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		for (int section = 0; section < kMaxModuleSections; ++section) {
			configParam(PARAM_THRESHOLD_1 + section, 0.f, 1.f, 0.5f, string::f("Channel %d probability", section + 1), "%", 0, 100);
			configSwitch(PARAM_ROLL_MODE_1 + section, 0.f, 1.f, 0.f, string::f("Channel %d coin mode", section + 1), { "Direct", "Toggle" });
			configSwitch(PARAM_OUT_MODE_1 + section, 0.f, 1.f, 0.f, string::f("Channel %d out mode", section + 1), { "Trigger", "Latch" });
			configInput(INPUT_IN_1 + section, string::f("Channel %d", section + 1));
			configInput(INPUT_P_1 + section, string::f("Channel %d probability", section + 1));
			configOutput(OUTPUT_OUT_1A + section, string::f("Channel %d A", section + 1));
			configOutput(OUTPUT_OUT_1B + section, string::f("Channel %d B", section + 1));
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				lastRollResults[section][channel] = ROLL_HEADS;
			}
			lightsDivider.setDivision(kLightFrequency);
		}
	}

	void process(const ProcessArgs& args) override {
		bOutputsConnected[0] = outputs[OUTPUT_OUT_1A].isConnected();
		bOutputsConnected[1] = outputs[OUTPUT_OUT_2A].isConnected();
		bOutputsConnected[2] = outputs[OUTPUT_OUT_1B].isConnected();
		bOutputsConnected[3] = outputs[OUTPUT_OUT_2B].isConnected();

		bool bIsLightsTurn = lightsDivider.process();

		for (int section = 0; section < kMaxModuleSections; ++section) {
			// Get input.
			Input* input = &inputs[INPUT_IN_1 + section];
			// 2nd input is normalized to 1st.
			if (section == 1 && !input->isConnected()) {
				input = &inputs[INPUT_IN_1 + 0];
			}
			channelCount = std::max(input->getChannels(), 1);

			rollModes[section] = static_cast<RollModes>(params[PARAM_ROLL_MODE_1 + section].getValue());
			outModes[section] = static_cast<OutModes>(params[PARAM_OUT_MODE_1 + section].getValue());

			bool lightAActive = false;
			bool lightBActive = false;

			// Process triggers.
			for (int channel = 0; channel < channelCount; ++channel) {
				bool gatePresent = input->getVoltage(channel) >= 2.f;
				if (btGateTriggers[section][channel].process(gatePresent)) {
					// Trigger.
					// Don't have to clamp here because the threshold comparison works without it.
					float threshold = params[PARAM_THRESHOLD_1 + section].getValue() + inputs[INPUT_P_1 + section].getPolyVoltage(channel) / 10.f;
					rollResults[section][channel] = (random::uniform() >= threshold) ? ROLL_HEADS : ROLL_TAILS;
					if (rollModes[section] == ROLL_TOGGLE) {
						rollResults[section][channel] = static_cast<RollResults>(lastRollResults[section][channel] ^ rollResults[section][channel]);
					}
					lastRollResults[section][channel] = rollResults[section][channel];
				}

				// Output gate logic
				bool gateAActive = (rollResults[section][channel] == ROLL_HEADS && (outModes[section] == OUT_MODE_LATCH || gatePresent));
				bool gateBActive = (rollResults[section][channel] == ROLL_TAILS && (outModes[section] == OUT_MODE_LATCH || gatePresent));

				if (channel == ledsChannel) {
					lightAActive = gateAActive;
					lightBActive = gateBActive;
				}

				// Set output gates
				if (bOutputsConnected[0 + section]) {
					outputs[OUTPUT_OUT_1A + section].setVoltage(gateAActive ? 10.f : 0.f, channel);
				}
				if (bOutputsConnected[2 + section]) {
					outputs[OUTPUT_OUT_1B + section].setVoltage(gateBActive ? 10.f : 0.f, channel);
				}
			}

			if (bOutputsConnected[0 + section]) {
				outputs[OUTPUT_OUT_1A + section].setChannels(channelCount);
			}
			if (bOutputsConnected[2 + section]) {
				outputs[OUTPUT_OUT_1B + section].setChannels(channelCount);
			}

			if (bIsLightsTurn) {
				if (ledsChannel >= channelCount) {
					ledsChannel = channelCount - 1;
				}

				const float sampleTime = args.sampleTime * kLightFrequency;
				int currentLight = LIGHTS_STATE + section * 2;
				lights[currentLight + 1].setSmoothBrightness(lightAActive, sampleTime);
				lights[currentLight + 0].setSmoothBrightness(lightBActive, sampleTime);

				currentLight = LIGHTS_ROLL_MODE + section * 2;
				lights[currentLight + 0].setBrightnessSmooth(rollModes[section] == ROLL_DIRECT ? 0.75f : 0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(rollModes[section] == ROLL_DIRECT ? 0.f : 0.75f, sampleTime);

				lights[LIGHTS_OUT_MODE + section].setBrightnessSmooth(outModes[section] ? 0.75 : 0.f, sampleTime);
			}
		}
	}


	void onReset() override {
		for (int section = 0; section < kMaxModuleSections; ++section) {
			params[PARAM_ROLL_MODE_1 + section].setValue(0);
			params[PARAM_OUT_MODE_1 + section].setValue(0);
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				lastRollResults[section][channel] = ROLL_HEADS;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* ledsChannelJ = json_integer(ledsChannel);
		json_object_set_new(rootJ, "ledsChannel", ledsChannelJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* ledsChannelJ = json_object_get(rootJ, "ledsChannel");
		if (ledsChannelJ) {
			ledsChannel = json_integer_value(ledsChannelJ);
		}
	}
};

struct AleaeWidget : SanguineModuleWidget {
	AleaeWidget(Aleae* module) {
		setModule(module);

		moduleName = "aleae";
		panelSize = SIZE_6;
		backplateColor = PLATE_PURPLE;

		makePanel();


		addScrews(SCREW_TOP_LEFT | SCREW_BOTTOM_LEFT);

		// Switch #1
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(4.622, 16.723), module,
			Aleae::PARAM_ROLL_MODE_1, Aleae::LIGHTS_ROLL_MODE + 0 * 2));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(25.863, 16.723), module,
			Aleae::PARAM_OUT_MODE_1, Aleae::LIGHTS_OUT_MODE + 0));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(15.24, 29.079), module, Aleae::PARAM_THRESHOLD_1));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.012, 44.303), module, Aleae::INPUT_IN_1));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(24.481, 44.303), module, Aleae::INPUT_P_1));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(6.012, 59.959), module, Aleae::OUTPUT_OUT_1A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 59.959), module, Aleae::LIGHTS_STATE + 0 * 2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(24.481, 59.959), module, Aleae::OUTPUT_OUT_1B));

		// Switch #2
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(4.622, 74.653), module,
			Aleae::PARAM_ROLL_MODE_2, Aleae::LIGHTS_ROLL_MODE + 1 * 2));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(25.863, 74.653), module,
			Aleae::PARAM_OUT_MODE_2, Aleae::LIGHTS_OUT_MODE + 1));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(15.24, 87.008), module, Aleae::PARAM_THRESHOLD_2));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.012, 102.232), module, Aleae::INPUT_IN_2));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(24.481, 102.232), module, Aleae::INPUT_P_2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(6.012, 117.888), module, Aleae::OUTPUT_OUT_2A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 117.888), module, Aleae::LIGHTS_STATE + 1 * 2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(24.481, 117.888), module, Aleae::OUTPUT_OUT_2B));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Aleae* module = dynamic_cast<Aleae*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> availableChannels;
		for (int i = 0; i < module->channelCount; ++i) {
			availableChannels.push_back(channelNumbers[i]);
		}
		menu->addChild(createIndexSubmenuItem("LEDs channel", availableChannels,
			[=]() {return module->ledsChannel; },
			[=](int i) {module->ledsChannel = i; }
		));
	}
};

Model* modelAleae = createModel<Aleae, AleaeWidget>("Sanguine-Aleae");