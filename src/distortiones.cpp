#include "plugin.hpp"
#include "distortiones/dsp/distortiones_modulator.h"
#include "sanguinecomponents.hpp"
#include "warpiespals.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Distortiones : SanguineModule {
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
		ENUMS(LIGHT_MODE, 9),
		LIGHTS_COUNT
	};

	int featureMode = 0;
	int frame[PORT_MAX_CHANNELS] = {};

	const int kLightFrequency = 128;

	dsp::BooleanTrigger btModeSwitch;
	dsp::ClockDivider lightDivider;
	distortiones::DistortionesModulator distortionesModulator[PORT_MAX_CHANNELS];
	distortiones::ShortFrame inputFrames[PORT_MAX_CHANNELS][60] = {};
	distortiones::ShortFrame outputFrames[PORT_MAX_CHANNELS][60] = {};

	bool bModeSwitchEnabled = false;
	bool bLastInModeSwitch = false;

	float lastAlgorithmValue = 0.f;

	distortiones::Parameters* distortionesParameters[PORT_MAX_CHANNELS];

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

		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			memset(&distortionesModulator[i], 0, sizeof(distortiones::DistortionesModulator));
			distortionesModulator[i].Init(96000.0f);
			distortionesParameters[i] = distortionesModulator[i].mutable_parameters();
		}

		featureMode = distortiones::FEATURE_MODE_META;
		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		int channelCount = std::max(std::max(inputs[INPUT_CARRIER].getChannels(), inputs[INPUT_MODULATOR].getChannels()), 1);

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
		const float sampleTime = kLightFrequency * args.sampleTime;

		float algorithmValue = 0.f;

		if (bModeSwitchEnabled) {
			featureMode = static_cast<distortiones::FeatureMode>(params[PARAM_ALGORITHM].getValue());
			distortionesModulator[0].set_feature_mode(distortiones::FeatureMode(featureMode));

			if (isLightsTurn) {
				int8_t ramp = getSystemTimeMs() & 127;
				uint8_t tri = (getSystemTimeMs() & 255) < 128 ? 127 + ramp : 255 - ramp;

				for (int i = 0; i < 3; i++) {
					lights[LIGHT_ALGORITHM + i].setBrightnessSmooth(((paletteWarpsParasiteFeatureMode[featureMode][i] * tri) >> 8) / 255.f, sampleTime);
				}
			}
		}
		else {
			lastAlgorithmValue = params[PARAM_ALGORITHM].getValue();
			algorithmValue = lastAlgorithmValue / 8.0;
		}

		for (int channel = 0; channel < channelCount; channel++) {
			distortionesModulator[channel].set_feature_mode(distortiones::FeatureMode(featureMode));

			distortionesParameters[channel]->carrier_shape = params[PARAM_CARRIER].getValue();

			float_4 f4Voltages;

			// Buffer loop
			if (++frame[channel] >= 60) {
				frame[channel] = 0;

				// LEVEL1 and LEVEL2 normalized values from cv_scaler.cc and a PR by Brian Head to AI's repository.
				f4Voltages[0] = inputs[INPUT_LEVEL1].getNormalVoltage(5.f, channel);
				f4Voltages[1] = inputs[INPUT_LEVEL2].getNormalVoltage(5.f, channel);
				f4Voltages[2] = inputs[INPUT_ALGORITHM].getVoltage(channel);
				f4Voltages[3] = inputs[INPUT_TIMBRE].getVoltage(channel);

				f4Voltages /= 5.f;

				distortionesParameters[channel]->channel_drive[0] = clamp(params[PARAM_LEVEL1].getValue() * f4Voltages[0], 0.f, 1.f);
				distortionesParameters[channel]->channel_drive[1] = clamp(params[PARAM_LEVEL2].getValue() * f4Voltages[1], 0.f, 1.f);

				distortionesParameters[channel]->raw_level[0] = distortionesParameters[channel]->channel_drive[0];
				distortionesParameters[channel]->raw_level[1] = distortionesParameters[channel]->channel_drive[1];



				if (!bModeSwitchEnabled) {
					distortionesParameters[channel]->raw_algorithm_pot = algorithmValue;

					distortionesParameters[channel]->raw_algorithm_cv = clamp(f4Voltages[2], -1.0f, 1.0f);

					distortionesParameters[channel]->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.0f, 1.0f);
					distortionesParameters[channel]->raw_algorithm = distortionesParameters[channel]->modulation_algorithm;
				}

				distortionesParameters[channel]->modulation_parameter = clamp(params[PARAM_TIMBRE].getValue() + f4Voltages[3], 0.0f, 1.0f);

				distortionesParameters[channel]->note = 60.0 * params[PARAM_LEVEL1].getValue() + 12.0
					* inputs[INPUT_LEVEL1].getNormalVoltage(2.0, channel) + 12.0;
				distortionesParameters[channel]->note += log2f(96000.0f * args.sampleTime) * 12.0f;

				distortionesModulator[channel].Process(inputFrames[channel], outputFrames[channel], 60);
			}

			inputFrames[channel][frame[channel]].l = clamp(int(inputs[INPUT_CARRIER].getVoltage(channel) / 16.0 * 32768), -32768, 32767);
			inputFrames[channel][frame[channel]].r = clamp(int(inputs[INPUT_MODULATOR].getVoltage(channel) / 16.0 * 32768), -32768, 32767);

			outputs[OUTPUT_MODULATOR].setVoltage(float(outputFrames[channel][frame[channel]].l) / 32768 * 5.0, channel);
			outputs[OUTPUT_AUX].setVoltage(float(outputFrames[channel][frame[channel]].r) / 32768 * 5.0, channel);
		}

		outputs[OUTPUT_MODULATOR].setChannels(channelCount);
		outputs[OUTPUT_AUX].setChannels(channelCount);

		if (isLightsTurn) {
			lights[LIGHT_CARRIER + 0].value = (distortionesParameters[0]->carrier_shape == 1
				|| distortionesParameters[0]->carrier_shape == 2) ? 1.0 : 0.0;
			lights[LIGHT_CARRIER + 1].value = (distortionesParameters[0]->carrier_shape == 2
				|| distortionesParameters[0]->carrier_shape == 3) ? 1.0 : 0.0;

			lights[LIGHT_MODE_SWITCH].setBrightness(bModeSwitchEnabled ? 1.f : 0.f);

			for (int i = 0; i < 9; i++) {
				lights[LIGHT_MODE + i].setBrightnessSmooth(featureMode == i ? 1.f : 0.f, sampleTime);
			}

			if (!bModeSwitchEnabled) {
				const uint8_t(*palette)[3];
				float zone;
				if (featureMode != distortiones::FEATURE_MODE_META) {
					palette = paletteWarpsFreqsShift;
				}
				else {
					palette = paletteWarpsDefault;
				}

				zone = 8.0f * distortionesParameters[0]->modulation_algorithm;
				MAKE_INTEGRAL_FRACTIONAL(zone);
				int zone_fractional_i = static_cast<int>(zone_fractional * 256);
				for (int i = 0; i < 3; i++) {
					int a = palette[zone_integral][i];
					int b = palette[zone_integral + 1][i];
					lights[LIGHT_ALGORITHM + i].setBrightness(static_cast<float>(a + ((b - a) * zone_fractional_i >> 8)) / 255.f);
				}
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "mode", json_integer(distortionesModulator[0].feature_mode()));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		if (json_t* modeJ = json_object_get(rootJ, "mode")) {
			distortionesModulator[0].set_feature_mode(static_cast<distortiones::FeatureMode>(json_integer_value(modeJ)));
			featureMode = distortionesModulator[0].feature_mode();
		}
	}

	void setFeatureMode(int modeNumber) {
		featureMode = modeNumber;
		distortionesModulator[0].set_feature_mode(distortiones::FeatureMode(featureMode));
	}
};

