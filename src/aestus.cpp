#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#include "tides/generator.h"
#include "tides/plotter.h"

#include "aestuscommon.hpp"
#include "aestus.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Aestus : SanguineModule {
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
		LIGHTS_COUNT
	};

	struct ModeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			Aestus* moduleAestus = static_cast<Aestus*>(module);
			if (!moduleAestus->bUseSheepFirmware) {
				return aestusCommon::modeMenuLabels[moduleAestus->generator.mode()];
			} else {
				return aestusCommon::sheepMenuLabels[moduleAestus->generator.mode()];
			}
		}
	};

	struct RangeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			Aestus* moduleAestus = static_cast<Aestus*>(module);
			return aestusCommon::rangeMenuLabels[moduleAestus->generator.range()];
		}
	};

	bool bUseSheepFirmware = false;
	bool bUseCalibrationOffset = true;
	bool bLastExternalSync = false;
	bool bWantPeacocks = false;
	tides::Generator generator;
	tides::Plotter plotter;
	int frame = 0;
	static const int kLightsFrequency = 16;
	uint8_t lastGate = 0;
	dsp::SchmittTrigger stMode;
	dsp::SchmittTrigger stRange;
	dsp::ClockDivider lightsDivider;
	std::string displayModel = aestus::displayModels[0];

	Aestus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		configButton<ModeParam>(PARAM_MODE, aestusCommon::modelModeHeaders[0]);
		configButton<RangeParam>(PARAM_RANGE, "Frequency range");
		configParam(PARAM_FREQUENCY, -48.f, 48.f, 0.f, "Frequency", " semitones");
		configParam(PARAM_FM, -12.f, 12.f, 0.f, "FM attenuverter", " centitones");
		configParam(PARAM_SHAPE, -1.f, 1.f, 0.f, "Shape", "%", 0, 100);
		configParam(PARAM_SLOPE, -1.f, 1.f, 0.f, "Slope", "%", 0, 100);
		configParam(PARAM_SMOOTHNESS, -1.f, 1.f, 0.f, "Smoothness", "%", 0, 100);

		configSwitch(PARAM_MODEL, 0.f, 1.f, 0.f, "Module model", aestus::menuLabels);

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

		memset(&generator, 0, sizeof(tides::Generator));
		generator.Init();
		plotter.Init(tides::plotInstructions, sizeof(tides::plotInstructions) / sizeof(tides::PlotInstruction));
		lightsDivider.setDivision(kLightsFrequency);
		onReset();
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		bool bIsLightsTurn = lightsDivider.process();

		if (!bWantPeacocks) {
			tides::GeneratorMode mode = generator.mode();
			if (stMode.process(params[PARAM_MODE].getValue())) {
				mode = tides::GeneratorMode((static_cast<int>(mode) + 1) % 3);
				generator.set_mode(mode);
			}

			tides::GeneratorRange range = generator.range();
			if (stRange.process(params[PARAM_RANGE].getValue())) {
				range = tides::GeneratorRange((static_cast<int>(range) - 1 + 3) % 3);
				generator.set_range(range);
			}

			bUseSheepFirmware = static_cast<bool>(params[PARAM_MODEL].getValue());

			bool bHaveExternalSync = static_cast<bool>(params[PARAM_SYNC].getValue()) || (!bUseSheepFirmware && inputs[INPUT_CLOCK].isConnected());

			// Buffer loop
			if (++frame >= 16) {
				frame = 0;

				// Sync
				// This takes a moment to catch up if sync is on and patches or presets have just been loaded!
				if (bHaveExternalSync != bLastExternalSync) {
					generator.set_sync(bHaveExternalSync);
					bLastExternalSync = bHaveExternalSync;
				}

				// Setup SIMD voltages
				float_4 paramVoltages = {};

				paramVoltages[0] = inputs[INPUT_FM].getNormalVoltage(0.1f);
				paramVoltages[1] = inputs[INPUT_SHAPE].getVoltage();
				paramVoltages[2] = inputs[INPUT_SLOPE].getVoltage();
				paramVoltages[3] = inputs[INPUT_SMOOTHNESS].getVoltage();

				paramVoltages /= 5.f;

				// Pitch.
				float pitch = params[PARAM_FREQUENCY].getValue();
				pitch += 12.f * (inputs[INPUT_PITCH].getVoltage() + aestusCommon::calibrationOffsets[bUseCalibrationOffset]);
				pitch += params[PARAM_FM].getValue() * paramVoltages[0];
				pitch += 60.f;
				// Scale to the global sample rate
				pitch += log2f(48000.f / args.sampleRate) * 12.f;
				generator.set_pitch(static_cast<int>(clamp(pitch * 128.f, static_cast<float>(-32768), static_cast<float>(32767))));

				// Shape, slope, smoothness.
				paramVoltages[1] += params[PARAM_SHAPE].getValue();
				paramVoltages[2] += params[PARAM_SLOPE].getValue();
				paramVoltages[3] += params[PARAM_SMOOTHNESS].getValue();

				paramVoltages = simd::clamp(paramVoltages, -1.f, 1.f);
				paramVoltages *= 32767.f;

				int16_t shape = paramVoltages[1];
				int16_t slope = paramVoltages[2];
				int16_t smoothness = paramVoltages[3];
				generator.set_shape(shape);
				generator.set_slope(slope);
				generator.set_smoothness(smoothness);

				// Generator
				generator.Process(bUseSheepFirmware);
			}

			// Level
			uint16_t level = clamp(inputs[INPUT_LEVEL].getNormalVoltage(8.f) / 8.f, 0.f, 1.f)
				* 65535;
			if (level < 32) {
				level = 0;
			}

			uint8_t gate = 0;
			if (inputs[INPUT_FREEZE].getVoltage() >= 0.7f) {
				gate |= tides::CONTROL_FREEZE;
			}
			if (inputs[INPUT_TRIGGER].getVoltage() >= 0.7f) {
				gate |= tides::CONTROL_GATE;
			}
			if (inputs[INPUT_CLOCK].getVoltage() >= 0.7f) {
				gate |= tides::CONTROL_CLOCK;
			}
			if (!(lastGate & tides::CONTROL_CLOCK) && (gate & tides::CONTROL_CLOCK)) {
				gate |= tides::CONTROL_CLOCK_RISING;
			}
			if (!(lastGate & tides::CONTROL_GATE) && (gate & tides::CONTROL_GATE)) {
				gate |= tides::CONTROL_GATE_RISING;
			}
			if ((lastGate & tides::CONTROL_GATE) && !(gate & tides::CONTROL_GATE)) {
				gate |= tides::CONTROL_GATE_FALLING;
			}
			lastGate = gate;

			const tides::GeneratorSample& sample = generator.Process(gate);
			uint32_t uni = sample.unipolar;
			int32_t bi = sample.bipolar;

			uni = uni * level >> 16;
			bi = -bi * level >> 16;
			float unipolarFlag = static_cast<float>(uni) / 65535;
			float bipolarFlag = static_cast<float>(bi) / 32768;

			outputs[OUTPUT_HIGH].setVoltage(sample.flags & tides::FLAG_END_OF_ATTACK ? 5.f : 0.f);
			outputs[OUTPUT_LOW].setVoltage(sample.flags & tides::FLAG_END_OF_RELEASE ? 5.f : 0.f);
			outputs[OUTPUT_UNI].setVoltage(unipolarFlag * 8.f);
			outputs[OUTPUT_BI].setVoltage(bipolarFlag * 5.f);

			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				lights[LIGHT_MODE + 0].setBrightnessSmooth(mode == tides::GENERATOR_MODE_AD ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				lights[LIGHT_MODE + 1].setBrightnessSmooth(mode == tides::GENERATOR_MODE_AR ?
					kSanguineButtonLightValue : 0.f, sampleTime);

				lights[LIGHT_RANGE + 0].setBrightnessSmooth(range == tides::GENERATOR_RANGE_LOW ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				lights[LIGHT_RANGE + 1].setBrightnessSmooth(range == tides::GENERATOR_RANGE_HIGH ?
					kSanguineButtonLightValue : 0.f, sampleTime);

				if (sample.flags & tides::FLAG_END_OF_ATTACK) {
					unipolarFlag *= -1.f;
				}
				lights[LIGHT_PHASE + 0].setBrightnessSmooth(fmaxf(0.f, unipolarFlag), sampleTime);
				lights[LIGHT_PHASE + 1].setBrightnessSmooth(fmaxf(0.f, -unipolarFlag), sampleTime);

				lights[LIGHT_SYNC + 0].setBrightnessSmooth(bHaveExternalSync && !(getSystemTimeMs() & 128) ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				lights[LIGHT_SYNC + 1].setBrightnessSmooth(bHaveExternalSync ? kSanguineButtonLightValue : 0.f, sampleTime);

				displayModel = aestus::displayModels[params[PARAM_MODEL].getValue()];

				if (!bUseSheepFirmware) {
					paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[0];
				} else {
					paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[1];
				}
			}
		} else {
			if (++frame >= 16) {
				frame = 0;
				plotter.Run();
				outputs[OUTPUT_UNI].setVoltage(rescale(static_cast<float>(plotter.x()), 0.f, UINT16_MAX, -8.f, 8.f));
				outputs[OUTPUT_BI].setVoltage(rescale(static_cast<float>(plotter.y()), 0.f, UINT16_MAX, 8.f, -8.f));
			}

			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				lights[LIGHT_MODE + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_MODE + 1].setBrightnessSmooth(1.f, sampleTime);

				lights[LIGHT_RANGE + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_RANGE + 1].setBrightnessSmooth(1.f, sampleTime);

				lights[LIGHT_PHASE + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_PHASE + 1].setBrightnessSmooth(1.f, sampleTime);

				lights[LIGHT_SYNC + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_SYNC + 1].setBrightnessSmooth(0.f, sampleTime);

				displayModel = aestus::displayModels[aestus::kPeacocksString];
			}
		}
	}

	void onReset() override {
		generator.set_mode(tides::GENERATOR_MODE_LOOPING);
		generator.set_range(tides::GENERATOR_RANGE_MEDIUM);
		params[PARAM_MODEL].setValue(0.f);
	}

	void onRandomize() override {
		generator.set_range(tides::GeneratorRange(random::u32() % 3));
		generator.set_mode(tides::GeneratorMode(random::u32() % 3));
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "mode", static_cast<int>(generator.mode()));
		setJsonInt(rootJ, "range", static_cast<int>(generator.range()));
		setJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);
		setJsonBoolean(rootJ, "wantPeacocksEgg", bWantPeacocks);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "mode", intValue)) {
			generator.set_mode(static_cast<tides::GeneratorMode>(intValue));
		}

		if (getJsonInt(rootJ, "range", intValue)) {
			generator.set_range(static_cast<tides::GeneratorRange>(intValue));
		}

		getJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);
		getJsonBoolean(rootJ, "wantPeacocksEgg", bWantPeacocks);
	}

	void setModel(int modelNum) {
		params[PARAM_MODEL].setValue(modelNum);
	}

	void setMode(int modeNum) {
		generator.set_mode(tides::GeneratorMode(modeNum));
	}

	void setRange(int rangeNum) {
		generator.set_range(tides::GeneratorRange(rangeNum));
	}
};

