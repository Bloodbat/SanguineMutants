#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "marbles/random/random_generator.h"
#include "marbles/random/random_stream.h"
#include "marbles/random/t_generator.h"
#include "marbles/random/x_y_generator.h"
#include "marbles/note_filter.h"
#include "marbles/scale_recorder.h"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

static const int BLOCK_SIZE = 5;
static const int MAX_T_MODES = 7;
static const int MAX_SCALES = 6;

static const marbles::Scale preset_scales[MAX_SCALES] = {
	// C major
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 },  // C
			{ 0.0833f, 16 },   // C#
			{ 0.1667f, 96 },   // D
			{ 0.2500f, 24 },   // D#
			{ 0.3333f, 128 },  // E
			{ 0.4167f, 64 },   // F
			{ 0.5000f, 8 },    // F#
			{ 0.5833f, 192 },  // G
			{ 0.6667f, 16 },   // G#
			{ 0.7500f, 96 },   // A
			{ 0.8333f, 24 },   // A#
			{ 0.9167f, 128 },  // B
		}
	},
	// C minor
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 },  // C
			{ 0.0833f, 16 },   // C#
			{ 0.1667f, 96 },   // D
			{ 0.2500f, 128 },  // Eb
			{ 0.3333f, 8 },    // E
			{ 0.4167f, 64 },   // F
			{ 0.5000f, 4 },    // F#
			{ 0.5833f, 192 },  // G
			{ 0.6667f, 96 },   // G#
			{ 0.7500f, 16 },   // A
			{ 0.8333f, 128 },  // Bb
			{ 0.9167f, 16 },   // B
		}
	},

	// Pentatonic
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 },  // C
			{ 0.0833f, 4 },    // C#
			{ 0.1667f, 96 },   // D
			{ 0.2500f, 4 },    // Eb
			{ 0.3333f, 4 },    // E
			{ 0.4167f, 140 },  // F
			{ 0.5000f, 4 },    // F#
			{ 0.5833f, 192 },  // G
			{ 0.6667f, 4 },    // G#
			{ 0.7500f, 96 },   // A
			{ 0.8333f, 4 },    // Bb
			{ 0.9167f, 4 },    // B
		}
	},

	// Pelog
	{
		1.0f,
		7,
		{
			{ 0.0000f, 255 },  // C
			{ 0.1275f, 128 },  // Db+
			{ 0.2625f, 32 },  // Eb-
			{ 0.4600f, 8 },    // F#-
			{ 0.5883f, 192 },  // G
			{ 0.7067f, 64 },  // Ab
			{ 0.8817f, 16 },    // Bb+
		}
	},

	// Raag Bhairav That
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 }, // ** Sa
			{ 0.0752f, 128 }, // ** Komal Re
			{ 0.1699f, 4 },   //    Re
			{ 0.2630f, 4 },   //    Komal Ga
			{ 0.3219f, 128 }, // ** Ga
			{ 0.4150f, 64 },  // ** Ma
			{ 0.4918f, 4 },   //    Tivre Ma
			{ 0.5850f, 192 }, // ** Pa
			{ 0.6601f, 64 },  // ** Komal Dha
			{ 0.7549f, 4 },   //    Dha
			{ 0.8479f, 4 },   //    Komal Ni
			{ 0.9069f, 64 },  // ** Ni
		}
	},

	// Raag Shri
	{
		1.0f,
		12,
		{
			{ 0.0000f, 255 }, // ** Sa
			{ 0.0752f, 4 },   //    Komal Re
			{ 0.1699f, 128 }, // ** Re
			{ 0.2630f, 64 },  // ** Komal Ga
			{ 0.3219f, 4 },   //    Ga
			{ 0.4150f, 128 }, // ** Ma
			{ 0.4918f, 4 },   //    Tivre Ma
			{ 0.5850f, 192 }, // ** Pa
			{ 0.6601f, 4 },   //    Komal Dha
			{ 0.7549f, 64 },  // ** Dha
			{ 0.8479f, 128 }, // ** Komal Ni
			{ 0.9069f, 4 },   //    Ni
		}
	},
};

static const marbles::Ratio y_divider_ratios[] = {
	{ 1, 64 },
	{ 1, 48 },
	{ 1, 32 },
	{ 1, 24 },
	{ 1, 16 },
	{ 1, 12 },
	{ 1, 8 },
	{ 1, 6 },
	{ 1, 4 },
	{ 1, 3 },
	{ 1, 2 },
	{ 1, 1 },
};

enum LightModes {
	LIGHT_OFF,
	LIGHT_ON,
	LIGHT_BLINK_SLOW,
	LIGHT_BLINK_FAST
};

static const LightModes tModeLights[MAX_T_MODES][2]{
	{ LIGHT_ON,  LIGHT_OFF }, // T_GENERATOR_MODEL_COMPLEMENTARY_BERNOULLI
	{ LIGHT_ON, LIGHT_ON }, // T_GENERATOR_MODEL_CLUSTERS
	{ LIGHT_OFF, LIGHT_ON }, // T_GENERATOR_MODEL_DRUMS
	{ LIGHT_BLINK_SLOW, LIGHT_OFF }, // T_GENERATOR_MODEL_INDEPENDENT_BERNOULLI
	{ LIGHT_BLINK_SLOW, LIGHT_BLINK_SLOW }, // T_GENERATOR_MODEL_DIVIDER
	{ LIGHT_OFF, LIGHT_BLINK_SLOW }, // T_GENERATOR_MODEL_THREE_STATES
	{ LIGHT_OFF, LIGHT_BLINK_FAST }, // T_GENERATOR_MODEL_MARKOV	
};

static const LightModes scaleLights[MAX_SCALES][2]{
	{ LIGHT_BLINK_SLOW,  LIGHT_OFF }, // Major
	{ LIGHT_BLINK_SLOW, LIGHT_BLINK_SLOW }, // Minor
	{ LIGHT_OFF, LIGHT_BLINK_SLOW }, // Pentatonic
	{ LIGHT_BLINK_FAST, LIGHT_OFF }, // Pelog
	{ LIGHT_BLINK_FAST, LIGHT_BLINK_FAST }, // Raag Bhairav That
	{ LIGHT_OFF, LIGHT_BLINK_FAST } // Raag Shri	
};

