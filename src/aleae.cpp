#include "plugin.hpp"
#include "sanguinecomponents.hpp"

struct Aleae : Module {
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

	enum RollModes {
		ROLL_DIRECT,
		ROLL_TOGGLE
	};

	enum OutModes {
		OUT_MODE_TRIGGER,
		OUT_MODE_LATCH
	};

	enum RollResults {
		ROLL_HEADS,
		ROLL_TAILS
	};

	dsp::BooleanTrigger btGateTriggers[2][16];
	dsp::SchmittTrigger stRollModeTriggers[2];
	dsp::SchmittTrigger stOutModeTriggers[2];

	RollResults rollResults[2][16] = {};
	RollResults lastRollResults[2][16];

	RollModes rollModes[2] = { ROLL_DIRECT, ROLL_DIRECT };
	OutModes outModes[2] = { OUT_MODE_TRIGGER, OUT_MODE_TRIGGER };

	struct RollModeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			// Convolution to avoid annoying warning.
			std::string rollString = "";
			if (module != nullptr) {
				Aleae* moduleAleae = static_cast<Aleae*>(module);

				if (paramId == PARAM_ROLL_MODE1 || paramId == PARAM_ROLL_MODE2) {
					RollModes rollMode;
					switch (paramId)
					{
					case PARAM_ROLL_MODE1: {
						rollMode = moduleAleae->rollModes[0];
						break;
					}
					case PARAM_ROLL_MODE2: {
						rollMode = moduleAleae->rollModes[1];
						break;
					}
					default:
						break;
					}

					switch (rollMode) {
					case ROLL_DIRECT: {
						rollString = "Direct";
						break;
					}
					case ROLL_TOGGLE: {
						rollString = "Toggle";
						break;
					}
					default: {
						rollString = "";
						break;
					}
					}
				}
			}
			else {
				rollString = "";
			}
			return rollString;
		}
	};

	struct OutModeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			// Convolution to avoid annoying warning.
			std::string outModeString = "";
			if (module != nullptr) {
				Aleae* moduleAleae = static_cast<Aleae*>(module);

				if (paramId == PARAM_OUT_MODE1 || paramId == PARAM_OUT_MODE2) {
					OutModes outMode;
					switch (paramId) {
					case PARAM_OUT_MODE1: {
						outMode = moduleAleae->outModes[0];
						break;
					}
					case PARAM_OUT_MODE2: {
						outMode = moduleAleae->outModes[1];
						break;
					}
					{
					default: {
						break;
					}
					}
					}

					switch (outMode)
					{
					case OUT_MODE_TRIGGER: {
						outModeString = "Trigger";
						break;
					}
					case OUT_MODE_LATCH: {
						outModeString = "Latch";
						break;
					}
					default: {
						outModeString = "";
						break;
					}
					}
				}
			}
			else {
				outModeString = "";
			}
			return outModeString;
		}
	};

	Aleae() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		for (int i = 0; i < 2; i++) {
			configParam(PARAM_THRESHOLD1 + i, 0.0, 1.0, 0.5, string::f("Channel %d probability", i + 1), "%", 0, 100);
			configButton<RollModeParam>(PARAM_ROLL_MODE1 + i, string::f("Channel %d coin mode", i + 1));
			configButton<OutModeParam>(PARAM_OUT_MODE1 + i, string::f("Channel %d out mode", i + 1));
			configInput(INPUT_IN1 + i, string::f("Channel %d", i + 1));
			configInput(INPUT_P1 + i, string::f("Channel %d probability", i + 1));
			configOutput(OUTPUT_OUT1A + i, string::f("Channel %d A", i + 1));
			configOutput(OUTPUT_OUT1B + i, string::f("Channel %d B", i + 1));
			for (int j = 0; j < 16; j++) {
				lastRollResults[i][j] = ROLL_HEADS;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		for (int i = 0; i < 2; i++) {
			// Get input.
			Input* input = &inputs[INPUT_IN1 + i];
			// 2nd input is normalized to 1st.
			if (i == 1 && !input->isConnected())
				input = &inputs[INPUT_IN1 + 0];
			int channelCount = std::max(input->getChannels(), 1);

			if (stRollModeTriggers[i].process(params[PARAM_ROLL_MODE1 + i].getValue())) {
				rollModes[i] = (RollModes)((rollModes[i] + 1) % 2);
			}

			if (stOutModeTriggers[i].process(params[PARAM_OUT_MODE1 + i].getValue())) {
				outModes[i] = (OutModes)((outModes[i] + 1) % 2);
			}

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
						rollResults[i][channel] = RollResults(lastRollResults[i][channel] ^ rollResults[i][channel]);
					}
					lastRollResults[i][channel] = rollResults[i][channel];
				}

				// Output gate logic								
				bool gateAActive = (rollResults[i][channel] == ROLL_HEADS && (outModes[i] == OUT_MODE_LATCH || gatePresent));
				bool gateBActive = (rollResults[i][channel] == ROLL_TAILS && (outModes[i] == OUT_MODE_LATCH || gatePresent));

				if (gateAActive)
					lightAActive = true;
				if (gateBActive)
					lightBActive = true;

				// Set output gates
				outputs[OUTPUT_OUT1A + i].setVoltage(gateAActive ? 10.f : 0.f, channel);
				outputs[OUTPUT_OUT1B + i].setVoltage(gateBActive ? 10.f : 0.f, channel);
			}

			outputs[OUTPUT_OUT1A + i].setChannels(channelCount);
			outputs[OUTPUT_OUT1B + i].setChannels(channelCount);

			int currentLight = LIGHTS_STATE + i * 2;
			lights[currentLight + 1].setSmoothBrightness(lightAActive, args.sampleTime);
			lights[currentLight + 0].setSmoothBrightness(lightBActive, args.sampleTime);

			currentLight = LIGHTS_ROLL_MODE + i * 2;
			lights[currentLight + 0].setBrightnessSmooth(rollModes[i] == ROLL_DIRECT ? 1.f : 0.f, args.sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(rollModes[i] == ROLL_DIRECT ? 0.f : 1.f, args.sampleTime);

			lights[LIGHTS_OUT_MODE + i].setBrightnessSmooth(outModes[i] == OUT_MODE_LATCH ? 1.f : 0.f, args.sampleTime);
		}
	}


	void onReset() override {
		for (int i = 0; i < 2; i++) {
			outModes[i] = OUT_MODE_TRIGGER;
			rollModes[i] = ROLL_DIRECT;
			for (int j = 0; j < 16; j++) {
				lastRollResults[i][j] = ROLL_HEADS;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* rollModesJ = json_array();
		json_t* outModesJ = json_array();
		for (int i = 0; i < 2; i++) {
			json_array_insert_new(rollModesJ, i, json_integer(rollModes[i]));
			json_array_insert_new(outModesJ, i, json_integer(outModes[i]));
		}
		json_object_set_new(rootJ, "rollModes", rollModesJ);
		json_object_set_new(rootJ, "outModes", outModesJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* rollModesJ = json_object_get(rootJ, "rollModes");
		if (rollModesJ) {
			for (int i = 0; i < 2; i++) {
				json_t* modeJ = json_array_get(rollModesJ, i);
				if (modeJ)
					rollModes[i] = (RollModes)(json_integer_value(modeJ));
			}
		}
		json_t* outModesJ = json_object_get(rootJ, "outModes");
		if (outModesJ) {
			for (int i = 0; i < 2; i++) {
				json_t* modeJ = json_array_get(outModesJ, i);
				if (modeJ)
					outModes[i] = (OutModes)(json_integer_value(modeJ));
			}
		}
	}
};


struct AleaeWidget : ModuleWidget {
	AleaeWidget(Aleae* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/aleae_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Switch #1
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(mm2px(Vec(4.622, 16.723)), module,
			Aleae::PARAM_ROLL_MODE1, Aleae::LIGHTS_ROLL_MODE + 0 * 2));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<OrangeLight>>>(mm2px(Vec(25.863, 16.723)), module,
			Aleae::PARAM_OUT_MODE1, Aleae::LIGHTS_OUT_MODE + 0));

		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(15.24, 29.079)), module, Aleae::PARAM_THRESHOLD1));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.012, 44.303)), module, Aleae::INPUT_IN1));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(24.481, 44.303)), module, Aleae::INPUT_P1));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(6.012, 59.959)), module, Aleae::OUTPUT_OUT1A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(15.24, 59.959)), module, Aleae::LIGHTS_STATE + 0 * 2));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(24.481, 59.959)), module, Aleae::OUTPUT_OUT1B));

		// Switch #2
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(mm2px(Vec(4.622, 74.653)), module,
			Aleae::PARAM_ROLL_MODE2, Aleae::LIGHTS_ROLL_MODE + 1 * 2));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<OrangeLight>>>(mm2px(Vec(25.863, 74.653)), module,
			Aleae::PARAM_OUT_MODE2, Aleae::LIGHTS_OUT_MODE + 1));

		addParam(createParamCentered<Sanguine1PSBlue>(mm2px(Vec(15.24, 87.008)), module, Aleae::PARAM_THRESHOLD2));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.012, 102.232)), module, Aleae::INPUT_IN2));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(24.481, 102.232)), module, Aleae::INPUT_P2));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(6.012, 117.888)), module, Aleae::OUTPUT_OUT2A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(15.24, 117.888)), module, Aleae::LIGHTS_STATE + 1 * 2));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(24.481, 117.888)), module, Aleae::OUTPUT_OUT2B));
	}
};

Model* modelAleae = createModel<Aleae, AleaeWidget>("Sanguine-Aleae");