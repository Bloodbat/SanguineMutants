#include "plugin.hpp"
#include "mutuus/dsp/mutuus_modulator.h"
#include "sanguinecomponents.hpp"
#include "warpiespals.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Mutuus : SanguineModule {
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
		INPUT_MODE,
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
		ENUMS(LIGHT_MODE, 9),
		ENUMS(LIGHT_CHANNEL_MODE, PORT_MAX_CHANNELS * 3),
		LIGHTS_COUNT
	};

	int featureMode = 0;
	int frame[PORT_MAX_CHANNELS] = {};

	const int kLightFrequency = 128;

	dsp::BooleanTrigger btModeSwitch;
	dsp::ClockDivider lightDivider;
	mutuus::MutuusModulator mutuusModulator[PORT_MAX_CHANNELS];
	mutuus::ShortFrame inputFrames[PORT_MAX_CHANNELS][60] = {};
	mutuus::ShortFrame outputFrames[PORT_MAX_CHANNELS][60] = {};

	bool bModeSwitchEnabled = false;
	bool bLastInModeSwitch = false;

	float lastAlgorithmValue = 0.f;

	mutuus::Parameters* mutuusParameters[PORT_MAX_CHANNELS];

	uint16_t reverbBuffer[PORT_MAX_CHANNELS][32768] = {};

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

		configInput(INPUT_MODE, "Mode");

		configBypass(INPUT_MODULATOR, OUTPUT_MODULATOR);

		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			memset(&mutuusModulator[i], 0, sizeof(mutuus::MutuusModulator));
			mutuusModulator[i].Init(96000.0f, reverbBuffer[i]);
			mutuusParameters[i] = mutuusModulator[i].mutable_parameters();
		}

		featureMode = mutuus::FEATURE_MODE_META;
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
		float sampleTime = kLightFrequency * args.sampleTime;

		float algorithmValue = 0.f;

		if (bModeSwitchEnabled) {
			featureMode = static_cast<mutuus::FeatureMode>(params[PARAM_ALGORITHM].getValue());
			mutuusModulator[0].set_feature_mode(mutuus::FeatureMode(featureMode));
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
			mutuus::FeatureMode channelFeatureMode = mutuus::FeatureMode(featureMode);

			if (inputs[INPUT_MODE].isConnected()) {
				channelFeatureMode = mutuus::FeatureMode(clamp(int(inputs[INPUT_MODE].getVoltage(channel)), 0, 8));
			}

			mutuusModulator[channel].set_feature_mode(channelFeatureMode);

			mutuusParameters[channel]->carrier_shape = params[PARAM_CARRIER].getValue();

			mutuusModulator[channel].set_alt_feature_mode(bool(params[PARAM_STEREO].getValue()));

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

				mutuusParameters[channel]->channel_drive[0] = clamp(params[PARAM_LEVEL1].getValue() * f4Voltages[0], 0.f, 1.f);
				mutuusParameters[channel]->channel_drive[1] = clamp(params[PARAM_LEVEL2].getValue() * f4Voltages[1], 0.f, 1.f);

				mutuusParameters[channel]->raw_level_cv[0] = clamp(f4Voltages[0], 0.f, 1.f);
				mutuusParameters[channel]->raw_level_cv[1] = clamp(f4Voltages[1], 0.f, 1.f);

				mutuusParameters[channel]->raw_level[0] = mutuusParameters[channel]->channel_drive[0];
				mutuusParameters[channel]->raw_level[1] = mutuusParameters[channel]->channel_drive[1];

				mutuusParameters[channel]->raw_level_pot[0] = clamp(params[PARAM_LEVEL1].getValue(), 0.0f, 1.0f);
				mutuusParameters[channel]->raw_level_pot[1] = clamp(params[PARAM_LEVEL2].getValue(), 0.0f, 1.0f);

				if (!bModeSwitchEnabled) {
					mutuusParameters[channel]->raw_algorithm_pot = algorithmValue;

					mutuusParameters[channel]->raw_algorithm_cv = clamp(f4Voltages[2], -1.0f, 1.0f);

					mutuusParameters[channel]->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.0f, 1.0f);
					mutuusParameters[channel]->raw_algorithm = mutuusParameters[channel]->modulation_algorithm;
				}

				mutuusParameters[channel]->modulation_parameter = clamp(params[PARAM_TIMBRE].getValue() + f4Voltages[3], 0.0f, 1.0f);
				mutuusParameters[channel]->raw_modulation = mutuusParameters[channel]->modulation_parameter;
				mutuusParameters[channel]->raw_modulation_pot = clamp(params[PARAM_TIMBRE].getValue(), 0.f, 1.f);
				mutuusParameters[channel]->raw_modulation_cv = clamp(f4Voltages[3], -1.f, 1.f);

				mutuusParameters[channel]->note = 60.0 * params[PARAM_LEVEL1].getValue() + 12.0
					* inputs[INPUT_LEVEL1].getNormalVoltage(2.0, channel) + 12.0;
				mutuusParameters[channel]->note += log2f(96000.0f * args.sampleTime) * 12.0f;

				mutuusModulator[channel].Process(inputFrames[channel], outputFrames[channel], 60);
			}

			inputFrames[channel][frame[channel]].l = clamp(int(inputs[INPUT_CARRIER].getVoltage(channel) / 16.0 * 32768), -32768, 32767);
			inputFrames[channel][frame[channel]].r = clamp(int(inputs[INPUT_MODULATOR].getVoltage(channel) / 16.0 * 32768), -32768, 32767);

			outputs[OUTPUT_MODULATOR].setVoltage(float(outputFrames[channel][frame[channel]].l) / 32768 * 5.0, channel);
			outputs[OUTPUT_AUX].setVoltage(float(outputFrames[channel][frame[channel]].r) / 32768 * 5.0, channel);
		}

		outputs[OUTPUT_MODULATOR].setChannels(channelCount);
		outputs[OUTPUT_AUX].setChannels(channelCount);

		if (isLightsTurn) {
			lights[LIGHT_CARRIER + 0].value = (mutuusParameters[0]->carrier_shape == 1
				|| mutuusParameters[0]->carrier_shape == 2) ? 1.0 : 0.0;
			lights[LIGHT_CARRIER + 1].value = (mutuusParameters[0]->carrier_shape == 2
				|| mutuusParameters[0]->carrier_shape == 3) ? 1.0 : 0.0;

			lights[LIGHT_MODE_SWITCH].setBrightness(bModeSwitchEnabled ? 1.f : 0.f);

			lights[LIGHT_STEREO].setBrightness(mutuusModulator[0].alt_feature_mode() ? 1.f : 0.f);

			for (int i = 0; i < 9; i++) {
				lights[LIGHT_MODE + i].setBrightnessSmooth(featureMode == i ? 1.f : 0.f, sampleTime);

				if (!bModeSwitchEnabled) {
					const uint8_t(*palette)[3];
					float zone;
					if (featureMode != mutuus::FEATURE_MODE_META) {
						palette = paletteWarpsFreqsShift;
					}
					else {
						palette = paletteWarpsDefault;
					}

					zone = 8.0f * mutuusParameters[0]->modulation_algorithm;
					MAKE_INTEGRAL_FRACTIONAL(zone);
					int zone_fractional_i = static_cast<int>(zone_fractional * 256);
					for (int i = 0; i < 3; i++) {
						int a = palette[zone_integral][i];
						int b = palette[zone_integral + 1][i];
						lights[LIGHT_ALGORITHM + i].setBrightness(static_cast<float>(a + ((b - a) * zone_fractional_i >> 8)) / 255.f);
					}
				}
			}


			for (int channel = 0; channel < channelCount; channel++) {
				const int currentLight = LIGHT_CHANNEL_MODE + channel * 3;

				for (int i = 0; i < 3; i++) {
					lights[currentLight + i].setBrightnessSmooth((paletteWarpsParasiteFeatureMode[mutuusModulator[channel].feature_mode()][i]) / 255.f, sampleTime);
				}
			}

			for (int channel = channelCount; channel < PORT_MAX_CHANNELS; channel++) {
				const int currentLight = LIGHT_CHANNEL_MODE + channel * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "mode", json_integer(mutuusModulator[0].feature_mode()));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		if (json_t* modeJ = json_object_get(rootJ, "mode")) {
			mutuusModulator[0].set_feature_mode(static_cast<mutuus::FeatureMode>(json_integer_value(modeJ)));
			featureMode = mutuusModulator[0].feature_mode();
		}
	}

	void setFeatureMode(int modeNumber) {
		featureMode = modeNumber;
		mutuusModulator[0].set_feature_mode(mutuus::FeatureMode(featureMode));
	}
};