static const RGBLightColor paletteMarmoraClockSource[4]{
	{ 0.f, 0.f, 1.f },
	{ 0.f, 1.f, 0.f },
	{ 1.f, 1.f, 0.f },
	{ 1.f, 0.f, 0.f }
};

enum DejaVuLockModes {
	DEJA_VU_LOCK_ON,
	DEJA_VU_LOCK_OFF,
	DEJA_VU_SUPER_LOCK
};

static const std::vector<std::string> marmoraTModeLabels = {
	"Complementary Bernoulli",
	"Clusters",
	"Drums",
	"Independent Bernoulli",
	"Divider",
	"Three states",
	"Markov"
};

static const std::vector<std::string> marmoraTRangeLabels = {
	"1/4x",
	"1x",
	"4x"
};

static const std::vector<std::string> marmoraXModeLabels = {
	"Identical",
	"Bump",
	"Tilt"
};

static const std::vector<std::string> marmoraXRangeLabels = {
	"Narrow (0V - 2V)",
	"Positive (0V - +5V)",
	"Full (-5V - +5V)"
};

static const std::vector<std::string> marmoraScaleLabels = {
	"Major",
	"Minor",
	"Pentatonic",
	"Gamelan (Pelog)",
	"Raag Bhairav That",
	"Raag Shri",
};

static const std::vector<std::string> marmoraInternalClockLabels = {
	"t₁ → X₁, T₂ → X₂, T₃ → X₃",
	"t₁ → X₁, X₂, X₃",
	"t₂ → X₁, X₂, X₃",
	"t₃ → X₁, X₂, X₃",
};

static const std::vector<std::string> marmoraLockLabels = {
	"Unlocked",
	"Locked"
};

struct Marmora : SanguineModule {
	enum ParamIds {
		PARAM_DEJA_VU_T,
		PARAM_DEJA_VU_X,
		PARAM_DEJA_VU,
		PARAM_T_RATE,
		PARAM_X_SPREAD,
		PARAM_T_MODE,
		PARAM_X_MODE,
		PARAM_DEJA_VU_LENGTH,
		PARAM_T_BIAS,
		PARAM_X_BIAS,
		PARAM_T_RANGE,
		PARAM_X_RANGE,
		PARAM_EXTERNAL,
		PARAM_T_JITTER,
		PARAM_X_STEPS,
		PARAM_Y_RATE,
		PARAM_Y_SPREAD,
		PARAM_Y_BIAS,
		PARAM_Y_STEPS,
		PARAM_GATE_BIAS,
		PARAM_GATE_JITTER,
		PARAM_SCALE,
		PARAM_INTERNAL_X_CLOCK_SOURCE,
		PARAM_T_SUPER_LOCK,
		PARAM_X_SUPER_LOCK,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_T_BIAS,
		INPUT_X_BIAS,
		INPUT_T_CLOCK,
		INPUT_T_RATE,
		INPUT_T_JITTER,
		INPUT_DEJA_VU,
		INPUT_X_STEPS,
		INPUT_X_SPREAD,
		INPUT_X_CLOCK,
		INPUT_T_RESET,
		INPUT_X_RESET,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_T1,
		OUTPUT_T2,
		OUTPUT_T3,
		OUTPUT_Y,
		OUTPUT_X1,
		OUTPUT_X2,
		OUTPUT_X3,
		OUTPUTS_COUNT
	};
	enum LightIds {
		LIGHT_DEJA_VU_T,
		LIGHT_DEJA_VU_X,
		ENUMS(LIGHT_T_MODE, 2),
		ENUMS(LIGHT_X_MODE, 2),
		ENUMS(LIGHT_T_RANGE, 2),
		ENUMS(LIGHT_X_RANGE, 2),
		ENUMS(LIGHT_EXTERNAL, 2),
		ENUMS(LIGHT_T1, 2),
		ENUMS(LIGHT_T2, 2),
		ENUMS(LIGHT_T3, 2),
		ENUMS(LIGHT_Y, 2),
		ENUMS(LIGHT_X1, 2),
		ENUMS(LIGHT_X2, 2),
		ENUMS(LIGHT_X3, 2),
		ENUMS(LIGHT_SCALE, 2),
		ENUMS(LIGHT_INTERNAL_CLOCK_SOURCE, 3),
		LIGHTS_COUNT
	};

	struct MarmoraScale {
		bool dirty;
		marbles::Scale scale;
	};

	marbles::RandomGenerator randomGenerator;
	marbles::RandomStream randomStream;
	marbles::TGenerator tGenerator;
	marbles::XYGenerator xyGenerator;
	marbles::NoteFilter noteFilter;
	marbles::ScaleRecorder scaleRecorder;

	bool bDejaVuTEnabled;
	bool bDejaVuXEnabled;
	bool bXClockSourceExternal = false;
	bool bTReset;
	bool bXReset;
	bool bModuleAdded = false;

	bool bScaleEditMode = false;
	bool bNoteGate = false;
	bool bLastGate = false;

	DejaVuLockModes dejaVuLockModeT = DEJA_VU_LOCK_ON;
	DejaVuLockModes dejaVuLockModeX = DEJA_VU_LOCK_ON;

	int xScale = 0;
	int yDividerIndex;
	int xClockSourceInternal;
	const int kLightDivider = 64;

	dsp::ClockDivider lightsDivider;
	dsp::SchmittTrigger stTReset;
	dsp::SchmittTrigger stXReset;

	// Buffers
	stmlib::GateFlags tClocks[BLOCK_SIZE] = {};
	stmlib::GateFlags lastTClock = 0;
	stmlib::GateFlags xyClocks[BLOCK_SIZE] = {};
	stmlib::GateFlags lastXYClock = 0;
	float rampMaster[BLOCK_SIZE] = {};
	float rampExternal[BLOCK_SIZE] = {};
	float rampSlave[2][BLOCK_SIZE] = {};
	bool gates[BLOCK_SIZE * 2] = {};
	float voltages[BLOCK_SIZE * 4] = {};
	int blockIndex = 0;

