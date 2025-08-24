#include "plugin.hpp"

#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"
#include "array"

#include "mutuus/dsp/mutuus_modulator.h"

#include "warpiescommon.hpp"
#include "warpiespals.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Mutuus : SanguineModule {
	static const int kModeCount = 9;

	enum ParamIds {
		PARAM_ALGORITHM,
		PARAM_TIMBRE,
		PARAM_CARRIER,
		PARAM_LEVEL_1,
		PARAM_LEVEL_2,
		PARAM_MODE_SWITCH,
		PARAM_STEREO,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_LEVEL_1,
		INPUT_LEVEL_2,
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
		ENUMS(LIGHT_MODE, kModeCount),
		ENUMS(LIGHT_CHANNEL_MODE, PORT_MAX_CHANNELS * 3),
		LIGHTS_COUNT
	};

	int featureMode = 0;
	int frames[PORT_MAX_CHANNELS] = {};

	static const int kLightsFrequency = 128;

	dsp::BooleanTrigger btModeSwitch;
	dsp::ClockDivider lightsDivider;
	mutuus::MutuusModulator modulators[PORT_MAX_CHANNELS];
	mutuus::ShortFrame inputFrames[PORT_MAX_CHANNELS][warpiescommon::kBlockSize] = {};
	mutuus::ShortFrame outputFrames[PORT_MAX_CHANNELS][warpiescommon::kBlockSize] = {};

	bool bModeSwitchEnabled = false;
	bool bLastInModeSwitch = false;

	bool bNotesModeSelection = false;

	bool bHaveModeCable = false;

	float lastAlgorithmValue = 0.f;

	mutuus::Parameters* parameters[PORT_MAX_CHANNELS];

	uint16_t reverbBuffers[PORT_MAX_CHANNELS][32768] = {};

	Mutuus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configButton(PARAM_STEREO, "Dual filter stereo");

		configButton(PARAM_MODE_SWITCH, "Mode select");

		configParam(PARAM_ALGORITHM, 0.f, 8.f, 0.f, "Algorithm");

		configParam(PARAM_CARRIER, 0.f, 3.f, 0.f, "Internal oscillator mode");

		configParam(PARAM_TIMBRE, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);

		configParam(PARAM_LEVEL_1, 0.f, 1.f, 1.f, "External oscillator amplitude / internal oscillator frequency", "%", 0.f, 100.f);
		configParam(PARAM_LEVEL_2, 0.f, 1.f, 1.f, "Modulator amplitude", "%", 0.f, 100.f);

		configInput(INPUT_LEVEL_1, "Level 1");
		configInput(INPUT_LEVEL_2, "Level 2");
		configInput(INPUT_ALGORITHM, "Algorithm");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_CARRIER, "Carrier");
		configInput(INPUT_MODULATOR, "Modulator");

		configOutput(OUTPUT_MODULATOR, "Modulator");
		configOutput(OUTPUT_AUX, "Auxiliary");

		configInput(INPUT_MODE, "Mode");

		configBypass(INPUT_MODULATOR, OUTPUT_MODULATOR);

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&modulators[channel], 0, sizeof(mutuus::MutuusModulator));
			modulators[channel].Init(96000.f, reverbBuffers[channel]);
			parameters[channel] = modulators[channel].mutable_parameters();
		}

		featureMode = mutuus::FEATURE_MODE_META;
		lightsDivider.setDivision(kLightsFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		std::array <mutuus::FeatureMode, PORT_MAX_CHANNELS> channelFeatureModes;

		int channelCount = std::max(std::max(inputs[INPUT_CARRIER].getChannels(), inputs[INPUT_MODULATOR].getChannels()), 1);

		int32_t internalOscillator = static_cast<int32_t>(params[PARAM_CARRIER].getValue());

		float algorithmValue = 0.f;

		float knobLevel1 = params[PARAM_LEVEL_1].getValue();
		float knobLevel2 = params[PARAM_LEVEL_2].getValue();

		float knobTimbre = params[PARAM_TIMBRE].getValue();

		float knobAlgorithm = params[PARAM_ALGORITHM].getValue();

		bool bWantStereo = static_cast<bool>(params[PARAM_STEREO].getValue());

		// TODO: FIX ME!!! Mode selection snapping to the wrong value when switching back to mode selection after first time.
		if (btModeSwitch.process(params[PARAM_MODE_SWITCH].getValue())) {
			bModeSwitchEnabled = !bModeSwitchEnabled;

			paramQuantities[PARAM_ALGORITHM]->snapEnabled = bModeSwitchEnabled;

			if (bLastInModeSwitch) {
				params[PARAM_ALGORITHM].setValue(lastAlgorithmValue);
			} else {
				params[PARAM_ALGORITHM].setValue(featureMode);
			}

			bLastInModeSwitch = bModeSwitchEnabled;
		}

		if (!bModeSwitchEnabled) {
			lastAlgorithmValue = knobAlgorithm;
			algorithmValue = lastAlgorithmValue / 8.f;
		}

		channelFeatureModes.fill(static_cast<mutuus::FeatureMode>(featureMode));

		if (bHaveModeCable) {
			for (int channel = 0; channel < channelCount; channel += 4) {
				float_4 modeVoltages;

				modeVoltages = inputs[INPUT_MODE].getVoltageSimd<float_4>(channel);

				if (bNotesModeSelection) {
					modeVoltages *= 12.f;
					modeVoltages = simd::round(modeVoltages);
				}

				modeVoltages = simd::clamp(modeVoltages, 0.f, 8.f);

				channelFeatureModes[channel] = static_cast<mutuus::FeatureMode>(modeVoltages[0]);
				channelFeatureModes[channel + 1] = static_cast<mutuus::FeatureMode>(modeVoltages[1]);
				channelFeatureModes[channel + 2] = static_cast<mutuus::FeatureMode>(modeVoltages[2]);
				channelFeatureModes[channel + 3] = static_cast<mutuus::FeatureMode>(modeVoltages[3]);
			}
		}

		for (int channel = 0; channel < channelCount; ++channel) {
			modulators[channel].set_feature_mode(channelFeatureModes[channel]);

			parameters[channel]->carrier_shape = internalOscillator;

			modulators[channel].set_alt_feature_mode(bWantStereo);

			float_4 f4Voltages;

			// Buffer loop
			if (++frames[channel] >= warpiescommon::kBlockSize) {
				frames[channel] = 0;

				// LEVEL1 and LEVEL2 normalized values from cv_scaler.cc and a PR by Brian Head to AI's repository.
				f4Voltages[0] = inputs[INPUT_LEVEL_1].getNormalVoltage(5.f, channel);
				f4Voltages[1] = inputs[INPUT_LEVEL_2].getNormalVoltage(5.f, channel);
				f4Voltages[2] = inputs[INPUT_ALGORITHM].getVoltage(channel);
				f4Voltages[3] = inputs[INPUT_TIMBRE].getVoltage(channel);

				f4Voltages /= 5.f;

				parameters[channel]->channel_drive[0] = clamp(knobLevel1 * f4Voltages[0], 0.f, 1.f);
				parameters[channel]->channel_drive[1] = clamp(knobLevel2 * f4Voltages[1], 0.f, 1.f);

				parameters[channel]->raw_level_cv[0] = clamp(f4Voltages[0], 0.f, 1.f);
				parameters[channel]->raw_level_cv[1] = clamp(f4Voltages[1], 0.f, 1.f);

				parameters[channel]->raw_level[0] = parameters[channel]->channel_drive[0];
				parameters[channel]->raw_level[1] = parameters[channel]->channel_drive[1];

				parameters[channel]->raw_level_pot[0] = knobLevel1;
				parameters[channel]->raw_level_pot[1] = knobLevel2;

				if (!bModeSwitchEnabled) {
					parameters[channel]->raw_algorithm_pot = algorithmValue;

					parameters[channel]->raw_algorithm_cv = clamp(f4Voltages[2], -1.f, 1.f);

					parameters[channel]->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.f, 1.f);
					parameters[channel]->raw_algorithm = parameters[channel]->modulation_algorithm;
				}

				parameters[channel]->modulation_parameter = clamp(knobTimbre + f4Voltages[3], 0.f, 1.f);
				parameters[channel]->raw_modulation = parameters[channel]->modulation_parameter;
				parameters[channel]->raw_modulation_pot = knobTimbre;
				parameters[channel]->raw_modulation_cv = clamp(f4Voltages[3], -1.f, 1.f);

				parameters[channel]->note = 60.f * knobLevel1 + 12.f
					* inputs[INPUT_LEVEL_1].getNormalVoltage(2.f, channel) + 12.f;
				parameters[channel]->note += log2f(96000.f * args.sampleTime) * 12.f;

				modulators[channel].Process(inputFrames[channel], outputFrames[channel], warpiescommon::kBlockSize);
			}

			inputFrames[channel][frames[channel]].l = clamp(static_cast<int>(inputs[INPUT_CARRIER].getVoltage(channel) / 8.f * 32768),
				-32768, 32767);
			inputFrames[channel][frames[channel]].r = clamp(static_cast<int>(inputs[INPUT_MODULATOR].getVoltage(channel) / 8.f * 32768),
				-32768, 32767);

			outputs[OUTPUT_MODULATOR].setVoltage(static_cast<float>(outputFrames[channel][frames[channel]].l) / 32768 * 5.f, channel);
			outputs[OUTPUT_AUX].setVoltage(static_cast<float>(outputFrames[channel][frames[channel]].r) / 32768 * 5.f, channel);
		}

		outputs[OUTPUT_MODULATOR].setChannels(channelCount);
		outputs[OUTPUT_AUX].setChannels(channelCount);

		if (lightsDivider.process()) {
			const float sampleTime = kLightsFrequency * args.sampleTime;

			lights[LIGHT_CARRIER].setBrightness(((parameters[0]->carrier_shape == 1) |
				(parameters[0]->carrier_shape == 2)) * kSanguineButtonLightValue);
			lights[LIGHT_CARRIER + 1].setBrightness(((parameters[0]->carrier_shape == 2) |
				(parameters[0]->carrier_shape == 3)) * kSanguineButtonLightValue);

			lights[LIGHT_MODE_SWITCH].setBrightness(bModeSwitchEnabled);

			lights[LIGHT_STEREO].setBrightness(modulators[0].alt_feature_mode() * kSanguineButtonLightValue);

			for (int mode = 0; mode < kModeCount; ++mode) {
				lights[LIGHT_MODE + mode].setBrightnessSmooth(featureMode == mode, sampleTime);
			}

			if (!bModeSwitchEnabled) {
				float zone;
				const uint8_t(*palette)[3] = featureMode != mutuus::FEATURE_MODE_META ?
					warpiespals::paletteFreqsShift : warpiespals::paletteDefault;

				zone = 8.f * parameters[0]->modulation_algorithm;
				MAKE_INTEGRAL_FRACTIONAL(zone);
				int integerZoneFractional = static_cast<int>(zone_fractional * 256);

				int aRed = palette[zone_integral][0];
				int bRed = palette[zone_integral + 1][0];

				int aGreen = palette[zone_integral][1];
				int bGreen = palette[zone_integral + 1][1];

				int aBlue = palette[zone_integral][2];
				int bBlue = palette[zone_integral + 1][2];

				float redValue = static_cast<float>(aRed + ((bRed - aRed) * integerZoneFractional >> 8)) / 255.f;
				float greenValue = static_cast<float>(aGreen + ((bGreen - aGreen) * integerZoneFractional >> 8)) / 255.f;
				float blueValue = static_cast<float>(aBlue + ((bBlue - aBlue) * integerZoneFractional >> 8)) / 255.f;

				lights[LIGHT_ALGORITHM].setBrightness(redValue);
				lights[LIGHT_ALGORITHM + 1].setBrightness(greenValue);
				lights[LIGHT_ALGORITHM + 2].setBrightness(blueValue);
			} else {
				featureMode = static_cast<mutuus::FeatureMode>(knobAlgorithm);
				modulators[0].set_feature_mode(mutuus::FeatureMode(featureMode));

				long long systemTimeMs = getSystemTimeMs();

				int8_t ramp = systemTimeMs & 127;
				uint8_t tri = (systemTimeMs & 255) < 128 ? 127 + ramp : 255 - ramp;

				lights[LIGHT_ALGORITHM].setBrightnessSmooth((
					(warpiespals::paletteParasiteFeatureMode[featureMode][0] * tri) >> 8) / 255.f, sampleTime);
				lights[LIGHT_ALGORITHM + 1].setBrightnessSmooth((
					(warpiespals::paletteParasiteFeatureMode[featureMode][1] * tri) >> 8) / 255.f, sampleTime);
				lights[LIGHT_ALGORITHM + 2].setBrightnessSmooth((
					(warpiespals::paletteParasiteFeatureMode[featureMode][2] * tri) >> 8) / 255.f, sampleTime);
			}

			for (int channel = 0; channel < channelCount; ++channel) {
				const int currentLight = LIGHT_CHANNEL_MODE + channel * 3;

				mutuus::FeatureMode featureMode = modulators[channel].feature_mode();

				float_4 rawFeatureMode;

				rawFeatureMode[0] = warpiespals::paletteParasiteFeatureMode[featureMode][0];
				rawFeatureMode[1] = warpiespals::paletteParasiteFeatureMode[featureMode][1];
				rawFeatureMode[2] = warpiespals::paletteParasiteFeatureMode[featureMode][2];

				rawFeatureMode /= 255.f;

				lights[currentLight].setBrightnessSmooth(rawFeatureMode[0], sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(rawFeatureMode[1], sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(rawFeatureMode[2], sampleTime);
			}

			for (int channel = channelCount; channel < PORT_MAX_CHANNELS; ++channel) {
				const int currentLight = LIGHT_CHANNEL_MODE + channel * 3;

				lights[currentLight].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
		}
	}

	void onPortChange(const PortChangeEvent& e) override {
		if (e.type == Port::INPUT && e.portId == INPUT_MODE) {
			bHaveModeCable = e.connecting;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "mode", static_cast<int>(modulators[0].feature_mode()));
		setJsonBoolean(rootJ, "NotesModeSelection", bNotesModeSelection);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "mode", intValue)) {
			modulators[0].set_feature_mode(static_cast<mutuus::FeatureMode>(intValue));
			featureMode = modulators[0].feature_mode();
		}

		getJsonBoolean(rootJ, "NotesModeSelection", bNotesModeSelection);
	}

	void setFeatureMode(int modeNumber) {
		featureMode = modeNumber;
		modulators[0].set_feature_mode(mutuus::FeatureMode(featureMode));
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
	explicit MutuusWidget(Mutuus* module) {
		setModule(module);

		moduleName = "mutuus";
		panelSize = sanguineThemes::SIZE_10;
		backplateColor = sanguineThemes::PLATE_GREEN;

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

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(8.412, 79.451), module, Mutuus::PARAM_LEVEL_1));
		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(25.4, 79.451), module, Mutuus::PARAM_LEVEL_2));
		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(42.388, 79.669), module, Mutuus::PARAM_TIMBRE));

		addInput(createInputCentered<BananutYellowPoly>(millimetersToPixelsVec(8.412, 96.146), module, Mutuus::INPUT_LEVEL_1));
		addInput(createInputCentered<BananutBluePoly>(millimetersToPixelsVec(25.4, 96.146), module, Mutuus::INPUT_LEVEL_2));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(42.388, 96.146), module, Mutuus::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.925, 112.172), module, Mutuus::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(18.777, 112.172), module, Mutuus::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(32.044, 112.172), module, Mutuus::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(42.896, 112.172), module, Mutuus::OUTPUT_AUX));

		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(13.849, 58.44), module, Mutuus::LIGHT_MODE));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(3.776, 47.107), module, Mutuus::LIGHT_MODE + 1));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(2.716, 31.98), module, Mutuus::LIGHT_MODE + 2));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(11.122, 19.359), module, Mutuus::LIGHT_MODE + 3));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(25.482, 14.496), module, Mutuus::LIGHT_MODE + 4));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(39.824, 19.413), module, Mutuus::LIGHT_MODE + 5));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(48.183, 32.064), module, Mutuus::LIGHT_MODE + 6));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(47.067, 47.187), module, Mutuus::LIGHT_MODE + 7));
		addChild(createLightCentered<TinyLight<RedLight>>(millimetersToPixelsVec(36.952, 58.483), module, Mutuus::LIGHT_MODE + 8));

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(14.281, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.398, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE + 1 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(18.516, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE + 2 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.633, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE + 3 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(30.148, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE + 4 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(32.265, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE + 5 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(34.382, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE + 6 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(36.5, 62.532), module,
			Mutuus::LIGHT_CHANNEL_MODE + 7 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(14.281, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 8 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.398, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 9 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(18.516, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 10 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.633, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 11 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(30.148, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 12 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(32.265, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 13 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(34.382, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 14 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(36.5, 65.191), module,
			Mutuus::LIGHT_CHANNEL_MODE + 15 * 3));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Mutuus* module = dynamic_cast<Mutuus*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Mode", mutuusModelLabels,
			[=]() {return module->featureMode; },
			[=](int i) {module->setFeatureMode(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("C4-G#4 direct mode selection", "", &module->bNotesModeSelection));
	}
};

Model* modelMutuus = createModel<Mutuus, MutuusWidget>("Sanguine-Mutuus");