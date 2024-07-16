#include "plugin.hpp"
#include "distortiones/dsp/distortiones_modulator.h"
#include "sanguinecomponents.hpp"
#include "warpiespals.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Distortiones : Module {
	enum ParamIds {
		PARAM_ALGORITHM,
		PARAM_TIMBRE,
		PARAM_CARRIER,
		PARAM_LEVEL1,
		PARAM_LEVEL2,
		PARAM_MODE_SWITCH,
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
		LIGHTS_COUNT
	};

	int featureMode;
	int frame = 0;

	const int kLightFrequency = 128;

	dsp::BooleanTrigger btModeSwitch;
	dsp::ClockDivider lightDivider;
	distortiones::DistortionesModulator distortionesModulator;
	distortiones::ShortFrame inputFrames[60] = {};
	distortiones::ShortFrame outputFrames[60] = {};

	bool bModeSwitchEnabled = false;
	bool bLastInModeSwitch = false;

	float lastAlgorithmValue = 0.f;

	distortiones::Parameters* distortionesParameters = distortionesModulator.mutable_parameters();

	Distortiones() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

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

		memset(&distortionesModulator, 0, sizeof(distortionesModulator));
		distortionesModulator.Init(96000.0f);

		featureMode = distortiones::FEATURE_MODE_META;
		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		distortionesParameters->carrier_shape = params[PARAM_CARRIER].getValue();

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
			featureMode = static_cast<distortiones::FeatureMode>(params[PARAM_ALGORITHM].getValue());
			distortionesModulator.set_feature_mode(distortiones::FeatureMode(featureMode));

			if (isLightsTurn) {
				const float sampleTime = kLightFrequency * args.sampleTime;
				int8_t ramp = getSystemTimeMs() & 127;
				uint8_t tri = (getSystemTimeMs() & 255) < 128 ? 127 + ramp : 255 - ramp;

				for (int i = 0; i < 3; i++) {
					lights[LIGHT_ALGORITHM + i].setBrightnessSmooth(((paletteWarpsParasiteFeatureMode[featureMode][i] * tri) >> 8) / 255.f, sampleTime);
				}
			}
		}

		if (isLightsTurn) {
			lights[LIGHT_CARRIER + 0].value = (distortionesParameters->carrier_shape == 1 || distortionesParameters->carrier_shape == 2) ? 1.0 : 0.0;
			lights[LIGHT_CARRIER + 1].value = (distortionesParameters->carrier_shape == 2 || distortionesParameters->carrier_shape == 3) ? 1.0 : 0.0;

			lights[LIGHT_MODE_SWITCH].setBrightness(bModeSwitchEnabled ? 1.f : 0.f);
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

			distortionesParameters->channel_drive[0] = clamp(params[PARAM_LEVEL1].getValue() * f4Voltages[0], 0.f, 1.f);
			distortionesParameters->channel_drive[1] = clamp(params[PARAM_LEVEL2].getValue() * f4Voltages[1], 0.f, 1.f);

			distortionesParameters->raw_level[0] = distortionesParameters->channel_drive[0];
			distortionesParameters->raw_level[1] = distortionesParameters->channel_drive[1];

			if (!bModeSwitchEnabled) {
				lastAlgorithmValue = params[PARAM_ALGORITHM].getValue();

				float algorithmValue = lastAlgorithmValue / 8.0;

				float val = clamp(lastAlgorithmValue, 0.0f, 1.0f);
				val = stmlib::Interpolate(distortiones::lut_pot_curve, val, 512.0f);
				distortionesParameters->raw_algorithm_pot = val;

				distortionesParameters->raw_algorithm_cv = clamp(f4Voltages[2], -1.0f, 1.0f);

				distortionesParameters->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.0f, 1.0f);
				distortionesParameters->raw_algorithm = distortionesParameters->modulation_algorithm;

				if (isLightsTurn) {
					const uint8_t(*palette)[3];
					float zone;
					if (featureMode != distortiones::FEATURE_MODE_META) {
						palette = paletteWarpsFreqsShift;
					}
					else {
						palette = paletteWarpsDefault;
					}

					zone = 8.0f * distortionesParameters->modulation_algorithm;
					MAKE_INTEGRAL_FRACTIONAL(zone);
					int zone_fractional_i = static_cast<int>(zone_fractional * 256);
					for (int i = 0; i < 3; i++) {
						int a = palette[zone_integral][i];
						int b = palette[zone_integral + 1][i];
						lights[LIGHT_ALGORITHM + i].setBrightness(static_cast<float>(a + ((b - a) * zone_fractional_i >> 8)) / 255.f);
					}
				}
			}

			distortionesParameters->modulation_parameter = clamp(params[PARAM_TIMBRE].getValue() + f4Voltages[3], 0.0f, 1.0f);

			distortionesParameters->note = 60.0 * params[PARAM_LEVEL1].getValue() + 12.0 * inputs[INPUT_LEVEL1].getNormalVoltage(2.0) + 12.0;
			distortionesParameters->note += log2f(96000.0f * args.sampleTime) * 12.0f;

			distortionesModulator.Process(inputFrames, outputFrames, 60);
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
		json_object_set_new(rootJ, "mode", json_integer(distortionesModulator.feature_mode()));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		if (json_t* modeJ = json_object_get(rootJ, "mode")) {
			distortionesModulator.set_feature_mode(static_cast<distortiones::FeatureMode>(json_integer_value(modeJ)));
			featureMode = distortionesModulator.feature_mode();
		}
	}

	void setFeatureMode(int modeNumber) {
		featureMode = modeNumber;
		distortionesModulator.set_feature_mode(distortiones::FeatureMode(featureMode));
	}
};

