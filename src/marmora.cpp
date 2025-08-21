#include "plugin.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#include <string>

#include "marbles/random/random_generator.h"
#include "marbles/random/random_stream.h"
#include "marbles/random/t_generator.h"
#include "marbles/random/x_y_generator.h"
#include "marbles/note_filter.h"
#include "marbles/scale_recorder.h"

#include "marmora.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

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
		ENUMS(LIGHT_INTERNAL_X_CLOCK_SOURCE, 3),
		LIGHTS_COUNT
	};

	struct MarmoraScale {
		bool bScaleDirty;
		marbles::Scale scale;
	};

	marbles::RandomGenerator randomGenerator;
	marbles::RandomStream randomStream;
	marbles::TGenerator tGenerator;
	marbles::XYGenerator xyGenerator;
	marbles::NoteFilter noteFilter;
	marbles::ScaleRecorder scaleRecorder;
	marbles::ClockSource xClockSourceInternal = marbles::CLOCK_SOURCE_INTERNAL_T1_T2_T3;

	bool bDejaVuTEnabled = false;
	bool bDejaVuXEnabled = false;
	bool bXClockSourceExternal = false;
	bool bWantTReset = false;
	bool bWantXReset = false;
	bool bModuleAdded = false;
	bool bWantMenuTReset = false;
	bool bWantMenuXReset = false;

	bool bScaleEditMode = false;
	bool bLastGate = false;

	bool bGates[marmora::kBlockSize * 2] = {};

	marmora::DejaVuLockModes dejaVuLockModeT = marmora::DEJA_VU_LOCK_ON;
	marmora::DejaVuLockModes dejaVuLockModeX = marmora::DEJA_VU_LOCK_ON;

	int xScale = 0;
	int yDividerIndex = 4;
	int blockIndex = 0;
	uint32_t userSeed = 1;

	static const int kLightDivider = 64;

	dsp::ClockDivider lightsDivider;
	dsp::SchmittTrigger stTReset;
	dsp::SchmittTrigger stXReset;

	// Buffers.
	stmlib::GateFlags tClocks[marmora::kBlockSize] = {};
	stmlib::GateFlags lastTClock = 0;
	stmlib::GateFlags xyClocks[marmora::kBlockSize] = {};
	stmlib::GateFlags lastXYClock = 0;

	float rampMaster[marmora::kBlockSize] = {};
	float rampExternal[marmora::kBlockSize] = {};
	float rampSlave[2][marmora::kBlockSize] = {};
	float voltages[marmora::kBlockSize * 4] = {};
	float newNoteVoltage = 0.f;

	// Storage.
	MarmoraScale marmoraScales[marmora::kMaxScales];

	struct LengthParam : ParamQuantity {
		std::string getDisplayValueString() override {
			float dejaVuLengthIndex = getValue() * (LENGTHOF(marmora::loopLengths) - 1);
			int dejaVuLength = marmora::loopLengths[static_cast<int>(roundf(dejaVuLengthIndex))];
			return string::f("%d", dejaVuLength);
		}

		void setDisplayValue(float 	displayValue) override {
			int inputValue = static_cast<int>(displayValue);
			switch (inputValue)
			{
			case 1:
				displayValue = 0.f;
				break;
			case 2:
				displayValue = 0.158f;
				break;
			case 3:
				displayValue = 0.308f;
				break;
			case 4:
				displayValue = 0.443f;
				break;
			case 5:
				displayValue = 0.5f;
				break;
			case 6:
				displayValue = 0.571f;
				break;
			case 7:
				displayValue = 0.633f;
				break;
			case 8:
				displayValue = 0.705f;
				break;
			case 10:
				displayValue = 0.771f;
				break;
			case 12:
				displayValue = 0.840f;
				break;
			case 14:
				displayValue = 0.912f;
				break;
			default:
				displayValue = 1.f;
				break;
			}

			setImmediateValue(displayValue);
		}
	};

	Marmora() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_DEJA_VU, 0.f, 1.f, 0.5f, "Deja vu probability", "%", 0.f, 100.f);
		configParam<LengthParam>(PARAM_DEJA_VU_LENGTH, 0.f, 1.f, 1.f, "Loop length");
		configInput(INPUT_DEJA_VU, "Deja vu");

		configButton(PARAM_DEJA_VU_T, "t deja vu");
		configSwitch(PARAM_T_SUPER_LOCK, 0.f, 1.f, 0.f, "t super lock", marmora::lockLabels);
		configSwitch(PARAM_T_MODE, 0.f, 6.f, 0.f, "t mode", marmora::tModeLabels);
		configSwitch(PARAM_T_RANGE, 0.f, 2.f, 0.f, "t clock range", marmora::tRangeLabels);
		configParam(PARAM_T_RATE, -1.f, 1.f, 0.f, "t clock rate");
		configParam(PARAM_T_BIAS, 0.f, 1.f, 0.5f, "t gate bias", "%", 0.f, 100.f);
		configParam(PARAM_T_JITTER, 0.f, 1.f, 0.f, "t randomness amount", "%", 0.f, 100.f);
		configParam(PARAM_GATE_BIAS, 0.f, 1.f, 0.5f, "t gate length", "%", 0.f, 100.f);
		configParam(PARAM_GATE_JITTER, 0.f, 1.f, 0.f, "t gate length randomness", "%", 0.f, 100.f);
		configInput(INPUT_T_BIAS, "t bias");
		configInput(INPUT_T_CLOCK, "t external clock");
		configInput(INPUT_T_RATE, "t rate");
		configInput(INPUT_T_JITTER, "t jitter");
		configInput(INPUT_T_RESET, "t reset");
		configOutput(OUTPUT_T1, "t₁");
		configOutput(OUTPUT_T2, "t₂");
		configOutput(OUTPUT_T3, "t₃");

		configButton(PARAM_DEJA_VU_X, "X deja vu");
		configSwitch(PARAM_X_SUPER_LOCK, 0.f, 1.f, 0.f, "X super lock", marmora::lockLabels);
		configParam(PARAM_X_SPREAD, 0.f, 1.f, 0.5f, "X probability distribution", "%", 0.f, 100.f);
		configSwitch(PARAM_X_MODE, 0.f, 2.f, 0.f, "X mode", marmora::xModeLabels);
		configParam(PARAM_X_BIAS, 0.f, 1.f, 0.5f, "X distribution bias", "%", 0.f, 100.f);
		configSwitch(PARAM_X_RANGE, 0.f, 2.f, 0.f, "X output voltage range", marmora::xRangeLabels);
		configButton(PARAM_EXTERNAL, "X external voltage processing mode");
		configParam(PARAM_X_STEPS, 0.f, 1.f, 0.5f, "X smoothness", "%", 0.f, 100.f);
		configSwitch(PARAM_SCALE, 0.f, 5.f, 0.f, "Scale", marmora::scaleLabels);
		configSwitch(PARAM_INTERNAL_X_CLOCK_SOURCE, 0.f, 3.f, 0.f, "Internal X clock source", marmora::internalClockLabels);
		configInput(INPUT_X_BIAS, "X bias");
		configInput(INPUT_X_STEPS, "X steps");
		configInput(INPUT_X_SPREAD, "X spread");
		configInput(INPUT_X_CLOCK, "X external clock");
		configInput(INPUT_X_RESET, "X reset");
		configOutput(OUTPUT_X1, "X₁");
		configOutput(OUTPUT_X2, "X₂");
		configOutput(OUTPUT_X3, "X₃");

		configParam(PARAM_Y_RATE, 0.f, 1.f, 0.375f, "Y clock divider");
		configParam(PARAM_Y_SPREAD, 0.f, 1.f, 0.5f, "Y probability distribution", "%", 0.f, 100.f);
		configParam(PARAM_Y_BIAS, 0.f, 1.f, 0.5f, "Y voltage offset", "%", 0.f, 100.f);
		configParam(PARAM_Y_STEPS, 0.f, 1.f, 0.f, "Y smoothness", "%", 0.f, 100.f);
		configOutput(OUTPUT_Y, "Y");

		lightsDivider.setDivision(kLightDivider);

		randomGenerator.Init(userSeed);
		randomStream.Init(&randomGenerator);
		noteFilter.Init();
		scaleRecorder.Init();

		for (int scale = 0; scale < marmora::kMaxScales; ++scale) {
			marmoraScales[scale].bScaleDirty = false;
			marmoraScales[scale].scale.Init();
			copyScale(marmora::presetScales[scale], marmoraScales[scale].scale);
		}

		init();
	}

	void process(const ProcessArgs& args) override {
		// Clocks.
		bool bTClockGate = inputs[INPUT_T_CLOCK].getVoltage() >= 1.7f;
		lastTClock = stmlib::ExtractGateFlags(lastTClock, bTClockGate);
		tClocks[blockIndex] = lastTClock;

		bool bXClockGate = inputs[INPUT_X_CLOCK].getVoltage() >= 1.7f;
		lastXYClock = stmlib::ExtractGateFlags(lastXYClock, bXClockGate);
		xyClocks[blockIndex] = lastXYClock;

		// Lock modes.
		dejaVuLockModeT = params[PARAM_T_SUPER_LOCK].getValue() ? marmora::DEJA_VU_SUPER_LOCK : marmora::DEJA_VU_LOCK_OFF;
		dejaVuLockModeX = params[PARAM_X_SUPER_LOCK].getValue() ? marmora::DEJA_VU_SUPER_LOCK : marmora::DEJA_VU_LOCK_OFF;

		const float dejaVuDistance = fabsf(params[PARAM_DEJA_VU].getValue() - 0.5f);

		if (dejaVuLockModeT != marmora::DEJA_VU_SUPER_LOCK && dejaVuDistance < 0.02f) {
			dejaVuLockModeT = marmora::DEJA_VU_LOCK_ON;
		}

		if (dejaVuLockModeX != marmora::DEJA_VU_SUPER_LOCK && dejaVuDistance < 0.02f) {
			dejaVuLockModeX = marmora::DEJA_VU_LOCK_ON;
		}

		bDejaVuTEnabled = params[PARAM_DEJA_VU_T].getValue();
		bDejaVuXEnabled = params[PARAM_DEJA_VU_X].getValue();

		xScale = params[PARAM_SCALE].getValue();
		xClockSourceInternal = static_cast<marbles::ClockSource>(params[PARAM_INTERNAL_X_CLOCK_SOURCE].getValue());
		bXClockSourceExternal = inputs[INPUT_X_CLOCK].isConnected();

		bWantTReset = stTReset.process(inputs[INPUT_T_RESET].getVoltage()) || bWantMenuTReset;
		bWantXReset = stXReset.process(inputs[INPUT_X_RESET].getVoltage()) || bWantMenuXReset;

		if (!bXClockSourceExternal) {
			bWantXReset |= bWantTReset;
		}

		bWantMenuTReset = bWantMenuXReset = false;

		// Process block.
		if (++blockIndex >= marmora::kBlockSize) {
			blockIndex = 0;
			stepBlock();
		}

		// Outputs.
		if (!bScaleEditMode) {
			outputs[OUTPUT_T1].setVoltage(bGates[blockIndex * 2] ? 10.f : 0.f);
			outputs[OUTPUT_T2].setVoltage((rampMaster[blockIndex] < 0.5f) ? 10.f : 0.f);
			outputs[OUTPUT_T3].setVoltage(bGates[blockIndex * 2 + 1] ? 10.f : 0.f);

			outputs[OUTPUT_X1].setVoltage(voltages[blockIndex * 4]);
			outputs[OUTPUT_X2].setVoltage(voltages[blockIndex * 4 + 1]);
			outputs[OUTPUT_X3].setVoltage(voltages[blockIndex * 4 + 2]);
		} else {
			outputs[OUTPUT_T1].setVoltage(bLastGate ? 10.f : 0.f);
			outputs[OUTPUT_T2].setVoltage(bLastGate ? 10.f : 0.f);
			outputs[OUTPUT_T3].setVoltage(bLastGate ? 10.f : 0.f);

			outputs[OUTPUT_X1].setVoltage(newNoteVoltage);
			outputs[OUTPUT_X2].setVoltage(newNoteVoltage);
			outputs[OUTPUT_X3].setVoltage(newNoteVoltage);
		}
		outputs[OUTPUT_Y].setVoltage(voltages[blockIndex * 4 + 3]);

		// Lights.
		if (lightsDivider.process()) {
			long long systemTimeMs = getSystemTimeMs();

			const float sampleTime = kLightDivider * args.sampleTime;

			if (bDejaVuTEnabled || dejaVuLockModeT == marmora::DEJA_VU_SUPER_LOCK) {
				drawDejaVuLight(LIGHT_DEJA_VU_T, dejaVuLockModeT, sampleTime, systemTimeMs);
			} else {
				lights[LIGHT_DEJA_VU_T].setBrightnessSmooth(0.f, sampleTime);
			}

			if (bDejaVuXEnabled || dejaVuLockModeX == marmora::DEJA_VU_SUPER_LOCK) {
				drawDejaVuLight(LIGHT_DEJA_VU_X, dejaVuLockModeX, sampleTime, systemTimeMs);
			} else {
				lights[LIGHT_DEJA_VU_X].setBrightnessSmooth(0.f, sampleTime);
			}

			int tMode = params[PARAM_T_MODE].getValue();
			drawLight(LIGHT_T_MODE, marmora::tModeLights[tMode][0], sampleTime, systemTimeMs);
			drawLight(LIGHT_T_MODE + 1, marmora::tModeLights[tMode][1], sampleTime, systemTimeMs);

			int xMode = static_cast<int>(params[PARAM_X_MODE].getValue()) % 3;
			lights[LIGHT_X_MODE].setBrightness(xMode == 0 || xMode == 1 ? kSanguineButtonLightValue : 0.f);
			lights[LIGHT_X_MODE + 1].setBrightness(xMode == 1 || xMode == 2 ? kSanguineButtonLightValue : 0.f);

			int tRange = static_cast<int>(params[PARAM_T_RANGE].getValue()) % 3;
			lights[LIGHT_T_RANGE].setBrightness(tRange == 0 || tRange == 1 ? kSanguineButtonLightValue : 0.f);
			lights[LIGHT_T_RANGE + 1].setBrightness(tRange == 1 || tRange == 2 ? kSanguineButtonLightValue : 0.f);

			int xRange = static_cast<int>(params[PARAM_X_RANGE].getValue()) % 3;
			lights[LIGHT_X_RANGE].setBrightness(xRange == 0 || xRange == 1 ? kSanguineButtonLightValue : 0.f);
			lights[LIGHT_X_RANGE + 1].setBrightness(xRange == 1 || xRange == 2 ? kSanguineButtonLightValue : 0.f);

			drawLight(LIGHT_SCALE, marmora::scaleLights[xScale][0], sampleTime, systemTimeMs);
			drawLight(LIGHT_SCALE + 1, marmora::scaleLights[xScale][1], sampleTime, systemTimeMs);

			if (!bScaleEditMode) {
				float lightExternalBrightness = params[PARAM_EXTERNAL].getValue() ? kSanguineButtonLightValue : 0.f;
				lights[LIGHT_EXTERNAL].setBrightnessSmooth(lightExternalBrightness, sampleTime);
				lights[LIGHT_EXTERNAL + 1].setBrightnessSmooth(lightExternalBrightness, sampleTime);

				// T1 and T3 are booleans: they'll never go negative.
				lights[LIGHT_T1].setBrightnessSmooth(bGates[blockIndex * 2], sampleTime);
				//lights[LIGHT_T1 + 1].setBrightnessSmooth(-bGates[blockIndex * 2 + 0], sampleTime);

				lights[LIGHT_T2].setBrightnessSmooth(rampMaster[blockIndex] < 0.5f, sampleTime);
				//lights[LIGHT_T2 + 1].setBrightnessSmooth(-(rampMaster[blockIndex] < 0.5f), sampleTime);

				lights[LIGHT_T3].setBrightnessSmooth(bGates[blockIndex * 2 + 1], sampleTime);
				//lights[LIGHT_T3 + 1].setBrightnessSmooth(-bGates[blockIndex * 2 + 1], sampleTime);

				float outputVoltage = 0.f;

				outputVoltage = math::rescale(voltages[blockIndex * 4], 0.f, 5.f, 0.f, 1.f);
				lights[LIGHT_X1].setBrightnessSmooth(outputVoltage, sampleTime);
				lights[LIGHT_X1 + 1].setBrightnessSmooth(-outputVoltage, sampleTime);

				outputVoltage = math::rescale(voltages[blockIndex * 4 + 1], 0.f, 5.f, 0.f, 1.f);
				lights[LIGHT_X2].setBrightnessSmooth(outputVoltage, sampleTime);
				lights[LIGHT_X2 + 1].setBrightnessSmooth(-outputVoltage, sampleTime);

				outputVoltage = math::rescale(voltages[blockIndex * 4 + 2], 0.f, 5.f, 0.f, 1.f);
				lights[LIGHT_X3].setBrightnessSmooth(outputVoltage, sampleTime);
				lights[LIGHT_X3 + 1].setBrightnessSmooth(-outputVoltage, sampleTime);
			} else {
				lights[LIGHT_EXTERNAL].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
				lights[LIGHT_EXTERNAL + 1].setBrightnessSmooth(0.f, sampleTime);

				lights[LIGHT_T1].setBrightnessSmooth(bLastGate, sampleTime);

				lights[LIGHT_T2].setBrightnessSmooth(bLastGate, sampleTime);

				lights[LIGHT_T3].setBrightnessSmooth(bLastGate, sampleTime);

				float scaledVoltage = math::rescale(newNoteVoltage, 0.f, 5.f, 0.f, 1.f);

				lights[LIGHT_X1].setBrightnessSmooth(scaledVoltage, sampleTime);
				lights[LIGHT_X1 + 1].setBrightnessSmooth(-scaledVoltage, sampleTime);

				lights[LIGHT_X2].setBrightnessSmooth(scaledVoltage, sampleTime);
				lights[LIGHT_X2 + 1].setBrightnessSmooth(-scaledVoltage, sampleTime);

				lights[LIGHT_X3].setBrightnessSmooth(scaledVoltage, sampleTime);
				lights[LIGHT_X3 + 1].setBrightnessSmooth(-scaledVoltage, sampleTime);
			}

			float yVoltage = math::rescale(voltages[blockIndex * 4 + 3], 0.f, 5.f, 0.f, 1.f);
			lights[LIGHT_Y].setBrightnessSmooth(yVoltage, sampleTime);
			lights[LIGHT_Y + 1].setBrightnessSmooth(-yVoltage, sampleTime);

			if (!bXClockSourceExternal) {
				lights[LIGHT_INTERNAL_X_CLOCK_SOURCE].setBrightnessSmooth(marmora::paletteClockSources[xClockSourceInternal].red, sampleTime);
				lights[LIGHT_INTERNAL_X_CLOCK_SOURCE + 1].setBrightnessSmooth(marmora::paletteClockSources[xClockSourceInternal].green, sampleTime);
				lights[LIGHT_INTERNAL_X_CLOCK_SOURCE + 2].setBrightnessSmooth(marmora::paletteClockSources[xClockSourceInternal].blue, sampleTime);
			} else {
				lights[LIGHT_INTERNAL_X_CLOCK_SOURCE].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_INTERNAL_X_CLOCK_SOURCE + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_INTERNAL_X_CLOCK_SOURCE + 2].setBrightnessSmooth(0.f, sampleTime);
			}

			getParamQuantity(PARAM_Y_RATE)->description = marmora::yDividerDescriptions[yDividerIndex];
		}
	}

	void drawLight(const int& light, const LightModes& lightMode, const float& sampleTime, const long long& systemTimeMs) {
		switch (lightMode) {
		case LIGHT_OFF:
			lights[light].setBrightnessSmooth(0.f, sampleTime);
			break;
		case LIGHT_ON:
			lights[light].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
			break;
		case LIGHT_BLINK_SLOW:
			lights[light].setBrightnessSmooth((systemTimeMs & 255) > 128 ? kSanguineButtonLightValue : 0.f, sampleTime);
			break;
		case LIGHT_BLINK_FAST:
			lights[light].setBrightnessSmooth((systemTimeMs & 127) > 64 ? kSanguineButtonLightValue : 0.f, sampleTime);
			break;
		default:
			break;
		}
	}

	void drawDejaVuLight(const int& light, const marmora::DejaVuLockModes& lockMode, const float& sampleTime,
		const long long& systemTimeMs) {
		int slowTriangle;
		int pulseWidth;
		int fastTriangle;
		switch (lockMode) {
		case marmora::DEJA_VU_LOCK_ON:
			slowTriangle = (systemTimeMs & 1023) >> 5;
			slowTriangle = slowTriangle >= 16 ? 31 - slowTriangle : slowTriangle;
			pulseWidth = systemTimeMs & 15;
			lights[light].setBrightnessSmooth(slowTriangle >= pulseWidth ? kSanguineButtonLightValue : 0.f, sampleTime);
			break;
		case marmora::DEJA_VU_LOCK_OFF:
			lights[light].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
			break;
		case marmora::DEJA_VU_SUPER_LOCK:
			fastTriangle = (systemTimeMs & 511) >> 4;
			fastTriangle = fastTriangle >= 16 ? 31 - fastTriangle : fastTriangle;
			pulseWidth = systemTimeMs & 15;
			lights[light].setBrightnessSmooth(fastTriangle >= pulseWidth ? kSanguineButtonLightValue : 0.f, sampleTime);
			break;
		}
	}

	void stepBlock() {
		// Ramps.
		marbles::Ramps ramps;
		ramps.master = rampMaster;
		ramps.external = rampExternal;
		ramps.slave[0] = rampSlave[0];
		ramps.slave[1] = rampSlave[1];

		float dejaVu = clamp(params[PARAM_DEJA_VU].getValue() + inputs[INPUT_DEJA_VU].getVoltage() / 5.f, 0.f, 1.f);

		float dejaVuLengthIndex = params[PARAM_DEJA_VU_LENGTH].getValue() * (LENGTHOF(marmora::loopLengths) - 1);
		int dejaVuLength = marmora::loopLengths[static_cast<int>(roundf(dejaVuLengthIndex))];

		// Set up TGenerator.
		bool bTClockSourceExternal = inputs[INPUT_T_CLOCK].isConnected();

		tGenerator.set_model(static_cast<marbles::TGeneratorModel>(params[PARAM_T_MODE].getValue()));
		tGenerator.set_range(static_cast<marbles::TGeneratorRange>(params[PARAM_T_RANGE].getValue()));
		float tRate = 60.f * (params[PARAM_T_RATE].getValue() + inputs[INPUT_T_RATE].getVoltage() / 5.f);
		tGenerator.set_rate(tRate);
		float tBias = clamp(params[PARAM_T_BIAS].getValue() + inputs[INPUT_T_BIAS].getVoltage() / 5.f, 0.f, 1.f);
		tGenerator.set_bias(tBias);
		float tJitter = clamp(params[PARAM_T_JITTER].getValue() + inputs[INPUT_T_JITTER].getVoltage() / 5.f, 0.f, 1.f);
		tGenerator.set_jitter(tJitter);
		if (dejaVuLockModeT != marmora::DEJA_VU_SUPER_LOCK) {
			tGenerator.set_deja_vu(bDejaVuTEnabled ? dejaVu : 0.f);
		} else {
			tGenerator.set_deja_vu(0.5f);
		}
		tGenerator.set_length(dejaVuLength);
		tGenerator.set_pulse_width_mean(params[PARAM_GATE_BIAS].getValue());
		tGenerator.set_pulse_width_std(params[PARAM_GATE_JITTER].getValue());

		tGenerator.Process(bTClockSourceExternal, &bWantTReset, tClocks, ramps, bGates, marmora::kBlockSize);

		// Set up XYGenerator.
		marbles::ClockSource xClockSource = xClockSourceInternal;
		if (bXClockSourceExternal) {
			xClockSource = marbles::CLOCK_SOURCE_EXTERNAL;
		}

		if (!bScaleEditMode) {
			marbles::GroupSettings x;
			x.control_mode = static_cast<marbles::ControlMode>(params[PARAM_X_MODE].getValue());
			x.voltage_range = static_cast<marbles::VoltageRange>(params[PARAM_X_RANGE].getValue());
			// TODO: Fix the scaling.
			/* I think the double multiplication by 0.5f (both in the next line and when assigning "u" might be wrong:
			   custom scales seem to behave nicely when NOT doing that...) -Bat */
			float noteCV = 0.5f * (params[PARAM_X_SPREAD].getValue() + inputs[INPUT_X_SPREAD].getVoltage() / 5.f);
			// NOTE: WTF is u? (A leftover from marbles.cc -Bat Ed.)
			float u = noteFilter.Process(0.5f * (noteCV + 1.f));
			x.register_mode = params[PARAM_EXTERNAL].getValue();
			x.register_value = u;

			x.spread = clamp(params[PARAM_X_SPREAD].getValue() + inputs[INPUT_X_SPREAD].getVoltage() / 5.f, 0.f, 1.f);
			x.bias = clamp(params[PARAM_X_BIAS].getValue() + inputs[INPUT_X_BIAS].getVoltage() / 5.f, 0.f, 1.f);
			x.steps = clamp(params[PARAM_X_STEPS].getValue() + inputs[INPUT_X_STEPS].getVoltage() / 5.f, 0.f, 1.f);
			if (dejaVuLockModeX != marmora::DEJA_VU_SUPER_LOCK) {
				x.deja_vu = bDejaVuXEnabled ? dejaVu : 0.f;
			} else {
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
			y.register_value = 0.f;
			y.spread = params[PARAM_Y_SPREAD].getValue();
			y.bias = params[PARAM_Y_BIAS].getValue();
			y.steps = params[PARAM_Y_STEPS].getValue();
			y.deja_vu = 0.f;
			y.length = 1;

			unsigned int index = params[PARAM_Y_RATE].getValue() * LENGTHOF(marmora::yDividerRatios);
			if (index < LENGTHOF(marmora::yDividerRatios)) {
				yDividerIndex = index;
			}
			y.ratio = marmora::yDividerRatios[yDividerIndex];
			y.scale_index = xScale;

			xyGenerator.Process(xClockSource, x, y, &bWantXReset, xyClocks, ramps, voltages, marmora::kBlockSize);
		} else {
			/* Was
			float noteCV = 0.5f * (params[PARAM_X_SPREAD].getValue() + inputs[INPUT_X_SPREAD].getVoltage() / 5.f);
			*/
			newNoteVoltage = inputs[INPUT_X_SPREAD].getVoltage();
			float noteCV = (newNoteVoltage / 5.f);
			float u = noteFilter.Process(0.5f * (noteCV + 1.f));
			if (inputs[INPUT_X_CLOCK].getVoltage() >= 0.5f) {
				float voltage = (u - 0.5f) * 10.f;
				if (!bLastGate) {
					scaleRecorder.NewNote(voltage);
					bLastGate = true;
				} else {
					scaleRecorder.UpdateVoltage(voltage);
				}
			} else {
				if (bLastGate) {
					scaleRecorder.AcceptNote();
					bLastGate = false;
				}
			}
		}
	}

	void init() {
		yDividerIndex = 4;
	}

	void onReset(const ResetEvent& e) override {
		init();
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		float sampleRate = e.sampleRate;
		memset(&tGenerator, 0, sizeof(marbles::TGenerator));
		memset(&xyGenerator, 0, sizeof(marbles::XYGenerator));

		tGenerator.Init(&randomStream, sampleRate);
		xyGenerator.Init(&randomStream, sampleRate);

		// Set scales.
		if (!bModuleAdded) {
			for (int scale = 0; scale < marmora::kMaxScales; ++scale) {
				xyGenerator.LoadScale(scale, marmora::presetScales[scale]);
				copyScale(marmora::presetScales[scale], marmoraScales[scale].scale);
				marmoraScales[scale].bScaleDirty = false;
			}
			bModuleAdded = true;
		} else {
			for (int scale = 0; scale < marmora::kMaxScales; ++scale) {
				xyGenerator.LoadScale(scale, marmoraScales[scale].scale);
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "y_divider_index", yDividerIndex);
		setJsonInt(rootJ, "userSeed", userSeed);

		for (int scale = 0; scale < marmora::kMaxScales; ++scale) {
			if (marmoraScales[scale].bScaleDirty) {
				json_t* scaleDataVoltagesJ = json_array();
				json_t* scaleDataWeightsJ = json_array();

				std::string scaleHeader = string::f("userScale%d", scale);
				std::string scaleBaseInterval = scaleHeader + "Interval";
				std::string scaleDegrees = scaleHeader + "Degrees";
				std::string scaleDataVoltages = scaleHeader + "DataVoltages";
				std::string scaleDataWeights = scaleHeader + "DataWeights";
				setJsonFloat(rootJ, scaleBaseInterval.c_str(), marmoraScales[scale].scale.base_interval);
				setJsonInt(rootJ, scaleDegrees.c_str(), marmoraScales[scale].scale.num_degrees);

				for (int degree = 0; degree < marmoraScales[scale].scale.num_degrees; ++degree) {
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

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "userSeed", intValue)) {
			userSeed = intValue;
			randomGenerator.Init(userSeed);
		}

		if (getJsonInt(rootJ, "y_divider_index", intValue)) {
			yDividerIndex = intValue;
		}

		int dirtyScalesCount = 0;

		for (int scale = 0; scale < marmora::kMaxScales; ++scale) {
			std::string scaleHeader = string::f("userScale%d", scale);
			std::string scaleBaseInterval = scaleHeader + "Interval";

			json_t* scaleBaseIntervalJ = json_object_get(rootJ, scaleBaseInterval.c_str());
			if (scaleBaseIntervalJ) {
				bool bScaleOk = false;
				std::string scaleDegrees = scaleHeader + "Degrees";
				std::string scaleDataVoltages = scaleHeader + "DataVoltages";
				std::string scaleDataWeights = scaleHeader + "DataWeights";
				marbles::Scale customScale;

				customScale.Init();

				customScale.base_interval = json_number_value(scaleBaseIntervalJ);

				json_t* scaleDegreesJ = json_object_get(rootJ, scaleDegrees.c_str());
				bScaleOk = scaleDegreesJ;
				if (bScaleOk) {
					customScale.num_degrees = json_integer_value(scaleDegreesJ);

					json_t* scaleDataVoltagesJ = json_object_get(rootJ, scaleDataVoltages.c_str());
					json_t* scaleDataWeightsJ = json_object_get(rootJ, scaleDataWeights.c_str());

					bScaleOk = scaleDataVoltagesJ && scaleDataWeightsJ;
					if (bScaleOk) {
						for (int degree = 0; degree < customScale.num_degrees; ++degree) {
							json_t* voltageJ = json_array_get(scaleDataVoltagesJ, degree);
							json_t* weightJ = json_array_get(scaleDataWeightsJ, degree);
							bScaleOk = voltageJ && weightJ;
							if (bScaleOk) {
								customScale.degree[degree].voltage = json_number_value(voltageJ);
								customScale.degree[degree].weight = json_integer_value(weightJ);
							}
						}
					}
				}
				if (bScaleOk) {
					copyScale(customScale, marmoraScales[scale].scale);
					marmoraScales[scale].bScaleDirty = true;
					++dirtyScalesCount;
				}
			}
		} // for
		if (dirtyScalesCount > 0) {
			for (int scale = 0; scale < marmora::kMaxScales; ++scale) {
				if (marmoraScales[scale].bScaleDirty) {
					xyGenerator.LoadScale(scale, marmoraScales[scale].scale);
				}
			}
		}
	}

	void toggleScaleEdit() {
		if (bScaleEditMode) {
			marbles::Scale customScale;
			bool bScaleSuccessful = scaleRecorder.ExtractScale(&customScale);
			if (bScaleSuccessful) {
				int currentScale = params[PARAM_SCALE].getValue();
				marmoraScales[currentScale].scale.Init();
				copyScale(customScale, marmoraScales[currentScale].scale);
				xyGenerator.LoadScale(currentScale, marmoraScales[currentScale].scale);
				marmoraScales[currentScale].bScaleDirty = true;
			}
		} else {
			scaleRecorder.Clear();
			bLastGate = false;
		}
		bScaleEditMode = !bScaleEditMode;
	}

	void resetScale() {
		int currentScale = params[PARAM_SCALE].getValue();

		copyScale(marmora::presetScales[currentScale], marmoraScales[currentScale].scale);
		marmoraScales[currentScale].bScaleDirty = false;
		xyGenerator.LoadScale(currentScale, marmoraScales[currentScale].scale);
	}

	void copyScale(const marbles::Scale& source, marbles::Scale& destination) {
		destination.base_interval = source.base_interval;
		destination.num_degrees = source.num_degrees;

		for (int degree = 0; degree < source.num_degrees; ++degree) {
			destination.degree[degree].voltage = source.degree[degree].voltage;
			destination.degree[degree].weight = source.degree[degree].weight;
		}
	}

	void setUserSeed(uint32_t newSeed) {
		userSeed = newSeed;
		randomGenerator.Init(userSeed);
	}
};

struct MarmoraWidget : SanguineModuleWidget {
	explicit MarmoraWidget(Marmora* module) {
		setModule(module);

		moduleName = "marmora";
		panelSize = sanguineThemes::SIZE_28;
		backplateColor = sanguineThemes::PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		addParam(createParam<Sanguine1PSPurple>(millimetersToPixelsVec(64.430, 5.618), module, Marmora::PARAM_DEJA_VU));

		addParam(createLightParam<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(8.677, 26.586), module,
			Marmora::PARAM_T_MODE, Marmora::LIGHT_T_MODE));

		addParam(createLightParam<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(35.582, 26.586), module,
			Marmora::PARAM_T_RANGE, Marmora::LIGHT_T_RANGE));

		CKD6* buttonDejaVuT = createParam<CKD6>(millimetersToPixelsVec(49.568, 21.610), module, Marmora::PARAM_DEJA_VU_T);
		buttonDejaVuT->latch = true;
		buttonDejaVuT->momentary = false;
		addParam(buttonDejaVuT);
		addChild(createLightCentered<CKD6Light<GreenLight>>(millimetersToPixelsVec(54.268, 26.298), module, Marmora::LIGHT_DEJA_VU_T));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(67.120, 26.630), module, Marmora::INPUT_DEJA_VU));

		CKD6* buttonDejaVuX = createParam<CKD6>(millimetersToPixelsVec(83.272, 21.610), module, Marmora::PARAM_DEJA_VU_X);
		buttonDejaVuX->latch = true;
		buttonDejaVuX->momentary = false;
		addParam(buttonDejaVuX);
		addChild(createLightCentered<CKD6Light<RedLight>>(millimetersToPixelsVec(87.972, 26.298), module, Marmora::LIGHT_DEJA_VU_X));

		addParam(createLightParam<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(100.657, 26.586), module,
			Marmora::PARAM_X_RANGE, Marmora::LIGHT_X_RANGE));

		addParam(createLightParam<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(127.576, 26.586), module,
			Marmora::PARAM_X_MODE, Marmora::LIGHT_X_MODE));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(4.571, 37.199), module, Marmora::INPUT_T_RATE));

		addParam(createParam<Sanguine3PSGreen>(millimetersToPixelsVec(16.373, 32.449), module, Marmora::PARAM_T_RATE));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(37.706, 37.199), module, Marmora::INPUT_T_CLOCK));

		TL1105* buttonTSuperLock = createParam<TL1105>(millimetersToPixelsVec(51.618, 34.547), module, Marmora::PARAM_T_SUPER_LOCK);
		buttonTSuperLock->momentary = false;
		buttonTSuperLock->latch = true;
		addParam(buttonTSuperLock);

		TL1105* buttonXSuperLock = createParam<TL1105>(millimetersToPixelsVec(85.322, 34.547), module, Marmora::PARAM_X_SUPER_LOCK);
		buttonXSuperLock->momentary = false;
		buttonXSuperLock->latch = true;
		addParam(buttonXSuperLock);

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(96.545, 37.199), module, Marmora::INPUT_X_CLOCK));

		addParam(createParam<Sanguine3PSRed>(millimetersToPixelsVec(108.356, 32.449), module, Marmora::PARAM_X_SPREAD));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(129.681, 37.199), module, Marmora::INPUT_X_SPREAD));

		addParam(createParam<Sanguine1PSPurple>(millimetersToPixelsVec(64.430, 41.416), module, Marmora::PARAM_DEJA_VU_LENGTH));

		addParam(createParam<Sanguine1PSGreen>(millimetersToPixelsVec(4.981, 52.644), module, Marmora::PARAM_T_BIAS));

		addParam(createParam<Sanguine1PSGreen>(millimetersToPixelsVec(31.886, 52.644), module, Marmora::PARAM_T_JITTER));

		addParam(createLightParam<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(millimetersToPixelsVec(97.545, 55.875),
			module, Marmora::PARAM_INTERNAL_X_CLOCK_SOURCE, Marmora::LIGHT_INTERNAL_X_CLOCK_SOURCE));

		addParam(createLightParam<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(114.106, 55.875),
			module, Marmora::PARAM_SCALE, Marmora::LIGHT_SCALE));

		addParam(createLightParam<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(130.681, 55.875),
			module, Marmora::PARAM_EXTERNAL, Marmora::LIGHT_EXTERNAL));

		addParam(createParam<Sanguine1PSBlue>(millimetersToPixelsVec(52.078, 63.529), module, Marmora::PARAM_Y_RATE));

		addParam(createParam<Sanguine1PSBlue>(millimetersToPixelsVec(76.782, 63.529), module, Marmora::PARAM_Y_STEPS));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(7.671, 71.519), module, Marmora::INPUT_T_BIAS));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(34.576, 71.519), module, Marmora::INPUT_T_JITTER));

		addParam(createParam<Sanguine1PSRed>(millimetersToPixelsVec(96.961, 69.879), module, Marmora::PARAM_X_STEPS));

		addParam(createParam<Sanguine1PSRed>(millimetersToPixelsVec(123.872, 69.879), module, Marmora::PARAM_X_BIAS));

		addInput(createInput<BananutBlack>(millimetersToPixelsVec(21.123, 77.916), module, Marmora::INPUT_T_RESET));

		addInput(createInput<BananutBlack>(millimetersToPixelsVec(113.106, 77.916), module, Marmora::INPUT_X_RESET));

		addParam(createParam<Sanguine1PSGreen>(millimetersToPixelsVec(4.981, 84.313), module, Marmora::PARAM_GATE_BIAS));

		addParam(createParam<Sanguine1PSGreen>(millimetersToPixelsVec(31.886, 84.313), module, Marmora::PARAM_GATE_JITTER));

		addParam(createParam<Sanguine1PSBlue>(millimetersToPixelsVec(52.078, 84.313), module, Marmora::PARAM_Y_SPREAD));

		addParam(createParam<Sanguine1PSBlue>(millimetersToPixelsVec(76.782, 84.313), module, Marmora::PARAM_Y_BIAS));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(99.651, 88.803), module, Marmora::INPUT_X_STEPS));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(126.562, 88.803), module, Marmora::INPUT_X_BIAS));

		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(5.701, 105.889), module, Marmora::LIGHT_T1));

		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(18.123, 105.889), module, Marmora::LIGHT_T2));

		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(30.545, 105.889), module, Marmora::LIGHT_T3));

		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(69.582, 105.889), module, Marmora::LIGHT_Y));

		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(108.599, 105.889), module, Marmora::LIGHT_X1));

		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(121.013, 105.889), module, Marmora::LIGHT_X2));

		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(133.427, 105.889), module, Marmora::LIGHT_X3));

		addOutput(createOutput<BananutRed>(millimetersToPixelsVec(3.240, 110.250), module, Marmora::OUTPUT_T1));

		addOutput(createOutput<BananutRed>(millimetersToPixelsVec(15.662, 110.250), module, Marmora::OUTPUT_T2));

		addOutput(createOutput<BananutRed>(millimetersToPixelsVec(28.084, 110.250), module, Marmora::OUTPUT_T3));

		addOutput(createOutput<BananutRed>(millimetersToPixelsVec(67.120, 110.250), module, Marmora::OUTPUT_Y));

		addOutput(createOutput<BananutRed>(millimetersToPixelsVec(106.137, 110.250), module, Marmora::OUTPUT_X1));

		addOutput(createOutput<BananutRed>(millimetersToPixelsVec(118.551, 110.250), module, Marmora::OUTPUT_X2));

		addOutput(createOutput<BananutRed>(millimetersToPixelsVec(130.965, 110.250), module, Marmora::OUTPUT_X3));

