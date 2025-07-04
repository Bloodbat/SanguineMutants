#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#include "bumps/bumps_generator.h"
#include "bumps/bumps_cv_scaler.h"

#include "aestuscommon.hpp"
#include "temulenti.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Temulenti : SanguineModule {
	enum ParamIds {
		PARAM_MODE,
		PARAM_RANGE,

		PARAM_MODEL,

		PARAM_FREQUENCY,
		PARAM_FM,

		PARAM_SHAPE,
		PARAM_SLOPE,
		PARAM_SMOOTHNESS,

		PARAM_SYNC,

		PARAM_QUANTIZER,

		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_SHAPE,
		INPUT_SLOPE,
		INPUT_SMOOTHNESS,

		INPUT_TRIGGER,
		INPUT_FREEZE,
		INPUT_PITCH,
		INPUT_FM,
		INPUT_LEVEL,

		INPUT_CLOCK,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_HIGH,
		OUTPUT_LOW,
		OUTPUT_UNI,
		OUTPUT_BI,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHT_MODE, 2),
		ENUMS(LIGHT_PHASE, 2),
		ENUMS(LIGHT_RANGE, 2),
		ENUMS(LIGHT_SYNC, 2),
		LIGHT_QUANTIZER1,
		LIGHT_QUANTIZER2,
		LIGHT_QUANTIZER3,
		LIGHTS_COUNT
	};

	struct ModeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			Temulenti* moduleTemulenti = static_cast<Temulenti*>(module);
			switch (moduleTemulenti->generator.feature_mode_) {
			case bumps::Generator::FEAT_MODE_RANDOM:
				return temulenti::drunksModeLabels[moduleTemulenti->generator.mode()];
				break;
			case bumps::Generator::FEAT_MODE_HARMONIC:
				return temulenti::bumpsModeLabels[moduleTemulenti->generator.mode()];
				break;
			case bumps::Generator::FEAT_MODE_SHEEP:
				return aestusCommon::sheepMenuLabels[moduleTemulenti->generator.mode()];
				break;
			default:
				return aestusCommon::modeMenuLabels[moduleTemulenti->generator.mode()];
				break;
			}
		}
	};

	struct RangeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			Temulenti* moduleTemulenti = static_cast<Temulenti*>(module);
			return aestusCommon::rangeMenuLabels[moduleTemulenti->generator.range()];
		}
	};

	bumps::Generator generator;
	int frame = 0;
	static const int kLightsFrequency = 16;
	uint8_t lastGate = 0;
	uint8_t quantize = 0;
	dsp::SchmittTrigger stMode;
	dsp::SchmittTrigger stRange;
	dsp::ClockDivider lightsDivider;
	std::string displayModel = temulenti::displayModels[0];
	bool bUseCalibrationOffset = true;
	bool bLastExternalSync = false;

	Temulenti() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		configButton<ModeParam>(PARAM_MODE, aestusCommon::modelModeHeaders[0]);
		configButton<RangeParam>(PARAM_RANGE, "Frequency range");
		configParam(PARAM_FREQUENCY, -48.f, 48.f, 0.f, "Frequency", " semitones");
		configParam(PARAM_FM, -12.f, 12.f, 0.f, "FM attenuverter", " centitones");
		configParam(PARAM_SHAPE, -1.f, 1.f, 0.f, "Shape", "%", 0, 100);
		configParam(PARAM_SLOPE, -1.f, 1.f, 0.f, "Slope", "%", 0, 100);
		configParam(PARAM_SMOOTHNESS, -1.f, 1.f, 0.f, "Smoothness", "%", 0, 100);

		configSwitch(PARAM_MODEL, 0.f, 3.f, 0.f, "Module model", temulenti::menuLabels);

		configInput(INPUT_SHAPE, "Shape");
		configInput(INPUT_SLOPE, "Slope");
		configInput(INPUT_SMOOTHNESS, "Smoothness");
		configInput(INPUT_TRIGGER, "Trigger");
		configInput(INPUT_FREEZE, "Freeze");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");
		configInput(INPUT_FM, "FM");
		configInput(INPUT_LEVEL, "Level");
		configInput(INPUT_CLOCK, "Clock");

		configOutput(OUTPUT_HIGH, "High tide");
		configOutput(OUTPUT_LOW, "Low tide");
		configOutput(OUTPUT_UNI, "Unipolar");
		configOutput(OUTPUT_BI, "Bipolar");

		configButton(PARAM_SYNC, "Clock sync/PLL mode");

		configSwitch(PARAM_QUANTIZER, 0.f, 7.f, 0.f, "Quantizer scale", temulenti::quantizerLabels);

		memset(&generator, 0, sizeof(bumps::Generator));
		generator.Init();
		lightsDivider.setDivision(kLightsFrequency);
		onReset();
	}

	void process(const ProcessArgs& args) override {
		bumps::GeneratorMode mode = generator.mode();
		if (stMode.process(params[PARAM_MODE].getValue())) {
			mode = bumps::GeneratorMode((static_cast<int>(mode) + 1) % 3);
			generator.set_mode(mode);
		}

		bumps::GeneratorRange range = generator.range();
		if (stRange.process(params[PARAM_RANGE].getValue())) {
			range = bumps::GeneratorRange((static_cast<int>(range) - 1 + 3) % 3);
			generator.set_range(range);
		}

		bool bHaveExternalSync = static_cast<bool>(params[PARAM_SYNC].getValue());

		//Buffer loop
		if (generator.writable_block()) {
			// Sync
			// This takes a moment to catch up if sync is on and patches or presets have just been loaded!
			if (bHaveExternalSync != bLastExternalSync) {
				generator.set_sync(bHaveExternalSync);
				bLastExternalSync = bHaveExternalSync;
			}

			// Pitch
			float pitchParam = params[PARAM_FREQUENCY].getValue() + (inputs[INPUT_PITCH].getVoltage() +
				aestusCommon::calibrationOffsets[bUseCalibrationOffset]) * 12.f;
			float fm = clamp(inputs[INPUT_FM].getNormalVoltage(0.1f) / 5.f * params[PARAM_FM].getValue() / 12.f, -1.f, 1.f) * 1536.f;

			pitchParam += 60.f;
			// This is probably not original but seems useful to keep the same frequency as in normal mode.
			if (generator.feature_mode_ == bumps::Generator::FEAT_MODE_HARMONIC && !bUseCalibrationOffset) {
				pitchParam -= 12.f;
			}

			// This is equivalent to shifting left by 7 bits.
			int16_t pitch = static_cast<int16_t>(pitchParam * 128);

			if ((quantize = params[PARAM_QUANTIZER].getValue())) {
				uint16_t semi = pitch >> 7;
				uint16_t octaves = semi / 12;
				semi -= octaves * 12;
				pitch = octaves * bumps::kOctave + bumps::quantize_lut[quantize - 1][semi];
			}

			// Scale to the global sample rate.
			pitch += log2f(48000.f / args.sampleRate) * 12.f * 128;

			if (generator.feature_mode_ == bumps::Generator::FEAT_MODE_HARMONIC) {
				generator.set_pitch_high_range(clamp(pitch, -32768, 32767), fm);
			} else {
				generator.set_pitch(clamp(pitch, -32768, 32767), fm);
			}

			if (generator.feature_mode_ == bumps::Generator::FEAT_MODE_RANDOM) {
				generator.set_pulse_width(clamp(1.f - -params[PARAM_FM].getValue() / 12.f, 0.f, 2.f) * 32767);
			}

			// Shape, slope, smoothness
			int16_t shape = clamp(params[PARAM_SHAPE].getValue() + inputs[INPUT_SHAPE].getVoltage() / 5.f, -1.f, 1.f) * 32767;
			int16_t slope = clamp(params[PARAM_SLOPE].getValue() + inputs[INPUT_SLOPE].getVoltage() / 5.f, -1.f, 1.f) * 32767;
			int16_t smoothness = clamp(params[PARAM_SMOOTHNESS].getValue() + inputs[INPUT_SMOOTHNESS].getVoltage() / 5.f, -1.f, 1.f) * 32767;
			generator.set_shape(shape);
			generator.set_slope(slope);
			generator.set_smoothness(smoothness);

			generator.FillBuffer();
		}

		// Level
		uint16_t level = clamp(inputs[INPUT_LEVEL].getNormalVoltage(8.f) / 8.f, 0.f, 1.f) * 65535;
		if (level < 32)
		{
			level = 0;
		}

		uint8_t gate = 0;
		if (inputs[INPUT_FREEZE].getVoltage() >= 0.7f) {
			gate |= bumps::CONTROL_FREEZE;
		}
		if (inputs[INPUT_TRIGGER].getVoltage() >= 0.7f) {
			gate |= bumps::CONTROL_GATE;
		}
		if (inputs[INPUT_CLOCK].getVoltage() >= 0.7f) {
			gate |= bumps::CONTROL_CLOCK;
		}
		if (!(lastGate & bumps::CONTROL_CLOCK) && (gate & bumps::CONTROL_CLOCK)) {
			gate |= bumps::CONTROL_CLOCK_RISING;
		}
		if (!(lastGate & bumps::CONTROL_GATE) && (gate & bumps::CONTROL_GATE)) {
			gate |= bumps::CONTROL_GATE_RISING;
		}
		if ((lastGate & bumps::CONTROL_GATE) && !(gate & bumps::CONTROL_GATE)) {
			gate |= bumps::CONTROL_GATE_FALLING;
		}
		lastGate = gate;

		const bumps::GeneratorSample& sample = generator.Process(gate);

		uint32_t uni = sample.unipolar;
		int32_t bi = sample.bipolar;

		uni = uni * level >> 16;
		bi = -bi * level >> 16;
		float unipolarFlag = static_cast<float>(uni) / 65535;
		float bipolarFlag = static_cast<float>(bi) / 32768;

		outputs[OUTPUT_HIGH].setVoltage((sample.flags & bumps::FLAG_END_OF_ATTACK) ? 5.f : 0.f);
		outputs[OUTPUT_LOW].setVoltage((sample.flags & bumps::FLAG_END_OF_RELEASE) ? 5.f : 0.f);
		outputs[OUTPUT_UNI].setVoltage(unipolarFlag * 8.f);
		outputs[OUTPUT_BI].setVoltage(bipolarFlag * 5.f);

		if (lightsDivider.process()) {
			const float sampleTime = kLightsFrequency * args.sampleTime;

			generator.feature_mode_ = bumps::Generator::FeatureMode(params[PARAM_MODEL].getValue());

			lights[LIGHT_MODE + 0].setBrightnessSmooth(mode == bumps::GENERATOR_MODE_AD ?
				kSanguineButtonLightValue : 0.f, sampleTime);
			lights[LIGHT_MODE + 1].setBrightnessSmooth(mode == bumps::GENERATOR_MODE_AR ?
				kSanguineButtonLightValue : 0.f, sampleTime);

			lights[LIGHT_RANGE + 0].setBrightnessSmooth(range == bumps::GENERATOR_RANGE_LOW ?
				kSanguineButtonLightValue : 0.f, sampleTime);
			lights[LIGHT_RANGE + 1].setBrightnessSmooth(range == bumps::GENERATOR_RANGE_HIGH ?
				kSanguineButtonLightValue : 0.f, sampleTime);

			if (sample.flags & bumps::FLAG_END_OF_ATTACK) {
				unipolarFlag *= -1.f;
			}
			lights[LIGHT_PHASE + 0].setBrightnessSmooth(fmaxf(0.f, unipolarFlag), sampleTime);
			lights[LIGHT_PHASE + 1].setBrightnessSmooth(fmaxf(0.f, -unipolarFlag), sampleTime);

			lights[LIGHT_SYNC + 0].setBrightnessSmooth(bHaveExternalSync && !(getSystemTimeMs() & 128) ?
				kSanguineButtonLightValue : 0.f, sampleTime);
			lights[LIGHT_SYNC + 1].setBrightnessSmooth(bHaveExternalSync ? kSanguineButtonLightValue : 0.f, sampleTime);

			if (quantize) {
				lights[LIGHT_QUANTIZER1].setBrightnessSmooth(quantize & 1 ? 1.f : 0.f, sampleTime);
				lights[LIGHT_QUANTIZER2].setBrightnessSmooth(quantize & 2 ? 1.f : 0.f, sampleTime);
				lights[LIGHT_QUANTIZER3].setBrightnessSmooth(quantize & 4 ? 1.f : 0.f, sampleTime);
			} else {
				for (int currentLight = 0; currentLight < 3; ++currentLight) {
					lights[LIGHT_QUANTIZER1 + currentLight].setBrightnessSmooth(0.f, sampleTime);
				}
			}

			displayModel = temulenti::displayModels[params[PARAM_MODEL].getValue()];

			switch (generator.feature_mode_)
			{
			case bumps::Generator::FEAT_MODE_HARMONIC:
				paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[2];
				break;
			case bumps::Generator::FEAT_MODE_SHEEP:
				paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[1];
				break;
			default:
				paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[0];
				break;
			}
		}
	}

	void onReset() override {
		generator.set_mode(bumps::GENERATOR_MODE_LOOPING);
		generator.set_range(bumps::GENERATOR_RANGE_MEDIUM);
		params[PARAM_MODEL].setValue(0.f);
	}

	void onRandomize() override {
		generator.set_range(bumps::GeneratorRange(random::u32() % 3));
		generator.set_mode(bumps::GeneratorMode(random::u32() % 3));
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "mode", static_cast<int>(generator.mode()));
		setJsonInt(rootJ, "range", static_cast<int>(generator.range()));
		setJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "mode", intValue)) {
			generator.set_mode(bumps::GeneratorMode(intValue));
		}

		if (getJsonInt(rootJ, "range", intValue)) {
			generator.set_range(bumps::GeneratorRange(intValue));
		}

		getJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);
	}

	void setModel(int modelNum) {
		params[PARAM_MODEL].setValue(modelNum);
	}

	void setMode(int modeNum) {
		generator.set_mode(bumps::GeneratorMode(modeNum));
	}

	void setRange(int rangeNum) {
		generator.set_range(bumps::GeneratorRange(rangeNum));
	}

	void setQuantizer(int quantizerNum) {
		params[PARAM_QUANTIZER].setValue(quantizerNum);
	}
};

