#include "plugin.hpp"
#include "sanguinecomponents.hpp"

#define ROLL_DIRECT 0
#define ROLL_TOGGLE 1

#define OUT_MODE_TRIGGER 0
#define OUT_MODE_LATCH 1

#define	ROLL_HEADS 0
#define ROLL_TAILS 1

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

	dsp::BooleanTrigger btGateTriggers[2][16];

	bool rollResults[2][16] = {};
	bool lastRollResults[2][16];

	bool rollModes[2] = { ROLL_DIRECT, ROLL_DIRECT };
	bool outModes[2] = { OUT_MODE_TRIGGER, OUT_MODE_TRIGGER };
	bool bOutputsConnected[OUTPUTS_COUNT];

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
		}
	}

	void process(const ProcessArgs& args) override {
		bOutputsConnected[0] = outputs[OUTPUT_OUT1A].isConnected();
		bOutputsConnected[1] = outputs[OUTPUT_OUT2A].isConnected();
		bOutputsConnected[2] = outputs[OUTPUT_OUT1B].isConnected();
		bOutputsConnected[3] = outputs[OUTPUT_OUT2B].isConnected();

		for (int i = 0; i < 2; i++) {
			// Get input.
			Input* input = &inputs[INPUT_IN1 + i];
			// 2nd input is normalized to 1st.
			if (i == 1 && !input->isConnected())
				input = &inputs[INPUT_IN1 + 0];
			int channelCount = std::max(input->getChannels(), 1);

			rollModes[i] = params[PARAM_ROLL_MODE1 + i].getValue();
			outModes[i] = params[PARAM_OUT_MODE1 + i].getValue();

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
						rollResults[i][channel] = lastRollResults[i][channel] ^ rollResults[i][channel];
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

			int currentLight = LIGHTS_STATE + i * 2;
			lights[currentLight + 1].setSmoothBrightness(lightAActive, args.sampleTime);
			lights[currentLight + 0].setSmoothBrightness(lightBActive, args.sampleTime);

			currentLight = LIGHTS_ROLL_MODE + i * 2;
			lights[currentLight + 0].setBrightnessSmooth(rollModes[i] == ROLL_DIRECT ? 1.f : 0.f, args.sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(rollModes[i] == ROLL_DIRECT ? 0.f : 1.f, args.sampleTime);

			lights[LIGHTS_OUT_MODE + i].setBrightnessSmooth(outModes[i], args.sampleTime);
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
};

struct AleaeWidget : ModuleWidget {
	AleaeWidget(Aleae* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/aleae_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		// Switch #1
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(mm2px(Vec(4.622, 16.723)), module,
			Aleae::PARAM_ROLL_MODE1, Aleae::LIGHTS_ROLL_MODE + 0 * 2));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(mm2px(Vec(25.863, 16.723)), module,
			Aleae::PARAM_OUT_MODE1, Aleae::LIGHTS_OUT_MODE + 0));

		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(15.24, 29.079)), module, Aleae::PARAM_THRESHOLD1));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(6.012, 44.303)), module, Aleae::INPUT_IN1));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(24.481, 44.303)), module, Aleae::INPUT_P1));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(6.012, 59.959)), module, Aleae::OUTPUT_OUT1A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(15.24, 59.959)), module, Aleae::LIGHTS_STATE + 0 * 2));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(24.481, 59.959)), module, Aleae::OUTPUT_OUT1B));

		// Switch #2
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(mm2px(Vec(4.622, 74.653)), module,
			Aleae::PARAM_ROLL_MODE2, Aleae::LIGHTS_ROLL_MODE + 1 * 2));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(mm2px(Vec(25.863, 74.653)), module,
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