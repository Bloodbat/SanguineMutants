#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "tides/generator.h"

static const std::vector<std::string> aestusDisplayModels = {
	"T",
	"S"
};

static const std::vector<std::string> aestusMenuLabels = {
	"Tidal Modulator",
	"Sheep - Wavetable synthesizer"
};

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

	bool bSheep = false;
	tides::Generator generator;
	int frame = 0;
	static const int kLightsFrequency = 16;
	uint8_t lastGate = 0;
	dsp::SchmittTrigger stMode;
	dsp::SchmittTrigger stRange;
	dsp::ClockDivider lightsDivider;
	std::string displayModel = aestusDisplayModels[0];

	Aestus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		configButton(PARAM_MODE, "Output mode");
		configButton(PARAM_RANGE, "Frequency range");
		configParam(PARAM_FREQUENCY, -48.0, 48.0, 0.0, "Main frequency");
		configParam(PARAM_FM, -12.0, 12.0, 0.0, "FM input attenuverter");
		configParam(PARAM_SHAPE, -1.0, 1.0, 0.0, "Shape");
		configParam(PARAM_SLOPE, -1.0, 1.0, 0.0, "Slope");
		configParam(PARAM_SMOOTHNESS, -1.0, 1.0, 0.0, "Smoothness");

		configSwitch(PARAM_MODEL, 0.f, 1.f, 0.f, "Module model", aestusMenuLabels);

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

		configButton(PARAM_SYNC, "PLL mode");

		generator.Init();
		lightsDivider.setDivision(kLightsFrequency);
		onReset();
	}

	void process(const ProcessArgs& args) override {
		bool bLightsTurn = lightsDivider.process();

		tides::GeneratorMode mode = generator.mode();
		if (stMode.process(params[PARAM_MODE].getValue())) {
			mode = tides::GeneratorMode((int(mode) - 1 + 3) % 3);
			generator.set_mode(mode);
		}

		tides::GeneratorRange range = generator.range();
		if (stRange.process(params[PARAM_RANGE].getValue())) {
			range = tides::GeneratorRange((int(range) - 1 + 3) % 3);
			generator.set_range(range);
		}

		bSheep = bool(params[PARAM_MODEL].getValue());

		bool bSync = bool(params[PARAM_SYNC].getValue());

		// Buffer loop
		if (++frame >= 16) {
			frame = 0;

			// Sync			
			// This takes a moment to catch up if sync is on and patches or presets have just been loaded!
			generator.set_sync(bSync);

			// Pitch
			float pitch = params[PARAM_FREQUENCY].getValue();
			pitch += 12.0 * inputs[INPUT_PITCH].getVoltage();
			pitch += params[PARAM_FM].getValue() * inputs[INPUT_FM].getNormalVoltage(0.1) / 5.0;
			pitch += 60.0;
			// Scale to the global sample rate
			pitch += log2f(48000.0 / args.sampleRate) * 12.0;
			generator.set_pitch(int(clamp(pitch * 0x80, float(-0x8000), float(0x7fff))));

			// Shape, slope, smoothness
			int16_t shape = clamp(params[PARAM_SHAPE].getValue() +
				inputs[INPUT_SHAPE].getVoltage() / 5.0f, -1.0f, 1.0f) * 0x7fff;
			int16_t slope = clamp(params[PARAM_SLOPE].getValue() +
				inputs[INPUT_SLOPE].getVoltage() / 5.0f, -1.0f, 1.0f) * 0x7fff;
			int16_t smoothness = clamp(params[PARAM_SMOOTHNESS].getValue() +
				inputs[INPUT_SMOOTHNESS].getVoltage() / 5.0f, -1.0f, 1.0f) * 0x7fff;
			generator.set_shape(shape);
			generator.set_slope(slope);
			generator.set_smoothness(smoothness);

			// Generator
			generator.Process(bSheep);
		}

		// Level
		uint16_t level = clamp(inputs[INPUT_LEVEL].getNormalVoltage(8.0) / 8.0f, 0.0f, 1.0f)
			* 0xffff;
		if (level < 32) {
			level = 0;
		}

		uint8_t gate = 0;
		if (inputs[INPUT_FREEZE].getVoltage() >= 0.7)
			gate |= tides::CONTROL_FREEZE;
		if (inputs[INPUT_TRIGGER].getVoltage() >= 0.7)
			gate |= tides::CONTROL_GATE;
		if (inputs[INPUT_CLOCK].getVoltage() >= 0.7)
			gate |= tides::CONTROL_CLOCK;
		if (!(lastGate & tides::CONTROL_CLOCK) && (gate & tides::CONTROL_CLOCK))
			gate |= tides::CONTROL_CLOCK_RISING;
		if (!(lastGate & tides::CONTROL_GATE) && (gate & tides::CONTROL_GATE))
			gate |= tides::CONTROL_GATE_RISING;
		if ((lastGate & tides::CONTROL_GATE) && !(gate & tides::CONTROL_GATE))
			gate |= tides::CONTROL_GATE_FALLING;
		lastGate = gate;

		const tides::GeneratorSample& sample = generator.Process(gate);
		uint32_t uni = sample.unipolar;
		int32_t bi = sample.bipolar;

		uni = uni * level >> 16;
		bi = -bi * level >> 16;
		float unipolarFlag = float(uni) / 0xffff;
		float bipolarFlag = float(bi) / 0x8000;

		outputs[OUTPUT_HIGH].setVoltage(sample.flags & tides::FLAG_END_OF_ATTACK ? 0.0 : 5.0);
		outputs[OUTPUT_LOW].setVoltage(sample.flags & tides::FLAG_END_OF_RELEASE ? 0.0 : 5.0);
		outputs[OUTPUT_UNI].setVoltage(unipolarFlag * 8.0);
		outputs[OUTPUT_BI].setVoltage(bipolarFlag * 5.0);

		if (bLightsTurn) {
			const float sampleTime = kLightsFrequency * args.sampleTime;

			lights[LIGHT_MODE + 0].setBrightnessSmooth(mode == tides::GENERATOR_MODE_AD ? 1.0 : 0.0, sampleTime);
			lights[LIGHT_MODE + 1].setBrightnessSmooth(mode == tides::GENERATOR_MODE_AR ? 1.0 : 0.0, sampleTime);

			lights[LIGHT_RANGE + 0].setBrightnessSmooth(range == tides::GENERATOR_RANGE_LOW ? 1.0 : 0.0, sampleTime);
			lights[LIGHT_RANGE + 1].setBrightnessSmooth(range == tides::GENERATOR_RANGE_HIGH ? 1.0 : 0.0, sampleTime);

			if (sample.flags & tides::FLAG_END_OF_ATTACK)
				unipolarFlag *= -1.0;
			lights[LIGHT_PHASE + 0].setBrightnessSmooth(fmaxf(0.0, unipolarFlag), sampleTime);
			lights[LIGHT_PHASE + 1].setBrightnessSmooth(fmaxf(0.0, -unipolarFlag), sampleTime);

			lights[LIGHT_SYNC + 0].setBrightnessSmooth(bSync && !(getSystemTimeMs() & 128) ? 1.f : 0.f, sampleTime);
			lights[LIGHT_SYNC + 1].setBrightnessSmooth(bSync ? 1.f : 0.f, sampleTime);

			displayModel = aestusDisplayModels[params[PARAM_MODEL].getValue()];
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
		json_object_set_new(rootJ, "mode", json_integer(int(generator.mode())));
		json_object_set_new(rootJ, "range", json_integer(int(generator.range())));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* modeJ = json_object_get(rootJ, "mode");
		if (modeJ) {
			generator.set_mode(tides::GeneratorMode(json_integer_value(modeJ)));
		}

		json_t* rangeJ = json_object_get(rootJ, "range");
		if (rangeJ) {
			generator.set_range(tides::GeneratorRange(json_integer_value(rangeJ)));
		}
	}

	void setModel(int modelNum) {
		params[PARAM_MODEL].setValue(modelNum);
	}
};