	// Storage
	MarmoraScale marmoraScales[MAX_SCALES];

	Marmora() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configButton(PARAM_DEJA_VU_T, "T deja vu");
		configButton(PARAM_DEJA_VU_X, "X deja vu");
		configParam(PARAM_DEJA_VU, 0.0, 1.0, 0.5, "Deja vu probability", "%", 0.f, 100.f);
		configParam(PARAM_T_RATE, -1.0, 1.0, 0.0, "Clock rate");
		configParam(PARAM_X_SPREAD, 0.0, 1.0, 0.5, "Probability distribution", "%", 0.f, 100.f);
		configSwitch(PARAM_T_MODE, 0.f, 6.f, 0.f, "T mode", marmoraTModeLabels);
		configSwitch(PARAM_X_MODE, 0.f, 2.f, 0.f, "X mode", marmoraXModeLabels);
		configParam(PARAM_DEJA_VU_LENGTH, 0.0, 1.0, 1.0, "Loop length", "", 0.f, 100.f);
		configParam(PARAM_T_BIAS, 0.0, 1.0, 0.5, "Gate bias", "%", 0.f, 100.f);
		configParam(PARAM_X_BIAS, 0.0, 1.0, 0.5, "Distribution bias", "%", 0.f, 100.f);
		configSwitch(PARAM_T_RANGE, 0.f, 2.f, 0.f, "Clock range mode", marmoraTRangeLabels);
		configSwitch(PARAM_X_RANGE, 0.f, 2.f, 0.f, "Output voltage range", marmoraXRangeLabels);
		configButton(PARAM_EXTERNAL, "External processing mode");
		configParam(PARAM_T_JITTER, 0.0, 1.0, 0.0, "Randomness amount", "%", 0.f, 100.f);
		configParam(PARAM_X_STEPS, 0.f, 1.f, 0.5f, "Smoothness", "%", 0.f, 100.f);
		configSwitch(PARAM_SCALE, 0.f, 5.f, 0.f, "Scale", marmoraScaleLabels);
		configSwitch(PARAM_INTERNAL_X_CLOCK_SOURCE, 0.f, 3.f, 0.f, "Internal X clock source", marmoraInternalClockLabels);
		configSwitch(PARAM_T_SUPER_LOCK, 0.f, 1.f, 0.f, "t Super lock", marmoraLockLabels);
		configSwitch(PARAM_X_SUPER_LOCK, 0.f, 1.f, 0.f, "X Super lock", marmoraLockLabels);

		configInput(INPUT_T_BIAS, "T bias");
		configInput(INPUT_X_BIAS, "X bias");
		configInput(INPUT_T_CLOCK, "T clock");
		configInput(INPUT_T_RATE, "T rate");
		configInput(INPUT_T_JITTER, "T jitter");
		configInput(INPUT_DEJA_VU, "Deja vu");
		configInput(INPUT_X_STEPS, "X steps");
		configInput(INPUT_X_SPREAD, "X spread");
		configInput(INPUT_X_CLOCK, "X clock");
		configInput(INPUT_T_RESET, "T reset");
		configInput(INPUT_X_RESET, "X reset");

		configOutput(OUTPUT_T1, "T₁");
		configOutput(OUTPUT_T2, "T₂");
		configOutput(OUTPUT_T3, "T₃");
		configOutput(OUTPUT_Y, "Y");
		configOutput(OUTPUT_X1, "X₁");
		configOutput(OUTPUT_X2, "X₂");
		configOutput(OUTPUT_X3, "X₃");

		configParam(PARAM_Y_RATE, 0.0, 1.0, 4.5 / LENGTHOF(y_divider_ratios), "Clock divide");
		configParam(PARAM_Y_SPREAD, 0.0, 1.0, 0.5, "Probability distribution", "%", 0.f, 100.f);
		configParam(PARAM_Y_BIAS, 0.0, 1.0, 0.5, "Voltage offset", "%", 0.f, 100.f);
		configParam(PARAM_Y_STEPS, 0.0, 1.0, 0.0, "Smoothness", "%", 0.f, 100.f);

		configParam(PARAM_GATE_BIAS, 0.0, 1.0, 0.5, "Gate length", "%", 0.f, 100.f);
		configParam(PARAM_GATE_JITTER, 0.0, 1.0, 0.0, "Gate length randomness", "%", 0.f, 100.f);

		lightsDivider.setDivision(kLightDivider);

		memset(&randomGenerator, 0, sizeof(marbles::RandomGenerator));
		memset(&randomStream, 0, sizeof(marbles::RandomStream));
		memset(&noteFilter, 0, sizeof(marbles::NoteFilter));

		randomGenerator.Init(1);
		randomStream.Init(&randomGenerator);
		noteFilter.Init();
		scaleRecorder.Init();

		for (int i = 0; i < MAX_SCALES; i++) {
			marmoraScales[i].dirty = false;
			marmoraScales[i].scale.Init();
			copyScale(preset_scales[i], marmoraScales[i].scale);
		}

