#include "plugin.hpp"
#include "mutuus/dsp/mutuus_modulator.h"
#include "sanguinecomponents.hpp"
#include "warpiespals.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Mutuus : Module {
	enum ParamIds {
		PARAM_ALGORITHM,
		PARAM_TIMBRE,
		PARAM_CARRIER,
		PARAM_LEVEL1,
		PARAM_LEVEL2,
		PARAM_MODE_SWITCH,
		PARAM_STEREO,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_LEVEL1,
		INPUT_LEVEL2,
		INPUT_ALGORITHM,
		INPUT_TIMBRE,
		INPUT_CARRIER,
		INPUT_MODULATOR,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_MODULATOR,
		OUTPUT_AUX,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHT_CARRIER, 2),
		ENUMS(LIGHT_ALGORITHM, 3),
		LIGHT_MODE_SWITCH,
		LIGHT_STEREO,
		LIGHTS_COUNT
	};

	int featureMode;
	int frame = 0;

	const int kLightFrequency = 128;

	dsp::BooleanTrigger btModeSwitch;
	dsp::ClockDivider lightDivider;
	mutuus::MutuusModulator mutuusModulator;
	mutuus::ShortFrame inputFrames[60] = {};
	mutuus::ShortFrame outputFrames[60] = {};

	bool bModeSwitchEnabled = false;
	bool bLastInModeSwitch = false;

	float lastAlgorithmValue = 0.f;

	mutuus::Parameters* mutuusParameters = mutuusModulator.mutable_parameters();

	uint16_t reverbBuffer[32768];

	Mutuus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configButton(PARAM_STEREO, "Dual filter stereo");

		configButton(PARAM_MODE_SWITCH, "Mode select");

		configParam(PARAM_ALGORITHM, 0.0, 8.0, 0.0, "Algorithm");

		configParam(PARAM_CARRIER, 0.f, 3.f, 0.f, "Internal oscillator mode");

		configParam(PARAM_TIMBRE, 0.0, 1.0, 0.5, "Timbre", "%", 0, 100);

		configParam(PARAM_LEVEL1, 0.0, 1.0, 1.0, "External oscillator amplitude / internal oscillator frequency", "%", 0, 100);
		configParam(PARAM_LEVEL2, 0.0, 1.0, 1.0, "Modulator amplitude", "%", 0, 100);

		configInput(INPUT_LEVEL1, "Level 1");
		configInput(INPUT_LEVEL2, "Level 2");
		configInput(INPUT_ALGORITHM, "Algorithm");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_CARRIER, "Carrier");
		configInput(INPUT_MODULATOR, "Modulator");

		configOutput(OUTPUT_MODULATOR, "Modulator");
		configOutput(OUTPUT_AUX, "Auxiliary");

		configBypass(INPUT_MODULATOR, OUTPUT_MODULATOR);

		memset(&mutuusModulator, 0, sizeof(mutuusModulator));
		mutuusModulator.Init(96000.0f, reverbBuffer);

		featureMode = mutuus::FEATURE_MODE_META;
		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		mutuusParameters->carrier_shape = params[PARAM_CARRIER].getValue();

		if (btModeSwitch.process(params[PARAM_MODE_SWITCH].getValue())) {
			bModeSwitchEnabled = !bModeSwitchEnabled;

			if (bModeSwitchEnabled) {
				paramQuantities[PARAM_ALGORITHM]->snapEnabled = true;
			}
			else {
				paramQuantities[PARAM_ALGORITHM]->snapEnabled = false;
			}

			if (bLastInModeSwitch) {
				params[PARAM_ALGORITHM].setValue(lastAlgorithmValue);
			}
			else {
				params[PARAM_ALGORITHM].setValue(featureMode);
			}

			bLastInModeSwitch = bModeSwitchEnabled;
		}

		bool isLightsTurn = lightDivider.process();

		if (bModeSwitchEnabled) {
			featureMode = static_cast<mutuus::FeatureMode>(params[PARAM_ALGORITHM].getValue());
			mutuusModulator.set_feature_mode(mutuus::FeatureMode(featureMode));

			if (isLightsTurn) {
				float sampleTime = kLightFrequency * args.sampleTime;
				int8_t ramp = getSystemTimeMs() >> 127;
				uint8_t tri = (getSystemTimeMs() & 255) < 128 ? 127 + ramp : 255 - ramp;

				for (int i = 0; i < 3; i++) {
					lights[LIGHT_ALGORITHM + i].setBrightnessSmooth(((paletteWarpsParasiteFeatureMode[featureMode][i] * tri) >> 8) / 255.f, sampleTime);
				}
			}
		}

		if (isLightsTurn) {
			lights[LIGHT_CARRIER + 0].value = (mutuusParameters->carrier_shape == 1 || mutuusParameters->carrier_shape == 2) ? 1.0 : 0.0;
			lights[LIGHT_CARRIER + 1].value = (mutuusParameters->carrier_shape == 2 || mutuusParameters->carrier_shape == 3) ? 1.0 : 0.0;

			lights[LIGHT_MODE_SWITCH].setBrightness(bModeSwitchEnabled ? 1.f : 0.f);

			mutuusModulator.set_alt_feature_mode(bool(params[PARAM_STEREO].getValue()));
			lights[LIGHT_STEREO].setBrightness(mutuusModulator.alt_feature_mode() ? 1.f : 0.f);
		}

		float_4 f4Voltages;

		// Buffer loop
		if (++frame >= 60) {
			frame = 0;

			// LEVEL1 and LEVEL2 normalized values from cv_scaler.cc and a PR by Brian Head to AI's repository.
			f4Voltages[0] = inputs[INPUT_LEVEL1].getNormalVoltage(5.f);
			f4Voltages[1] = inputs[INPUT_LEVEL2].getNormalVoltage(5.f);
			f4Voltages[2] = inputs[INPUT_ALGORITHM].getVoltage();
			f4Voltages[3] = inputs[INPUT_TIMBRE].getVoltage();

			f4Voltages /= 5.f;

			mutuusParameters->channel_drive[0] = clamp(params[PARAM_LEVEL1].getValue() * f4Voltages[0], 0.f, 1.f);
			mutuusParameters->channel_drive[1] = clamp(params[PARAM_LEVEL2].getValue() * f4Voltages[1], 0.f, 1.f);

			mutuusParameters->raw_level_cv[0] = clamp(f4Voltages[0], 0.f, 1.f);
			mutuusParameters->raw_level_cv[1] = clamp(f4Voltages[1], 0.f, 1.f);

			mutuusParameters->raw_level[0] = mutuusParameters->channel_drive[0];
			mutuusParameters->raw_level[1] = mutuusParameters->channel_drive[1];

			mutuusParameters->raw_level_pot[0] = clamp(params[PARAM_LEVEL1].getValue(), 0.0f, 1.0f);
			mutuusParameters->raw_level_pot[1] = clamp(params[PARAM_LEVEL2].getValue(), 0.0f, 1.0f);

			if (!bModeSwitchEnabled) {
				lastAlgorithmValue = params[PARAM_ALGORITHM].getValue();

				float algorithmValue = lastAlgorithmValue / 8.0;

				switch (featureMode)
				{
				case mutuus::FEATURE_MODE_DELAY: {
					mutuusParameters->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.0f, 1.0f);
					mutuusParameters->raw_algorithm = mutuusParameters->modulation_algorithm;
					mutuusParameters->raw_algorithm_cv = clamp(f4Voltages[2], -1.0f, 1.0f);
					break;
				}
				default: {
					float val = clamp(lastAlgorithmValue, 0.0f, 1.0f);
					val = stmlib::Interpolate(mutuus::lut_pot_curve, val, 512.0f);
					mutuusParameters->raw_algorithm_pot = val;

					mutuusParameters->raw_algorithm_cv = clamp(f4Voltages[2], -1.0f, 1.0f);

					mutuusParameters->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.0f, 1.0f);
					mutuusParameters->raw_algorithm = mutuusParameters->modulation_algorithm;
					break;
				}
				}

				if (isLightsTurn) {
					const uint8_t(*palette)[3];
					float zone;
					if (featureMode != mutuus::FEATURE_MODE_META) {
						palette = paletteWarpsFreqsShift;
					}
					else {
						palette = paletteWarpsDefault;
					}

					zone = 8.0f * mutuusParameters->modulation_algorithm;
					MAKE_INTEGRAL_FRACTIONAL(zone);
					int zone_fractional_i = static_cast<int>(zone_fractional * 256);
					for (int i = 0; i < 3; i++) {
						int a = palette[zone_integral][i];
						int b = palette[zone_integral + 1][i];
						lights[LIGHT_ALGORITHM + i].setBrightness(static_cast<float>(a + ((b - a) * zone_fractional_i >> 8)) / 255.f);
					}
				}
			}

			mutuusParameters->modulation_parameter = clamp(params[PARAM_TIMBRE].getValue() + f4Voltages[3], 0.0f, 1.0f);
			mutuusParameters->raw_modulation = mutuusParameters->modulation_parameter;
			mutuusParameters->raw_modulation_pot = clamp(params[PARAM_TIMBRE].getValue(), 0.f, 1.f);
			mutuusParameters->raw_modulation_cv = clamp(f4Voltages[3], -1.f, 1.f);

			mutuusParameters->note = 60.0 * params[PARAM_LEVEL1].getValue() + 12.0 * inputs[INPUT_LEVEL1].getNormalVoltage(2.0) + 12.0;
			mutuusParameters->note += log2f(96000.0f * args.sampleTime) * 12.0f;

			mutuusModulator.Process(inputFrames, outputFrames, 60);
		}

		inputFrames[frame].l = clamp(int(inputs[INPUT_CARRIER].getVoltage() / 16.0 * 0x8000), -0x8000, 0x7fff);
		inputFrames[frame].r = clamp(int(inputs[INPUT_MODULATOR].getVoltage() / 16.0 * 0x8000), -0x8000, 0x7fff);
		outputs[OUTPUT_MODULATOR].setVoltage(float(outputFrames[frame].l) / 0x8000 * 5.0);
		outputs[OUTPUT_AUX].setVoltage(float(outputFrames[frame].r) / 0x8000 * 5.0);
	}

	long long getSystemTimeMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "mode", json_integer(mutuusModulator.feature_mode()));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		if (json_t* modeJ = json_object_get(rootJ, "mode")) {
			mutuusModulator.set_feature_mode(static_cast<mutuus::FeatureMode>(json_integer_value(modeJ)));
			featureMode = mutuusModulator.feature_mode();
		}
	}

	void setFeatureMode(int modeNumber) {
		featureMode = modeNumber;
		mutuusModulator.set_feature_mode(mutuus::FeatureMode(featureMode));
	}
};