static const std::vector<std::string> distortionesModelLabels = {
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

struct DistortionesWidget : SanguineModuleWidget {
	DistortionesWidget(Distortiones* module) {
		setModule(module);

		moduleName = "distortiones";
		panelSize = SIZE_10;
		backplateColor = PLATE_RED;

		makePanel();

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

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(33.894, 63.862), module, Distortiones::INPUT_ALGORITHM));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(8.412, 79.451), module, Distortiones::PARAM_LEVEL1));
		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(25.4, 79.451), module, Distortiones::PARAM_LEVEL2));

		addInput(createInputCentered<BananutYellowPoly>(millimetersToPixelsVec(8.412, 96.146), module, Distortiones::INPUT_LEVEL1));
		addInput(createInputCentered<BananutBluePoly>(millimetersToPixelsVec(25.4, 96.146), module, Distortiones::INPUT_LEVEL2));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(42.388, 96.146), module, Distortiones::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.925, 112.172), module, Distortiones::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(18.777, 112.172), module, Distortiones::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(32.044, 112.172), module, Distortiones::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(42.896, 112.172), module, Distortiones::OUTPUT_AUX));

		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(13.849, 58.44), module, Distortiones::LIGHT_MODE + 0));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(3.776, 47.107), module, Distortiones::LIGHT_MODE + 1));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(2.716, 31.98), module, Distortiones::LIGHT_MODE + 2));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(11.122, 19.359), module, Distortiones::LIGHT_MODE + 3));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(25.482, 14.496), module, Distortiones::LIGHT_MODE + 4));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(39.824, 19.413), module, Distortiones::LIGHT_MODE + 5));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(48.183, 32.064), module, Distortiones::LIGHT_MODE + 6));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(47.067, 47.187), module, Distortiones::LIGHT_MODE + 7));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(36.952, 58.483), module, Distortiones::LIGHT_MODE + 8));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Distortiones* module = dynamic_cast<Distortiones*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Mode", distortionesModelLabels,
			[=]() {return module->featureMode; },
			[=](int i) {module->setFeatureMode(i); }
		));
	}
};

Model* modelDistortiones = createModel<Distortiones, DistortionesWidget>("Sanguine-Distortiones");