static const std::string distortionesModelLabels[9] = {
	"Binaural doppler panner",
	"Wave folder",
	"Chebyschev wave shaper",
	"Frequency shifter",
	"Dual bit mangler",
	"Comparator with Chebyschev waveshaper",
	"Vocoder",
	"Variable-rate delay",
	"Meta mode",
};

struct DistortionesWidget : ModuleWidget {
	DistortionesWidget(Distortiones* module) {
		setModule(module);
		SanguinePanel* panel = new SanguinePanel("res/backplate_10hp_red.svg", "res/distortiones_faceplate.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(45.963, 12.897),
			module, Distortiones::PARAM_MODE_SWITCH, Distortiones::LIGHT_MODE_SWITCH));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(25.4, 37.486), module, Distortiones::PARAM_ALGORITHM));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(25.4, 37.486),
			module, Distortiones::LIGHT_ALGORITHM));
		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(42.388, 79.669), module, Distortiones::PARAM_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(16.906, 63.862),
			module, Distortiones::PARAM_CARRIER, Distortiones::LIGHT_CARRIER));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.894, 63.862), module, Distortiones::INPUT_ALGORITHM));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(8.412, 79.451), module, Distortiones::PARAM_LEVEL1));
		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(25.4, 79.451), module, Distortiones::PARAM_LEVEL2));

		addInput(createInputCentered<BananutYellow>(millimetersToPixelsVec(8.412, 96.146), module, Distortiones::INPUT_LEVEL1));
		addInput(createInputCentered<BananutBlue>(millimetersToPixelsVec(25.4, 96.146), module, Distortiones::INPUT_LEVEL2));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(42.388, 96.146), module, Distortiones::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.925, 112.172), module, Distortiones::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(18.777, 112.172), module, Distortiones::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(32.044, 112.172), module, Distortiones::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(42.896, 112.172), module, Distortiones::OUTPUT_AUX));
	}

	void appendContextMenu(Menu* menu) override {
		Distortiones* module = dynamic_cast<Distortiones*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Mode", "", [=](Menu* menu) {
			for (int i = 0; i < 9; i++) {
				menu->addChild(createCheckMenuItem(distortionesModelLabels[i], "",
					[=]() {return module->featureMode == i; },
					[=]() {module->setFeatureMode(i); }
				));
			}
			}));
	}
};

Model* modelDistortiones = createModel<Distortiones, DistortionesWidget>("Sanguine-Distortiones");