#ifndef METAMODULE
		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 41.957, 114.855);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 94.378, 113.441);
		addChild(mutantsLogo);
#endif
	}

	struct TextFieldMenuItem : ui::TextField {
		uint32_t value;
		Marmora* module;

		TextFieldMenuItem(uint32_t theValue, Marmora* theModule) {
			value = theValue;
			module = theModule;
			multiline = false;
			box.size = Vec(150, 20);
			text = string::f("%u", value);
		}

		void step() override {
			// Keep selected.
			APP->event->setSelectedWidget(this);
			TextField::step();
		}

		void onSelectKey(const SelectKeyEvent& e) override {
			if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				uint32_t newValue = 0;
				if (strToUInt32(text.c_str(), newValue)) {
					module->setUserSeed(newValue);
				}

				ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
				overlay->requestDelete();
				e.consume(this);
			}

			if (!e.getTarget()) {
				TextField::onSelectKey(e);
			}
		}
	};

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Marmora* module = dynamic_cast<Marmora*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("t generator", "",
			[=](Menu* menu) {
				menu->addChild(createIndexSubmenuItem("Mode", marmora::tModeLabels,
					[=]() {return module->params[Marmora::PARAM_T_MODE].getValue(); },
					[=](int i) {module->params[Marmora::PARAM_T_MODE].setValue(i); }
					));

				menu->addChild(createIndexSubmenuItem("Range", marmora::tRangeLabels,
					[=]() {return module->params[Marmora::PARAM_T_RANGE].getValue(); },
					[=](int i) {module->params[Marmora::PARAM_T_RANGE].setValue(i); }
				));

				menu->addChild(createMenuItem("Reset/reseed", "", [=]() {
					module->bWantMenuTReset = true;
					}));

			}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("X generator", "",
			[=](Menu* menu) {
				menu->addChild(createIndexSubmenuItem("Mode", marmora::xModeLabels,
					[=]() {return module->params[Marmora::PARAM_X_MODE].getValue(); },
					[=](int i) {module->params[Marmora::PARAM_X_MODE].setValue(i); }
					));

				menu->addChild(createIndexSubmenuItem("Range", marmora::xRangeLabels,
					[=]() {return module->params[Marmora::PARAM_X_RANGE].getValue(); },
					[=](int i) {module->params[Marmora::PARAM_X_RANGE].setValue(i); }
				));

				menu->addChild(createIndexSubmenuItem("Internal clock source", marmora::internalClockLabels,
					[=]() {return module->params[Marmora::PARAM_INTERNAL_X_CLOCK_SOURCE].getValue(); },
					[=](int i) {module->params[Marmora::PARAM_INTERNAL_X_CLOCK_SOURCE].setValue(i); }
				));

				menu->addChild(createMenuItem("Reset", "", [=]() {
					module->bWantMenuXReset = true;
					}));
			}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Module seed", "",
			[=](Menu* menu) {
				menu->addChild(createMenuItem("Reseed rng", "", [=]() {
					module->randomGenerator.GetWord();
					}));

				menu->addChild(new MenuSeparator);

				menu->addChild(createMenuLabel("Min: 1, Max: 4294967295, ENTER to set"));

				menu->addChild(createSubmenuItem("User", "",
					[=](Menu* menu) {
						menu->addChild(new TextFieldMenuItem(module->userSeed, module));
					}
				));
			}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Scales", "",
			[=](Menu* menu) {
				menu->addChild(createIndexSubmenuItem("Select active", marmora::scaleLabels,
					[=]() {return module->params[Marmora::PARAM_SCALE].getValue(); },
					[=](int i) {module->params[Marmora::PARAM_SCALE].setValue(i); }
					));

				menu->addChild(new MenuSeparator);

				menu->addChild(createCheckMenuItem("Edit current", "",
					[=]() {return module->bScaleEditMode; },
					[=]() {module->toggleScaleEdit(); }));

				menu->addChild(createMenuItem("Reset current", "",
					[=]() {module->resetScale(); }
				));
			}
		));
	}
};

Model* modelMarmora = createModel<Marmora, MarmoraWidget>("Sanguine-Marmora");