struct AestusWidget : SanguineModuleWidget {
	explicit AestusWidget(Aestus* module) {
		setModule(module);

		moduleName = "aestus";
		panelSize = SIZE_14;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(11.966, 19.002), module, Aestus::PARAM_MODEL));

		FramebufferWidget* aestusFrameBuffer = new FramebufferWidget();
		addChild(aestusFrameBuffer);

		SanguineTinyNumericDisplay* displayModel = new SanguineTinyNumericDisplay(1, module, 23.42, 17.343);
		displayModel->displayType = DISPLAY_STRING;
		aestusFrameBuffer->addChild(displayModel);
		displayModel->fallbackString = aestus::displayModels[0];

		if (module) {
			displayModel->values.displayText = &module->displayModel;
		}

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(59.142, 19.002), module,
			Aestus::PARAM_SYNC, Aestus::LIGHT_SYNC));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(11.966, 29.086),
			module, Aestus::PARAM_MODE, Aestus::LIGHT_MODE));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(20.888, 37.214), module, Aestus::LIGHT_PHASE));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(35.56, 37.214), module, Aestus::PARAM_FREQUENCY));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(59.142, 37.214), module, Aestus::PARAM_FM));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(11.966, 45.343),
			module, Aestus::PARAM_RANGE, Aestus::LIGHT_RANGE));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(11.966, 62.855), module, Aestus::PARAM_SHAPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(35.56, 62.855), module, Aestus::PARAM_SLOPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(59.142, 62.855), module, Aestus::PARAM_SMOOTHNESS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(11.966, 78.995), module, Aestus::INPUT_SHAPE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(35.56, 78.995), module, Aestus::INPUT_SLOPE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(59.142, 78.995), module, Aestus::INPUT_SMOOTHNESS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.665, 95.56), module, Aestus::INPUT_TRIGGER));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(21.11, 95.56), module, Aestus::INPUT_FREEZE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(35.554, 95.56), module, Aestus::INPUT_PITCH));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(49.998, 95.56), module, Aestus::INPUT_FM));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(64.442, 95.56), module, Aestus::INPUT_LEVEL));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(6.665, 111.643), module, Aestus::INPUT_CLOCK));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(21.11, 111.643), module, Aestus::OUTPUT_HIGH));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(35.554, 111.643), module, Aestus::OUTPUT_LOW));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(49.998, 111.643), module, Aestus::OUTPUT_UNI));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(64.442, 111.643), module, Aestus::OUTPUT_BI));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Aestus* module = dynamic_cast<Aestus*>(this->module);
		if (!module->bWantPeacocks) {
			assert(module);

			menu->addChild(new MenuSeparator);

			menu->addChild(createIndexSubmenuItem("Model", aestus::menuLabels,
				[=]() { return module->params[Aestus::PARAM_MODEL].getValue(); },
				[=](int i) { module->setModel(i); }
			));

			if (!module->bUseSheepFirmware) {
				menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[0], aestusCommon::modeMenuLabels,
					[=]() { return module->generator.mode(); },
					[=](int i) { module->setMode(i); }
				));
			} else {
				menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[1], aestusCommon::sheepMenuLabels,
					[=]() { return module->generator.mode(); },
					[=](int i) { module->setMode(i); }
				));
			}

			menu->addChild(createIndexSubmenuItem("Range", aestusCommon::rangeMenuLabels,
				[=]() { return module->generator.range(); },
				[=](int i) { module->setRange(i); }
			));

			menu->addChild(new MenuSeparator);

			menu->addChild(createSubmenuItem("Compatibility options", "",
				[=](Menu* menu) {
					menu->addChild(createBoolPtrMenuItem("Frequency knob center is C4", "", &module->bUseCalibrationOffset));
				}
			));
		}

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Peacocks easter egg", "", &module->bWantPeacocks));
	}
};


Model* modelAestus = createModel<Aestus, AestusWidget>("Sanguine-Aestus");