struct TemulentiWidget : SanguineModuleWidget {
	explicit TemulentiWidget(Temulenti* module) {
		setModule(module);

		moduleName = "temulenti";
		panelSize = SIZE_14;
		backplateColor = PLATE_RED;

		makePanel();

		addScrews(SCREW_ALL);

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(11.966, 19.002), module, Temulenti::PARAM_MODEL));

		FramebufferWidget* temulentiFrameBuffer = new FramebufferWidget();
		addChild(temulentiFrameBuffer);

		SanguineTinyNumericDisplay* displayModel = new SanguineTinyNumericDisplay(1, module, 23.42, 17.343);
		displayModel->displayType = DISPLAY_STRING;
		temulentiFrameBuffer->addChild(displayModel);
		displayModel->fallbackString = temulenti::displayModels[0];

		if (module) {
			displayModel->values.displayText = &module->displayModel;
		}

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.56, 19.002), module, Temulenti::PARAM_QUANTIZER));
		addChild(createLightCentered<TinyLight<GreenLight> >(millimetersToPixelsVec(40.438, 16.496), module, Temulenti::LIGHT_QUANTIZER1));
		addChild(createLightCentered<TinyLight<GreenLight> >(millimetersToPixelsVec(40.438, 19.002), module, Temulenti::LIGHT_QUANTIZER2));
		addChild(createLightCentered<TinyLight<GreenLight> >(millimetersToPixelsVec(40.438, 21.496), module, Temulenti::LIGHT_QUANTIZER3));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(59.142, 19.002), module,
			Temulenti::PARAM_SYNC, Temulenti::LIGHT_SYNC));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(11.966, 29.086), module,
			Temulenti::PARAM_MODE, Temulenti::LIGHT_MODE));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(20.888, 37.214), module, Temulenti::LIGHT_PHASE));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(35.56, 37.214), module, Temulenti::PARAM_FREQUENCY));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(59.142, 37.214), module, Temulenti::PARAM_FM));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(11.966, 45.343),
			module, Temulenti::PARAM_RANGE, Temulenti::LIGHT_RANGE));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(11.966, 62.855), module, Temulenti::PARAM_SHAPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(35.56, 62.855), module, Temulenti::PARAM_SLOPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(59.142, 62.855), module, Temulenti::PARAM_SMOOTHNESS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(11.966, 78.995), module, Temulenti::INPUT_SHAPE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(35.56, 78.995), module, Temulenti::INPUT_SLOPE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(59.142, 78.995), module, Temulenti::INPUT_SMOOTHNESS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.665, 95.56), module, Temulenti::INPUT_TRIGGER));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(21.11, 95.56), module, Temulenti::INPUT_FREEZE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(35.554, 95.56), module, Temulenti::INPUT_PITCH));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(49.998, 95.56), module, Temulenti::INPUT_FM));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(64.442, 95.56), module, Temulenti::INPUT_LEVEL));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.665, 111.643), module, Temulenti::INPUT_CLOCK));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(21.11, 111.643), module, Temulenti::OUTPUT_HIGH));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(35.554, 111.643), module, Temulenti::OUTPUT_LOW));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(49.998, 111.643), module, Temulenti::OUTPUT_UNI));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(64.442, 111.643), module, Temulenti::OUTPUT_BI));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Temulenti* module = dynamic_cast<Temulenti*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Model", temulenti::menuLabels,
			[=]() { return module->params[Temulenti::PARAM_MODEL].getValue(); },
			[=](int i) { module->setModel(i); }
		));

		switch (module->generator.feature_mode_)
		{
		case bumps::Generator::FEAT_MODE_RANDOM:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[0], temulenti::drunksModeLabels,
				[=]() { return module->generator.mode(); },
				[=](int i) { module->setMode(i); }
			));
			break;
		case bumps::Generator::FEAT_MODE_HARMONIC:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[2], temulenti::bumpsModeLabels,
				[=]() { return module->generator.mode(); },
				[=](int i) { module->setMode(i); }
			));
			break;
		case bumps::Generator::FEAT_MODE_SHEEP:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[1], aestusCommon::sheepMenuLabels,
				[=]() { return module->generator.mode(); },
				[=](int i) { module->setMode(i); }
			));
			break;
		default:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[0], aestusCommon::modeMenuLabels,
				[=]() { return module->generator.mode(); },
				[=](int i) { module->setMode(i); }
			));
			break;
		}

		menu->addChild(createIndexSubmenuItem("Range", aestusCommon::rangeMenuLabels,
			[=]() { return module->generator.range(); },
			[=](int i) { module->setRange(i); }
		));

		menu->addChild(createIndexSubmenuItem("Quantizer scale", temulenti::quantizerLabels,
			[=]() { return module->params[Temulenti::PARAM_QUANTIZER].getValue(); },
			[=](int i) { module->setQuantizer(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Compatibility options", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("Frequency knob center is C4", "", &module->bUseCalibrationOffset));
			}
		));
	}
};


Model* modelTemulenti = createModel<Temulenti, TemulentiWidget>("Sanguine-Temulenti");
