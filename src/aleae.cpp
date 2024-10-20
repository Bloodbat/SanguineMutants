#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "aleae.hpp"

struct Aleae : SanguineModule {
	enum ParamIds {
		PARAM_THRESHOLD1,
		PARAM_THRESHOLD2,
		PARAM_ROLL_MODE1,
		PARAM_ROLL_MODE2,
		PARAM_OUT_MODE1,
		PARAM_OUT_MODE2,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_IN1,
		INPUT_IN2,
		INPUT_P1,
		INPUT_P2,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_OUT1A,
		OUTPUT_OUT2A,
		OUTPUT_OUT1B,
		OUTPUT_OUT2B,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHTS_STATE, 2 * 2),
		ENUMS(LIGHTS_ROLL_MODE, 2 * 2),
		ENUMS(LIGHTS_OUT_MODE, 2),
		LIGHTS_COUNT
	};

	dsp::BooleanTrigger btGateTriggers[2][16];
	dsp::ClockDivider lightsDivider;

	RollResults rollResults[2][16] = {};
	RollResults lastRollResults[2][16] = {};

	RollModes rollModes[2] = { ROLL_DIRECT, ROLL_DIRECT };
	OutModes outModes[2] = { OUT_MODE_TRIGGER, OUT_MODE_TRIGGER };
	bool bOutputsConnected[OUTPUTS_COUNT];

	const int kLightFrequency = 16;
	int ledsChannel = 0;
	int channelCount = 0;

	Aleae() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		for (int i = 0; i < 2; i++) {
			configParam(PARAM_THRESHOLD1 + i, 0.f, 1.f, 0.5f, string::f("Channel %d probability", i + 1), "%", 0, 100);
			configSwitch(PARAM_ROLL_MODE1 + i, 0.f, 1.f, 0.f, string::f("Channel %d coin mode", i + 1), { "Direct", "Toggle" });
			configSwitch(PARAM_OUT_MODE1 + i, 0.f, 1.f, 0.f, string::f("Channel %d out mode", i + 1), { "Trigger", "Latch" });
			configInput(INPUT_IN1 + i, string::f("Channel %d", i + 1));
			configInput(INPUT_P1 + i, string::f("Channel %d probability", i + 1));
			configOutput(OUTPUT_OUT1A + i, string::f("Channel %d A", i + 1));
			configOutput(OUTPUT_OUT1B + i, string::f("Channel %d B", i + 1));
			for (int j = 0; j < 16; j++) {
				lastRollResults[i][j] = ROLL_HEADS;
			}
			lightsDivider.setDivision(kLightFrequency);
		}
	}

	void process(const ProcessArgs& args) override {
		bOutputsConnected[0] = outputs[OUTPUT_OUT1A].isConnected();
		bOutputsConnected[1] = outputs[OUTPUT_OUT2A].isConnected();
		bOutputsConnected[2] = outputs[OUTPUT_OUT1B].isConnected();
		bOutputsConnected[3] = outputs[OUTPUT_OUT2B].isConnected();

		bool bIsLightsTurn = lightsDivider.process();

		for (int i = 0; i < 2; i++) {
			// Get input.
			Input* input = &inputs[INPUT_IN1 + i];
			// 2nd input is normalized to 1st.
			if (i == 1 && !input->isConnected()) {
				input = &inputs[INPUT_IN1 + 0];
			}
			channelCount = std::max(input->getChannels(), 1);

			rollModes[i] = static_cast<RollModes>(params[PARAM_ROLL_MODE1 + i].getValue());
			outModes[i] = static_cast<OutModes>(params[PARAM_OUT_MODE1 + i].getValue());

			bool lightAActive = false;
			bool lightBActive = false;

			// Process triggers.
			for (int channel = 0; channel < channelCount; channel++) {
				bool gatePresent = input->getVoltage(channel) >= 2.f;
				if (btGateTriggers[i][channel].process(gatePresent)) {
					// Trigger.
					// Don't have to clamp here because the threshold comparison works without it.
					float threshold = params[PARAM_THRESHOLD1 + i].getValue() + inputs[INPUT_P1 + i].getPolyVoltage(channel) / 10.f;
					rollResults[i][channel] = (random::uniform() >= threshold) ? ROLL_HEADS : ROLL_TAILS;
					if (rollModes[i] == ROLL_TOGGLE) {
						rollResults[i][channel] = static_cast<RollResults>(lastRollResults[i][channel] ^ rollResults[i][channel]);
					}
					lastRollResults[i][channel] = rollResults[i][channel];
				}

				// Output gate logic
				bool gateAActive = (rollResults[i][channel] == ROLL_HEADS && (outModes[i] == OUT_MODE_LATCH || gatePresent));
				bool gateBActive = (rollResults[i][channel] == ROLL_TAILS && (outModes[i] == OUT_MODE_LATCH || gatePresent));

				if (channel == ledsChannel) {
					lightAActive = gateAActive;
					lightBActive = gateBActive;
				}

				// Set output gates
				if (bOutputsConnected[0 + i]) {
					outputs[OUTPUT_OUT1A + i].setVoltage(gateAActive ? 10.f : 0.f, channel);
				}
				if (bOutputsConnected[2 + i]) {
					outputs[OUTPUT_OUT1B + i].setVoltage(gateBActive ? 10.f : 0.f, channel);
				}
			}

			if (bOutputsConnected[0 + i]) {
				outputs[OUTPUT_OUT1A + i].setChannels(channelCount);
			}
			if (bOutputsConnected[2 + i]) {
				outputs[OUTPUT_OUT1B + i].setChannels(channelCount);
			}

			if (bIsLightsTurn) {
				if (ledsChannel >= channelCount) {
					ledsChannel = channelCount - 1;
				}

				const float sampleTime = args.sampleTime * kLightFrequency;
				int currentLight = LIGHTS_STATE + i * 2;
				lights[currentLight + 1].setSmoothBrightness(lightAActive, sampleTime);
				lights[currentLight + 0].setSmoothBrightness(lightBActive, sampleTime);

				currentLight = LIGHTS_ROLL_MODE + i * 2;
				lights[currentLight + 0].setBrightnessSmooth(rollModes[i] == ROLL_DIRECT ? 0.75f : 0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(rollModes[i] == ROLL_DIRECT ? 0.f : 0.75f, sampleTime);

				lights[LIGHTS_OUT_MODE + i].setBrightnessSmooth(outModes[i] ? 0.75 : 0.f, sampleTime);
			}
		}
	}


	void onReset() override {
		for (int i = 0; i < 2; i++) {
			params[PARAM_ROLL_MODE1 + i].setValue(0);
			params[PARAM_OUT_MODE1 + i].setValue(0);
			for (int j = 0; j < 16; j++) {
				lastRollResults[i][j] = ROLL_HEADS;
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
			Aleae::PARAM_ROLL_MODE1, Aleae::LIGHTS_ROLL_MODE + 0 * 2));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(25.863, 16.723), module,
			Aleae::PARAM_OUT_MODE1, Aleae::LIGHTS_OUT_MODE + 0));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(15.24, 29.079), module, Aleae::PARAM_THRESHOLD1));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.012, 44.303), module, Aleae::INPUT_IN1));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(24.481, 44.303), module, Aleae::INPUT_P1));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(6.012, 59.959), module, Aleae::OUTPUT_OUT1A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 59.959), module, Aleae::LIGHTS_STATE + 0 * 2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(24.481, 59.959), module, Aleae::OUTPUT_OUT1B));

		// Switch #2
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(4.622, 74.653), module,
			Aleae::PARAM_ROLL_MODE2, Aleae::LIGHTS_ROLL_MODE + 1 * 2));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(25.863, 74.653), module,
			Aleae::PARAM_OUT_MODE2, Aleae::LIGHTS_OUT_MODE + 1));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(15.24, 87.008), module, Aleae::PARAM_THRESHOLD2));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.012, 102.232), module, Aleae::INPUT_IN2));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(24.481, 102.232), module, Aleae::INPUT_P2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(6.012, 117.888), module, Aleae::OUTPUT_OUT2A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 117.888), module, Aleae::LIGHTS_STATE + 1 * 2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(24.481, 117.888), module, Aleae::OUTPUT_OUT2B));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Aleae* module = dynamic_cast<Aleae*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> availableChannels;
		for (int i = 0; i < module->channelCount; i++) {
			availableChannels.push_back(channelNumbers[i]);
		}
		menu->addChild(createIndexSubmenuItem("LEDs channel", availableChannels,
			[=]() {return module->ledsChannel; },
			[=](int i) {module->ledsChannel = i; }
		));
	}
};

Model* modelAleae = createModel<Aleae, AleaeWidget>("Sanguine-Aleae");