static const std::vector<std::string> mutuusModelLabels = {
	"Dual state variable filter",
	"Ensemble FX",
	"Reverbs",
	"Frequency shifter",
	"Bit crusher",
	"Chebyschev wave shaper",
	"Doppler panner",
	"Delay",
	"Meta mode",
};

struct MutuusWidget : SanguineModuleWidget {
	MutuusWidget(Mutuus* module) {
		setModule(module);

		moduleName = "mutuus";
		panelSize = SIZE_10;
		backplateColor = PLATE_GREEN;

		makePanel();

		addScrews(SCREW_ALL);

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(millimetersToPixelsVec(4.836, 16.119),
			module, Mutuus::PARAM_STEREO, Mutuus::LIGHT_STEREO));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(45.963, 16.119),
			module, Mutuus::PARAM_MODE_SWITCH, Mutuus::LIGHT_MODE_SWITCH));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(25.4, 37.486), module, Mutuus::PARAM_ALGORITHM));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(25.4, 37.486), module, Mutuus::LIGHT_ALGORITHM));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(8.412, 63.862), module, Mutuus::INPUT_MODE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(25.4, 63.862),
			module, Mutuus::PARAM_CARRIER, Mutuus::LIGHT_CARRIER));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(42.388, 63.862), module, Mutuus::INPUT_ALGORITHM));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(8.412, 79.451), module, Mutuus::PARAM_LEVEL1));
		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(25.4, 79.451), module, Mutuus::PARAM_LEVEL2));
		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(42.388, 79.669), module, Mutuus::PARAM_TIMBRE));

		addInput(createInputCentered<BananutYellowPoly>(millimetersToPixelsVec(8.412, 96.146), module, Mutuus::INPUT_LEVEL1));
		addInput(createInputCentered<BananutBluePoly>(millimetersToPixelsVec(25.4, 96.146), module, Mutuus::INPUT_LEVEL2));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(42.388, 96.146), module, Mutuus::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.925, 112.172), module, Mutuus::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(18.777, 112.172), module, Mutuus::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(32.044, 112.172), module, Mutuus::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(42.896, 112.172), module, Mutuus::OUTPUT_AUX));

		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(13.849, 58.44), module, Mutuus::LIGHT_MODE + 0));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(3.776, 47.107), module, Mutuus::LIGHT_MODE + 1));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(2.716, 31.98), module, Mutuus::LIGHT_MODE + 2));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(11.122, 19.359), module, Mutuus::LIGHT_MODE + 3));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(25.482, 14.496), module, Mutuus::LIGHT_MODE + 4));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(39.824, 19.413), module, Mutuus::LIGHT_MODE + 5));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(48.183, 32.064), module, Mutuus::LIGHT_MODE + 6));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(47.067, 47.187), module, Mutuus::LIGHT_MODE + 7));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(36.952, 58.483), module, Mutuus::LIGHT_MODE + 8));

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(14.281, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 0 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.398, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 1 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(18.516, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 2 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.633, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 3 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(30.148, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 4 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(32.265, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 5 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(34.382, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 6 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(36.5, 62.532), module, Mutuus::LIGHT_CHANNEL_MODE + 7 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(14.281, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 8 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.398, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 9 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(18.516, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 10 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.633, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 11 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(30.148, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 12 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(32.265, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 13 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(34.382, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 14 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(36.5, 65.191), module, Mutuus::LIGHT_CHANNEL_MODE + 15 * 3));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Mutuus* module = dynamic_cast<Mutuus*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Mode", mutuusModelLabels,
			[=]() {return module->featureMode; },
			[=](int i) {module->setFeatureMode(i); }
		));
	}
};

Model* modelMutuus = createModel<Mutuus, MutuusWidget>("Sanguine-Mutuus");