		onSampleRateChange();
		onReset();
	}

	void process(const ProcessArgs& args) override {
		// TODO!! Add reseeding! (From the manual).

		// Clocks
		bool tGate = inputs[INPUT_T_CLOCK].getVoltage() >= 1.7f;
		lastTClock = stmlib::ExtractGateFlags(lastTClock, tGate);
		tClocks[blockIndex] = lastTClock;

		bool xGate = (inputs[INPUT_X_CLOCK].getVoltage() >= 1.7f);
		lastXYClock = stmlib::ExtractGateFlags(lastXYClock, xGate);
		xyClocks[blockIndex] = lastXYClock;

		// Lock modes
		dejaVuLockModeT = params[PARAM_T_SUPER_LOCK].getValue() ? DEJA_VU_SUPER_LOCK : DEJA_VU_LOCK_OFF;
		dejaVuLockModeX = params[PARAM_X_SUPER_LOCK].getValue() ? DEJA_VU_SUPER_LOCK : DEJA_VU_LOCK_OFF;

		const float dejaVuDistance = fabsf(params[PARAM_DEJA_VU].getValue() - 0.5f);

		if (dejaVuLockModeT != DEJA_VU_SUPER_LOCK && dejaVuDistance < 0.02f) {
			dejaVuLockModeT = DEJA_VU_LOCK_ON;
		}

		if (dejaVuLockModeX != DEJA_VU_SUPER_LOCK && dejaVuDistance < 0.02f) {
			dejaVuLockModeX = DEJA_VU_LOCK_ON;
		}

		bDejaVuTEnabled = params[PARAM_DEJA_VU_T].getValue();
		bDejaVuXEnabled = params[PARAM_DEJA_VU_X].getValue();

		xScale = params[PARAM_SCALE].getValue();
		xClockSourceInternal = params[PARAM_INTERNAL_X_CLOCK_SOURCE].getValue();
		bXClockSourceExternal = inputs[INPUT_X_CLOCK].isConnected();

		bTReset = stTReset.process(inputs[INPUT_T_RESET].getVoltage());
		bXReset = stXReset.process(inputs[INPUT_X_RESET].getVoltage());

		if (!bXClockSourceExternal) {
			bXReset |= bTReset;
		}

		// Process block
		if (++blockIndex >= BLOCK_SIZE) {
			blockIndex = 0;
			stepBlock();
		}

		// Outputs
		outputs[OUTPUT_T1].setVoltage(gates[blockIndex * 2 + 0] ? 10.f : 0.f);
		outputs[OUTPUT_T2].setVoltage((rampMaster[blockIndex] < 0.5f) ? 10.f : 0.f);
		outputs[OUTPUT_T3].setVoltage(gates[blockIndex * 2 + 1] ? 10.f : 0.f);

		outputs[OUTPUT_X1].setVoltage(voltages[blockIndex * 4 + 0]);
		outputs[OUTPUT_X2].setVoltage(voltages[blockIndex * 4 + 1]);
		outputs[OUTPUT_X3].setVoltage(voltages[blockIndex * 4 + 2]);
		outputs[OUTPUT_Y].setVoltage(voltages[blockIndex * 4 + 3]);

		// Lights
		if (lightsDivider.process()) {
			const float sampleTime = kLightDivider * args.sampleTime;

			if (bDejaVuTEnabled || dejaVuLockModeT == DEJA_VU_SUPER_LOCK) {
				drawDejaVuLight(LIGHT_DEJA_VU_T, dejaVuLockModeT, sampleTime);
			}
			else {
				lights[LIGHT_DEJA_VU_T].setBrightnessSmooth(0.f, sampleTime);
			}

			if (bDejaVuXEnabled || dejaVuLockModeX == DEJA_VU_SUPER_LOCK) {
				drawDejaVuLight(LIGHT_DEJA_VU_X, dejaVuLockModeX, sampleTime);
			}
			else {
				lights[LIGHT_DEJA_VU_X].setBrightnessSmooth(0.f, sampleTime);
			}

			int tMode = params[PARAM_T_MODE].getValue();
			drawLight(LIGHT_T_MODE + 0, tModeLights[tMode][0], sampleTime);
			drawLight(LIGHT_T_MODE + 1, tModeLights[tMode][1], sampleTime);

			int xMode = int(params[PARAM_X_MODE].getValue()) % 3;
			lights[LIGHT_X_MODE + 0].setBrightness(xMode == 0 || xMode == 1);
			lights[LIGHT_X_MODE + 1].setBrightness(xMode == 1 || xMode == 2);

			int tRange = int(params[PARAM_T_RANGE].getValue()) % 3;
			lights[LIGHT_T_RANGE + 0].setBrightness(tRange == 0 || tRange == 1);
			lights[LIGHT_T_RANGE + 1].setBrightness(tRange == 1 || tRange == 2);

			int xRange = int(params[PARAM_X_RANGE].getValue()) % 3;
			lights[LIGHT_X_RANGE + 0].setBrightness(xRange == 0 || xRange == 1);
			lights[LIGHT_X_RANGE + 1].setBrightness(xRange == 1 || xRange == 2);

			if (!bScaleEditMode) {
				lights[LIGHT_EXTERNAL + 0].setBrightnessSmooth(params[PARAM_EXTERNAL].getValue(), sampleTime);
				lights[LIGHT_EXTERNAL + 1].setBrightnessSmooth(params[PARAM_EXTERNAL].getValue(), sampleTime);
			}
			else {
				lights[LIGHT_EXTERNAL + 0].setBrightnessSmooth(1.f, sampleTime);
				lights[LIGHT_EXTERNAL + 1].setBrightnessSmooth(0.f, sampleTime);
			}

			drawLight(LIGHT_SCALE + 0, scaleLights[xScale][0], sampleTime);
			drawLight(LIGHT_SCALE + 1, scaleLights[xScale][1], sampleTime);

			lights[LIGHT_T1 + 0].setBrightnessSmooth(gates[blockIndex * 2 + 0], sampleTime);
			lights[LIGHT_T1 + 1].setBrightnessSmooth(-gates[blockIndex * 2 + 0], sampleTime);

			lights[LIGHT_T2 + 0].setBrightnessSmooth(rampMaster[blockIndex] < 0.5f, sampleTime);
			lights[LIGHT_T2 + 1].setBrightnessSmooth(-(rampMaster[blockIndex] < 0.5f), sampleTime);

			lights[LIGHT_T3 + 0].setBrightnessSmooth(gates[blockIndex * 2 + 1], sampleTime);
			lights[LIGHT_T3 + 1].setBrightnessSmooth(-gates[blockIndex * 2 + 1], sampleTime);

			lights[LIGHT_X1 + 0].setBrightnessSmooth(math::rescale(voltages[blockIndex * 4 + 0], 0.f, 5.f, 0.f, 1.f), sampleTime);
			lights[LIGHT_X1 + 1].setBrightnessSmooth(math::rescale(-voltages[blockIndex * 4 + 0], 0.f, 5.f, 0.f, 1.f), sampleTime);

			lights[LIGHT_X2 + 0].setBrightnessSmooth(math::rescale(voltages[blockIndex * 4 + 1], 0.f, 5.f, 0.f, 1.f), sampleTime);
			lights[LIGHT_X2 + 1].setBrightnessSmooth(math::rescale(-voltages[blockIndex * 4 + 1], 0.f, 5.f, 0.f, 1.f), sampleTime);

			lights[LIGHT_X3 + 0].setBrightnessSmooth(math::rescale(voltages[blockIndex * 4 + 2], 0.f, 5.f, 0.f, 1.f), sampleTime);
			lights[LIGHT_X3 + 1].setBrightnessSmooth(math::rescale(-voltages[blockIndex * 4 + 2], 0.f, 5.f, 0.f, 1.f), sampleTime);

			lights[LIGHT_Y + 0].setBrightnessSmooth(math::rescale(voltages[blockIndex * 4 + 3], 0.f, 5.f, 0.f, 1.f), sampleTime);
			lights[LIGHT_Y + 1].setBrightnessSmooth(math::rescale(-voltages[blockIndex * 4 + 3], 0.f, 5.f, 0.f, 1.f), sampleTime);

			if (!bXClockSourceExternal) {
				lights[LIGHT_INTERNAL_CLOCK_SOURCE + 0].setBrightnessSmooth(paletteMarmoraClockSource[xClockSourceInternal].red, sampleTime);
				lights[LIGHT_INTERNAL_CLOCK_SOURCE + 1].setBrightnessSmooth(paletteMarmoraClockSource[xClockSourceInternal].green, sampleTime);
				lights[LIGHT_INTERNAL_CLOCK_SOURCE + 2].setBrightnessSmooth(paletteMarmoraClockSource[xClockSourceInternal].blue, sampleTime);
			}
			else {
				lights[LIGHT_INTERNAL_CLOCK_SOURCE + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_INTERNAL_CLOCK_SOURCE + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_INTERNAL_CLOCK_SOURCE + 2].setBrightnessSmooth(0.f, sampleTime);
			}
		}
	}

	void drawLight(const int light, const LightModes lightMode, const float sampleTime) {
		switch (lightMode)
		{
		case LIGHT_OFF:
		{
			lights[light].setBrightnessSmooth(0.f, sampleTime);
			break;
		}
		case LIGHT_ON: {
			lights[light].setBrightnessSmooth(1.f, sampleTime);
			break;
		}
		case LIGHT_BLINK_SLOW: {
			lights[light].setBrightnessSmooth((getSystemTimeMs() & 255) > 128 ? 1.f : 0.f, sampleTime);
			break;
		}
		case LIGHT_BLINK_FAST: {
			lights[light].setBrightnessSmooth((getSystemTimeMs() & 127) > 64 ? 1.f : 0.f, sampleTime);
			break;
		}
		}
	}

	void drawDejaVuLight(const int light, DejaVuLockModes lockMode, const float sampleTime) {
		switch (lockMode)
		{
		case DEJA_VU_LOCK_ON: {
			int slowTriangle = (getSystemTimeMs() & 1023) >> 5;
			slowTriangle = slowTriangle >= 16 ? 31 - slowTriangle : slowTriangle;
			int pulseWidth = getSystemTimeMs() & 15;
			lights[light].setBrightnessSmooth(slowTriangle >= pulseWidth ? 1.f : 0.f, sampleTime);
			break;
		}
		case DEJA_VU_LOCK_OFF: {
			lights[light].setBrightnessSmooth(1.f, sampleTime);
			break;
		}
		case DEJA_VU_SUPER_LOCK: {
			int fastTriangle = (getSystemTimeMs() & 511) >> 4;
			fastTriangle = fastTriangle >= 16 ? 31 - fastTriangle : fastTriangle;
			int pulseWidth = getSystemTimeMs() & 15;
			lights[light].setBrightnessSmooth(fastTriangle >= pulseWidth ? 1.f : 0.f, sampleTime);
			break;
		}
		}
	}

	void stepBlock() {
		// Ramps
		marbles::Ramps ramps;
		ramps.master = rampMaster;
		ramps.external = rampExternal;
		ramps.slave[0] = rampSlave[0];
		ramps.slave[1] = rampSlave[1];

		float dejaVu = clamp(params[PARAM_DEJA_VU].getValue() + inputs[INPUT_DEJA_VU].getVoltage() / 5.f, 0.f, 1.f);
		static const int loop_length[] = {
			1, 1, 1, 2, 2,
			2, 2, 2, 3, 3,
			3, 3, 4, 4, 4,
			4, 4, 5, 5, 6,
			6, 6, 7, 7, 8,
			8, 8, 10, 10, 12,
			12, 12, 14, 14, 16,
			16
		};
		float dejaVuLengthIndex = params[PARAM_DEJA_VU_LENGTH].getValue() * (LENGTHOF(loop_length) - 1);
		int dejaVuLength = loop_length[(int)roundf(dejaVuLengthIndex)];

		// Setup TGenerator
		bool tExternalClock = inputs[INPUT_T_CLOCK].isConnected();

		tGenerator.set_model(marbles::TGeneratorModel(params[PARAM_T_MODE].getValue()));
		tGenerator.set_range(marbles::TGeneratorRange(params[PARAM_T_RANGE].getValue()));
		float tRate = 60.f * (params[PARAM_T_RATE].getValue() + inputs[INPUT_T_RATE].getVoltage() / 5.f);
		tGenerator.set_rate(tRate);
		float tBias = clamp(params[PARAM_T_BIAS].getValue() + inputs[INPUT_T_BIAS].getVoltage() / 5.f, 0.f, 1.f);
		tGenerator.set_bias(tBias);
		float tJitter = clamp(params[PARAM_T_JITTER].getValue() + inputs[INPUT_T_JITTER].getVoltage() / 5.f, 0.f, 1.f);
		tGenerator.set_jitter(tJitter);
		if (dejaVuLockModeT != DEJA_VU_SUPER_LOCK) {
			tGenerator.set_deja_vu(bDejaVuTEnabled ? dejaVu : 0.f);
		}
		else {
			tGenerator.set_deja_vu(0.5f);
		}
		tGenerator.set_length(dejaVuLength);
		tGenerator.set_pulse_width_mean(params[PARAM_GATE_BIAS].getValue());
		tGenerator.set_pulse_width_std(params[PARAM_GATE_JITTER].getValue());

		tGenerator.Process(tExternalClock, &bTReset, tClocks, ramps, gates, BLOCK_SIZE);

		// Set up XYGenerator
		marbles::ClockSource xClockSource = marbles::ClockSource(xClockSourceInternal);
		if (bXClockSourceExternal) {
			xClockSource = marbles::CLOCK_SOURCE_EXTERNAL;
		}

		if (!bScaleEditMode) {
			marbles::GroupSettings x;
			x.control_mode = marbles::ControlMode(params[PARAM_X_MODE].getValue());
			x.voltage_range = marbles::VoltageRange(params[PARAM_X_RANGE].getValue());
			// TODO Fix the scaling
			//  I think the double multiplication by 0.5f (both in the next line and when assigning "u" might be wrong: custom scales seem to behave nicely when NOT doing that...) -Bat
			float noteCV = 0.5f * (params[PARAM_X_SPREAD].getValue() + inputs[INPUT_X_SPREAD].getVoltage() / 5.f);
			// TODO WTF is u? (A leftover from marbles.cc -Bat Ed.)
			float u = noteFilter.Process(0.5f * (noteCV + 1.f));
			x.register_mode = params[PARAM_EXTERNAL].getValue();
			x.register_value = u;

			float xSpread = clamp(params[PARAM_X_SPREAD].getValue() + inputs[INPUT_X_SPREAD].getVoltage() / 5.f, 0.f, 1.f);
			x.spread = xSpread;
			float xBias = clamp(params[PARAM_X_BIAS].getValue() + inputs[INPUT_X_BIAS].getVoltage() / 5.f, 0.f, 1.f);
			x.bias = xBias;
			float xSteps = clamp(params[PARAM_X_STEPS].getValue() + inputs[INPUT_X_STEPS].getVoltage() / 5.f, 0.f, 1.f);
			x.steps = xSteps;
			if (dejaVuLockModeX != DEJA_VU_SUPER_LOCK) {
				x.deja_vu = bDejaVuXEnabled ? dejaVu : 0.f;
			}
			else {
				x.deja_vu = 0.5f;
			}
			x.length = dejaVuLength;
			x.ratio.p = 1;
			x.ratio.q = 1;
			x.scale_index = xScale;

			marbles::GroupSettings y;
			y.control_mode = marbles::CONTROL_MODE_IDENTICAL;
			y.voltage_range = marbles::VOLTAGE_RANGE_FULL;
			y.register_mode = false;
			y.register_value = 0.0f;
			y.spread = params[PARAM_Y_SPREAD].getValue();
			y.bias = params[PARAM_Y_BIAS].getValue();
			y.steps = params[PARAM_Y_STEPS].getValue();
			y.deja_vu = 0.0f;
			y.length = 1;

			unsigned int index = params[PARAM_Y_RATE].getValue() * LENGTHOF(y_divider_ratios);
			if (index < LENGTHOF(y_divider_ratios))
				yDividerIndex = index;
			y.ratio = y_divider_ratios[yDividerIndex];
			y.scale_index = xScale;

			xyGenerator.Process(xClockSource, x, y, &bXReset, xyClocks, ramps, voltages, BLOCK_SIZE);
		}
		else {
			/* Was
			float noteCV = 0.5f * (params[PARAM_X_SPREAD].getValue() + inputs[INPUT_X_SPREAD].getVoltage() / 5.f);
			*/
			float noteCV = (inputs[INPUT_X_SPREAD].getVoltage() / 5.f);
			float u = noteFilter.Process(0.5f * (noteCV + 1.f));
			float voltage = (u - 0.5f) * 10.0f;
			if (inputs[INPUT_X_CLOCK].getVoltage() >= 0.5f) {
				if (!bLastGate) {
					scaleRecorder.NewNote(voltage);
					bLastGate = true;
				}
				else {
					scaleRecorder.UpdateVoltage(voltage);
				}
			}
			else {
				if (bLastGate) {
					scaleRecorder.AcceptNote();
					bLastGate = false;
				}
			}
		}
	}

	void onReset() override {
		yDividerIndex = 4;
	}

	void onSampleRateChange() override {
		float sampleRate = APP->engine->getSampleRate();
		memset(&tGenerator, 0, sizeof(marbles::TGenerator));
		memset(&xyGenerator, 0, sizeof(marbles::XYGenerator));

		tGenerator.Init(&randomStream, sampleRate);
		xyGenerator.Init(&randomStream, sampleRate);

		// Set scales
		if (!bModuleAdded) {
			for (int i = 0; i < MAX_SCALES; i++) {
				xyGenerator.LoadScale(i, preset_scales[i]);
				copyScale(preset_scales[i], marmoraScales[i].scale);
				marmoraScales[i].dirty = false;
			}
			bModuleAdded = true;
		}
		else {
			for (int i = 0; i < MAX_SCALES; i++) {
				xyGenerator.LoadScale(i, marmoraScales[i].scale);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "y_divider_index", json_integer(yDividerIndex));

		for (int scale = 0; scale < MAX_SCALES; scale++) {
			if (marmoraScales[scale].dirty) {
				json_t* scaleDataVoltagesJ = json_array();
				json_t* scaleDataWeightsJ = json_array();

				std::string scaleHeader = string::f("userScale%d", scale);
				std::string scaleBaseInterval = scaleHeader + "Interval";
				std::string scaleDegrees = scaleHeader + "Degrees";
				std::string scaleDataVoltages = scaleHeader + "DataVoltages";
				std::string scaleDataWeights = scaleHeader + "DataWeights";
				json_object_set_new(rootJ, scaleBaseInterval.c_str(), json_real(marmoraScales[scale].scale.base_interval));
				json_object_set_new(rootJ, scaleDegrees.c_str(), json_integer(marmoraScales[scale].scale.num_degrees));
				for (int degree = 0; degree < marmoraScales[scale].scale.num_degrees; degree++) {
					json_array_insert_new(scaleDataVoltagesJ, degree, json_real(marmoraScales[scale].scale.degree[degree].voltage));
					json_array_insert_new(scaleDataWeightsJ, degree, json_integer(marmoraScales[scale].scale.degree[degree].weight));
				}
				json_object_set_new(rootJ, scaleDataVoltages.c_str(), scaleDataVoltagesJ);
				json_object_set_new(rootJ, scaleDataWeights.c_str(), scaleDataWeightsJ);
			}
		}

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* y_divider_indexJ = json_object_get(rootJ, "y_divider_index");
		if (y_divider_indexJ)
			yDividerIndex = json_integer_value(y_divider_indexJ);

		int dirtyScalesCount = 0;

		for (int scale = 0; scale < MAX_SCALES; scale++) {
			bool scaleOk = true;

			std::string scaleHeader = string::f("userScale%d", scale);
			std::string scaleBaseInterval = scaleHeader + "Interval";

			json_t* scaleBaseIntervalJ = json_object_get(rootJ, scaleBaseInterval.c_str());
			if (scaleBaseIntervalJ) {
				std::string scaleDegrees = scaleHeader + "Degrees";
				std::string scaleDataVoltages = scaleHeader + "DataVoltages";
				std::string scaleDataWeights = scaleHeader + "DataWeights";
				marbles::Scale customScale;

				customScale.Init();

				customScale.base_interval = json_number_value(scaleBaseIntervalJ);

				json_t* scaleDegreesJ = json_object_get(rootJ, scaleDegrees.c_str());
				if (scaleDegreesJ) {
					customScale.num_degrees = json_integer_value(scaleDegreesJ);

					json_t* scaleDataVoltagesJ = json_object_get(rootJ, scaleDataVoltages.c_str());
					json_t* scaleDataWeightsJ = json_object_get(rootJ, scaleDataWeights.c_str());

					if (scaleDataVoltagesJ && scaleDataWeightsJ) {
						for (int degree = 0; degree < customScale.num_degrees; degree++) {
							json_t* voltageJ = json_array_get(scaleDataVoltagesJ, degree);
							json_t* weightJ = json_array_get(scaleDataWeightsJ, degree);
							if (voltageJ && weightJ) {
								customScale.degree[degree].voltage = json_number_value(voltageJ);
								customScale.degree[degree].weight = json_integer_value(weightJ);
							}
							else {
								scaleOk = false;
							}
						}
					}
					else {
						scaleOk = false;
					}
				}
				else {
					scaleOk = false;
				}
				if (scaleOk) {
					copyScale(customScale, marmoraScales[scale].scale);
					marmoraScales[scale].dirty = true;
					dirtyScalesCount++;
				}
			} // if (scaleBaseIntervalJ) {
		} // for
		if (dirtyScalesCount > 0) {
			for (int scale = 0; scale < MAX_SCALES; scale++) {
				if (marmoraScales[scale].dirty) {
					xyGenerator.LoadScale(scale, marmoraScales[scale].scale);
				}
			}
		}
	}

	void toggleScaleEdit() {
		if (bScaleEditMode) {
			marbles::Scale customScale;
			bool success = scaleRecorder.ExtractScale(&customScale);
			if (success) {
				int currentScale = params[PARAM_SCALE].getValue();
				marmoraScales[currentScale].scale.Init();
				copyScale(customScale, marmoraScales[currentScale].scale);
				xyGenerator.LoadScale(currentScale, marmoraScales[currentScale].scale);
				marmoraScales[currentScale].dirty = true;
			}
		}
		else {
			scaleRecorder.Clear();
			bLastGate = false;
		}
		bScaleEditMode = !bScaleEditMode;
	}

	void resetScale() {
		int currentScale = params[PARAM_SCALE].getValue();

		copyScale(preset_scales[currentScale], marmoraScales[currentScale].scale);
		marmoraScales[currentScale].dirty = false;
		xyGenerator.LoadScale(currentScale, marmoraScales[currentScale].scale);
	}

	void copyScale(const marbles::Scale source, marbles::Scale& destination) {
		destination.base_interval = source.base_interval;
		destination.num_degrees = source.num_degrees;

		for (int degree = 0; degree < source.num_degrees; degree++) {
			destination.degree[degree].voltage = source.degree[degree].voltage;
			destination.degree[degree].weight = source.degree[degree].weight;
		}
	}
};