struct AestusWidget : SanguineModuleWidget {
	AestusWidget(Aestus* module) {
		setModule(module);

		moduleName = "aestus";
		panelSize = SIZE_14;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<CKD6>(millimetersToPixelsVec(6.665, 17.143), module, Aestus::PARAM_MODE));
		addChild(createLightCentered<CKD6Light<GreenRedLight>>(millimetersToPixelsVec(6.665, 17.143), module, Aestus::LIGHT_MODE));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(23.075, 18.802), module, Aestus::PARAM_MODEL));

		FramebufferWidget* aestusFrameBuffer = new FramebufferWidget();
		addChild(aestusFrameBuffer);

		SanguineTinyNumericDisplay* displayModel = new SanguineTinyNumericDisplay(1, module, 32.724, 17.143);
		displayModel->displayType = DISPLAY_STRING;
		aestusFrameBuffer->addChild(displayModel);
		displayModel->fallbackString = aestusDisplayModels[0];

		if (module)
			displayModel->values.displayText = &module->displayModel;

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(48.038, 18.802), module,
			Aestus::PARAM_SYNC, Aestus::LIGHT_SYNC));

		addParam(createParamCentered<CKD6>(millimetersToPixelsVec(64.442, 17.143), module, Aestus::PARAM_RANGE));
		addChild(createLightCentered<CKD6Light<GreenRedLight>>(millimetersToPixelsVec(64.442, 17.143), module, Aestus::LIGHT_RANGE));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(23.075, 34.714), module, Aestus::LIGHT_PHASE));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(35.56, 34.714), module, Aestus::PARAM_FREQUENCY));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(59.142, 34.714), module, Aestus::PARAM_FM));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(11.966, 60.355), module, Aestus::PARAM_SHAPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(35.56, 60.355), module, Aestus::PARAM_SLOPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(59.142, 60.355), module, Aestus::PARAM_SMOOTHNESS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(11.966, 76.495), module, Aestus::INPUT_SHAPE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(35.56, 76.495), module, Aestus::INPUT_SLOPE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(59.142, 76.495), module, Aestus::INPUT_SMOOTHNESS));

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
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Mode", aestusMenuLabels,
			[=]() { return module->params[Aestus::PARAM_MODEL].getValue(); },
			[=](int i) { module->setModel(i); }
		));
	}
};


Model* modelAestus = createModel<Aestus, AestusWidget>("Sanguine-Aestus");