static const std::string mutuusModelLabels[9] = {
	"Dual state variable filter",
	"Ladder filter",
	"Reverbs",
	"Frequency shifter",
	"Bit crusher",
	"Chebyschev wave shaper",
	"Doppler panner",
	"Delay",
	"Meta mode",
};

struct MutuusWidget : ModuleWidget {
	MutuusWidget(Mutuus* module) {
		setModule(module);
		SanguinePanel* panel = new SanguinePanel(pluginInstance, "res/backplate_10hp_green.svg", "res/mutuus_faceplate.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(millimetersToPixelsVec(4.836, 16.119),
			module, Mutuus::PARAM_STEREO, Mutuus::LIGHT_STEREO));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(45.963, 16.119),
			module, Mutuus::PARAM_MODE_SWITCH, Mutuus::LIGHT_MODE_SWITCH));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(25.4, 37.486), module, Mutuus::PARAM_ALGORITHM));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(25.4, 37.486), module, Mutuus::LIGHT_ALGORITHM));
		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(42.388, 79.669), module, Mutuus::PARAM_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(16.906, 63.862),
			module, Mutuus::PARAM_CARRIER, Mutuus::LIGHT_CARRIER));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.894, 63.862), module, Mutuus::INPUT_ALGORITHM));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(8.412, 79.451), module, Mutuus::PARAM_LEVEL1));
		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(25.4, 79.451), module, Mutuus::PARAM_LEVEL2));

		addInput(createInputCentered<BananutYellow>(millimetersToPixelsVec(8.412, 96.146), module, Mutuus::INPUT_LEVEL1));
		addInput(createInputCentered<BananutBlue>(millimetersToPixelsVec(25.4, 96.146), module, Mutuus::INPUT_LEVEL2));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(42.388, 96.146), module, Mutuus::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.925, 112.172), module, Mutuus::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(18.777, 112.172), module, Mutuus::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(32.044, 112.172), module, Mutuus::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(42.896, 112.172), module, Mutuus::OUTPUT_AUX));
	}

	void appendContextMenu(Menu* menu) override {
		Mutuus* module = dynamic_cast<Mutuus*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Mode", "", [=](Menu* menu) {
			for (int i = 0; i < 9; i++) {
				menu->addChild(createCheckMenuItem(mutuusModelLabels[i], "",
					[=]() {return module->featureMode == i; },
					[=]() {module->setFeatureMode(i); }
				));
			}
			}));
	}
};

Model* modelMutuus = createModel<Mutuus, MutuusWidget>("Sanguine-Mutuus");