struct MarmoraWidget : SanguineModuleWidget {
	MarmoraWidget(Marmora* module) {
		setModule(module);

		moduleName = "marmora";
		panelSize = SIZE_28;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		TL1105* buttonTSuperLock = createParamCentered<TL1105>(millimetersToPixelsVec(48.162, 13.907), module, Marmora::PARAM_T_SUPER_LOCK);
		buttonTSuperLock->momentary = false;
		buttonTSuperLock->latch = true;
		addParam(buttonTSuperLock);

		TL1105* buttonXSuperLock = createParamCentered<TL1105>(millimetersToPixelsVec(94.078, 13.907), module, Marmora::PARAM_X_SUPER_LOCK);
		buttonXSuperLock->momentary = false;
		buttonXSuperLock->latch = true;
		addParam(buttonXSuperLock);

		CKD6* buttonDejaVuT = createParamCentered<CKD6>(millimetersToPixelsVec(38.262, 18.084), module, Marmora::PARAM_DEJA_VU_T);
		buttonDejaVuT->latch = true;
		buttonDejaVuT->momentary = false;
		addParam(buttonDejaVuT);
		addChild(createLightCentered<CKD6Light<GreenLight>>(millimetersToPixelsVec(38.262, 18.084), module, Marmora::LIGHT_DEJA_VU_T));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(71.12, 16.696), module, Marmora::PARAM_DEJA_VU));

		CKD6* buttonDejaVuX = createParamCentered<CKD6>(millimetersToPixelsVec(104.0, 18.084), module, Marmora::PARAM_DEJA_VU_X);
		buttonDejaVuX->latch = true;
		buttonDejaVuX->momentary = false;
		addParam(buttonDejaVuX);
		addChild(createLightCentered<CKD6Light<RedLight>>(millimetersToPixelsVec(104.0, 18.084), module, Marmora::LIGHT_DEJA_VU_X));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(10.799, 29.573), module, Marmora::PARAM_T_MODE, Marmora::LIGHT_T_MODE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(25.641, 35.373), module, Marmora::PARAM_T_RANGE, Marmora::LIGHT_T_RANGE));

		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(50.226, 37.59), module, Marmora::PARAM_GATE_BIAS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(71.12, 37.59), module, Marmora::INPUT_DEJA_VU));

		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(71.12, 58.483), module, Marmora::PARAM_GATE_JITTER));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(116.623, 35.373), module, Marmora::PARAM_X_RANGE, Marmora::LIGHT_X_RANGE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(131.477, 29.573), module, Marmora::PARAM_X_MODE, Marmora::LIGHT_X_MODE));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(9.671, 42.781), module, Marmora::INPUT_T_RATE));

		addParam(createParamCentered<Sanguine3PSGreen>(millimetersToPixelsVec(26.663, 57.166), module, Marmora::PARAM_T_RATE));

		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(48.445, 59.072), module, Marmora::PARAM_T_JITTER));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(92.014, 37.59), module, Marmora::PARAM_DEJA_VU_LENGTH));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(93.795, 59.072), module, Marmora::PARAM_X_STEPS));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(115.577, 57.166), module, Marmora::PARAM_X_SPREAD));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(132.561, 42.781), module, Marmora::INPUT_X_SPREAD));

		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(9.671, 70.927), module, Marmora::PARAM_T_BIAS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(39.965, 75.619), module, Marmora::INPUT_T_JITTER));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(24.818, 93.182),
			module, Marmora::PARAM_SCALE, Marmora::LIGHT_SCALE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(132.561, 51.792),
			module, Marmora::PARAM_EXTERNAL, Marmora::LIGHT_EXTERNAL));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(102.275, 75.619), module, Marmora::INPUT_X_STEPS));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(132.562, 70.927), module, Marmora::PARAM_X_BIAS));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(24.818, 79.424), module, Marmora::INPUT_T_CLOCK));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(117.418, 79.424), module, Marmora::INPUT_X_CLOCK));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(9.671, 88.407), module, Marmora::INPUT_T_BIAS));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(46.551, 90.444), module, Marmora::PARAM_Y_RATE));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(95.69, 90.444), module, Marmora::PARAM_Y_STEPS));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(millimetersToPixelsVec(117.418, 93.182),
			module, Marmora::PARAM_INTERNAL_X_CLOCK_SOURCE, Marmora::LIGHT_INTERNAL_CLOCK_SOURCE));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(132.562, 88.407), module, Marmora::INPUT_X_BIAS));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(51.268, 110.22), module, Marmora::PARAM_Y_SPREAD));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(92.291, 110.22), module, Marmora::PARAM_Y_BIAS));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(6.567, 107.415), module, Marmora::LIGHT_T1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(6.567, 114.238), module, Marmora::OUTPUT_T1));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(18.989, 107.415), module, Marmora::LIGHT_T2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(18.989, 114.238), module, Marmora::OUTPUT_T2));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(31.411, 107.415), module, Marmora::LIGHT_T3));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(31.411, 114.238), module, Marmora::OUTPUT_T3));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(71.12, 107.415), module, Marmora::LIGHT_Y));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(71.12, 114.238), module, Marmora::OUTPUT_Y));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(110.862, 107.415), module, Marmora::LIGHT_X1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(110.862, 114.238), module, Marmora::OUTPUT_X1));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(123.277, 107.415), module, Marmora::LIGHT_X2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(123.277, 114.238), module, Marmora::OUTPUT_X2));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(135.691, 107.415), module, Marmora::LIGHT_X3));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(135.691, 114.238), module, Marmora::OUTPUT_X3));

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(55.45, 73.437), module, Marmora::INPUT_T_RESET));
		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(86.79, 73.437), module, Marmora::INPUT_X_RESET));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 61.356, 89.818);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 74.358, 96.751);
		addChild(mutantsLogo);
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Marmora* module = dynamic_cast<Marmora*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("T mode", marmoraTModeLabels,
			[=]() {return module->params[Marmora::PARAM_T_MODE].getValue(); },
			[=](int i) {module->params[Marmora::PARAM_T_MODE].setValue(i); }
		));

		menu->addChild(createIndexSubmenuItem("T range", marmoraTRangeLabels,
			[=]() {return module->params[Marmora::PARAM_T_RANGE].getValue(); },
			[=](int i) {module->params[Marmora::PARAM_T_RANGE].setValue(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("X mode", marmoraXModeLabels,
			[=]() {return module->params[Marmora::PARAM_X_MODE].getValue(); },
			[=](int i) {module->params[Marmora::PARAM_X_MODE].setValue(i); }
		));

		menu->addChild(createIndexSubmenuItem("X range", marmoraXRangeLabels,
			[=]() {return module->params[Marmora::PARAM_X_RANGE].getValue(); },
			[=](int i) {module->params[Marmora::PARAM_X_RANGE].setValue(i); }
		));

		menu->addChild(createIndexSubmenuItem("Internal X clock source", marmoraInternalClockLabels,
			[=]() {return module->params[Marmora::PARAM_INTERNAL_X_CLOCK_SOURCE].getValue(); },
			[=](int i) {module->params[Marmora::PARAM_INTERNAL_X_CLOCK_SOURCE].setValue(i); }
		));

		menu->addChild(createIndexSubmenuItem("Scale", marmoraScaleLabels,
			[=]() {return module->params[Marmora::PARAM_SCALE].getValue(); },
			[=](int i) {module->params[Marmora::PARAM_SCALE].setValue(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createCheckMenuItem("Scale edit mode", "",
			[=]() {return module->bScaleEditMode; },
			[=]() {module->toggleScaleEdit(); }));

		menu->addChild(createMenuItem("Reset current scale", "",
			[=]() {module->resetScale(); }
		));
	}
};

Model* modelMarmora = createModel<Marmora, MarmoraWidget>("Sanguine-Marmora");