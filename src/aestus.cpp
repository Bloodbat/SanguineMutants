#include "plugin.hpp"

#include <array>

#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"
#include "sanguinechannels.hpp"

#include "tides/generator.h"
#include "tides/plotter.h"

#include "aestuscommon.hpp"
#include "aestus.hpp"

using namespace sanguineCommonCode;

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

		INPUT_MODEL,
		INPUT_MODE,
		INPUT_RANGE,
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
		LIGHT_SYNC,

		ENUMS(LIGHT_CHANNEL_1, 3),
		ENUMS(LIGHT_CHANNEL_2, 3),
		ENUMS(LIGHT_CHANNEL_3, 3),
		ENUMS(LIGHT_CHANNEL_4, 3),
		ENUMS(LIGHT_CHANNEL_5, 3),
		ENUMS(LIGHT_CHANNEL_6, 3),
		ENUMS(LIGHT_CHANNEL_7, 3),
		ENUMS(LIGHT_CHANNEL_8, 3),
		ENUMS(LIGHT_CHANNEL_9, 3),
		ENUMS(LIGHT_CHANNEL_10, 3),
		ENUMS(LIGHT_CHANNEL_11, 3),
		ENUMS(LIGHT_CHANNEL_12, 3),
		ENUMS(LIGHT_CHANNEL_13, 3),
		ENUMS(LIGHT_CHANNEL_14, 3),
		ENUMS(LIGHT_CHANNEL_15, 3),
		ENUMS(LIGHT_CHANNEL_16, 3),
		LIGHTS_COUNT
	};

	struct ModeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			Aestus* moduleAestus = static_cast<Aestus*>(module);
			if (!moduleAestus->channelIsSheep[moduleAestus->displayChannel]) {
				return aestusCommon::modeMenuLabels[moduleAestus->generators[moduleAestus->displayChannel].mode()];
			} else {
				return aestusCommon::sheepMenuLabels[moduleAestus->generators[moduleAestus->displayChannel].mode()];
			}
		}
	};

	struct RangeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			Aestus* moduleAestus = static_cast<Aestus*>(module);
			return aestusCommon::rangeMenuLabels[moduleAestus->generators[moduleAestus->displayChannel].range()];
		}
	};

	bool bSheepSelected = false;
	bool channelIsSheep[PORT_MAX_CHANNELS];
	bool lastSheepFirmwares[PORT_MAX_CHANNELS];
	bool bUseCalibrationOffset = true;
	bool lastExternalSyncs[PORT_MAX_CHANNELS];
	bool bWantPeacocks = false;
	size_t frames[PORT_MAX_CHANNELS];
	static const int kLightsFrequency = 16;
	int channelCount = 1;
	int displayChannel = 0;
	uint8_t lastGates[PORT_MAX_CHANNELS];

	tides::GeneratorMode selectedMode = tides::GENERATOR_MODE_LOOPING;
	tides::GeneratorMode lastModes[PORT_MAX_CHANNELS];
	tides::GeneratorRange selectedRange = tides::GENERATOR_RANGE_MEDIUM;
	tides::GeneratorRange lastRanges[PORT_MAX_CHANNELS];

	tides::Generator generators[PORT_MAX_CHANNELS];
	tides::Plotter plotters[PORT_MAX_CHANNELS];
	dsp::SchmittTrigger stMode;
	dsp::SchmittTrigger stRange;
	dsp::ClockDivider lightsDivider;
	std::string displayModel = aestus::displayModels[0];

	float log2SampleRate = 0.f;

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

		configInput(INPUT_MODEL, "Model CV");
		configInput(INPUT_MODE, "Mode CV");
		configInput(INPUT_RANGE, "Range CV");

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&generators[channel], 0, sizeof(tides::Generator));
			generators[channel].Init();
			plotters[channel].Init(tides::plotInstructions,
				sizeof(tides::plotInstructions) / sizeof(tides::PlotInstruction));
		}

		init();

		lightsDivider.setDivision(kLightsFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		channelCount = std::max(std::max(inputs[INPUT_PITCH].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

		bool bIsLightsTurn = lightsDivider.process();

		if (!bWantPeacocks) {
			bSheepSelected = params[PARAM_MODEL].getValue() > 0.f;

			bool bHaveExternalSync = static_cast<bool>(params[PARAM_SYNC].getValue());

			if (stMode.process(params[PARAM_MODE].getValue())) {
				selectedMode = static_cast<tides::GeneratorMode>((static_cast<int>(selectedMode) + 1) % 3);
			}

			if (stRange.process(params[PARAM_RANGE].getValue()) && !bHaveExternalSync) {
				selectedRange = static_cast<tides::GeneratorRange>((static_cast<int>(selectedRange) - 1 + 3) % 3);
			}

			std::array<tides::GeneratorMode, PORT_MAX_CHANNELS> channelModes;
			std::array<tides::GeneratorRange, PORT_MAX_CHANNELS> channelRanges;

			channelModes.fill(selectedMode);
			channelRanges.fill(selectedRange);

			bool bModelConnected = inputs[INPUT_MODEL].isConnected();
			bool bModeConnected = inputs[INPUT_MODE].isConnected();
			bool bRangeConnected = inputs[INPUT_RANGE].isConnected();

			float knobFrequency = params[PARAM_FREQUENCY].getValue();
			float knobFm = params[PARAM_FM].getValue();
			float knobShape = params[PARAM_SHAPE].getValue();
			float knobSlope = params[PARAM_SLOPE].getValue();
			float knobSmoothness = params[PARAM_SMOOTHNESS].getValue();

			tides::GeneratorSample samples[PORT_MAX_CHANNELS];
			float unipolarFlags[PORT_MAX_CHANNELS];

			if (bModeConnected) {
				float_4 modeVoltages;
				for (int channel = 0; channel < channelCount; channel += 4) {
					modeVoltages = inputs[INPUT_MODE].getVoltageSimd<float_4>(channel);

					modeVoltages = simd::round(modeVoltages);
					modeVoltages = simd::clamp(modeVoltages, 0.f, 3.f);

					channelModes[channel] = static_cast<tides::GeneratorMode>(modeVoltages[0]);
					channelModes[channel + 1] = static_cast<tides::GeneratorMode>(modeVoltages[1]);
					channelModes[channel + 2] = static_cast<tides::GeneratorMode>(modeVoltages[2]);
					channelModes[channel + 3] = static_cast<tides::GeneratorMode>(modeVoltages[3]);
				}
			}

			if (bRangeConnected) {
				float_4 rangeVoltages;
				for (int channel = 0; channel < channelCount; channel += 4) {
					rangeVoltages = inputs[INPUT_MODE].getVoltageSimd<float_4>(channel);

					rangeVoltages = simd::round(rangeVoltages);
					rangeVoltages = simd::clamp(rangeVoltages, 0.f, 3.f);

					channelRanges[channel] = static_cast<tides::GeneratorRange>(rangeVoltages[0]);
					channelRanges[channel + 1] = static_cast<tides::GeneratorRange>(rangeVoltages[1]);
					channelRanges[channel + 2] = static_cast<tides::GeneratorRange>(rangeVoltages[2]);
					channelRanges[channel + 3] = static_cast<tides::GeneratorRange>(rangeVoltages[3]);
				}
			}

			for (int channel = 0; channel < channelCount; ++channel) {
				if (lastModes[channel] != channelModes[channel]) {
					generators[channel].set_mode(channelModes[channel]);
					lastModes[channel] = channelModes[channel];
				}

				if (lastRanges[channel] != channelRanges[channel]) {
					generators[channel].set_range(channelRanges[channel]);
					lastRanges[channel] = channelRanges[channel];
				}

				channelIsSheep[channel] = (!bModelConnected && bSheepSelected) ||
					(bModelConnected && inputs[INPUT_MODEL].getVoltage(channel) >= 1.f);

				// Buffer loop.
				if (++frames[channel] >= tides::kBlockSize) {
					frames[channel] = 0;

					// Sync.
					// This takes a moment to catch up if sync is on and patches or presets have just been loaded!
					if (bHaveExternalSync != lastExternalSyncs[channel]) {
						generators[channel].set_sync(bHaveExternalSync);
						lastExternalSyncs[channel] = bHaveExternalSync;
					}

					// Setup SIMD voltages.
					float_4 inputVoltages;

					inputVoltages[0] = inputs[INPUT_FM].getNormalVoltage(0.1f, channel);
					inputVoltages[1] = inputs[INPUT_SHAPE].getVoltage(channel);
					inputVoltages[2] = inputs[INPUT_SLOPE].getVoltage(channel);
					inputVoltages[3] = inputs[INPUT_SMOOTHNESS].getVoltage(channel);

					inputVoltages /= 5.f;

					// Pitch.
					float pitch = knobFrequency;
					pitch += 12.f * (inputs[INPUT_PITCH].getVoltage(channel) +
						aestusCommon::calibrationOffsets[bUseCalibrationOffset]);
					pitch += knobFm * inputVoltages[0];
					pitch += 60.f;
					// Scale to the global sample rate.
					pitch += log2SampleRate * 12.f;
					generators[channel].set_pitch(static_cast<int>(
						clamp(pitch * 128.f, static_cast<float>(-32768), static_cast<float>(32767))));

					// Shape, slope, smoothness.
					inputVoltages[1] += knobShape;
					inputVoltages[2] += knobSlope;
					inputVoltages[3] += knobSmoothness;

					inputVoltages = simd::clamp(inputVoltages, -1.f, 1.f);
					inputVoltages *= 32767.f;

					int16_t shape = static_cast<int16_t>(inputVoltages[1]);
					int16_t slope = static_cast<int16_t>(inputVoltages[2]);
					int16_t smoothness = static_cast<int16_t>(inputVoltages[3]);
					generators[channel].set_shape(shape);
					generators[channel].set_slope(slope);
					generators[channel].set_smoothness(smoothness);

					generators[channel].Process(channelIsSheep[channel]);
				}

				// Level.
				uint16_t level = static_cast<uint16_t>(
					clamp(inputs[INPUT_LEVEL].getNormalVoltage(8.f, channel) / 8.f, 0.f, 1.f)) * 65535;
				if (level < 32) {
					level = 0;
				}

				uint8_t gate = 0;
				if (inputs[INPUT_FREEZE].getVoltage(channel) >= 0.7f) {
					gate |= tides::CONTROL_FREEZE;
				}
				if (inputs[INPUT_TRIGGER].getVoltage(channel) >= 0.7f) {
					gate |= tides::CONTROL_GATE;
				}
				if (inputs[INPUT_CLOCK].getVoltage(channel) >= 0.7f) {
					gate |= tides::CONTROL_CLOCK;
				}
				if (!(lastGates[channel] & tides::CONTROL_CLOCK) && (gate & tides::CONTROL_CLOCK)) {
					gate |= tides::CONTROL_CLOCK_RISING;
				}
				if (!(lastGates[channel] & tides::CONTROL_GATE) && (gate & tides::CONTROL_GATE)) {
					gate |= tides::CONTROL_GATE_RISING;
				}
				if ((lastGates[channel] & tides::CONTROL_GATE) && !(gate & tides::CONTROL_GATE)) {
					gate |= tides::CONTROL_GATE_FALLING;
				}
				lastGates[channel] = gate;

				samples[channel] = generators[channel].Process(gate);
				uint32_t uni = samples[channel].unipolar;
				int32_t bi = samples[channel].bipolar;

				uni = uni * level >> 16;
				bi = -bi * level >> 16;
				unipolarFlags[channel] = static_cast<float>(uni) / 65535;
				float bipolarFlag = static_cast<float>(bi) / 32768;

				outputs[OUTPUT_HIGH].setVoltage((samples[channel].flags & tides::FLAG_END_OF_ATTACK) * 5.f, channel);
				outputs[OUTPUT_LOW].setVoltage((samples[channel].flags & tides::FLAG_END_OF_RELEASE) * 5.f, channel);
				outputs[OUTPUT_UNI].setVoltage(unipolarFlags[channel] * 8.f, channel);
				outputs[OUTPUT_BI].setVoltage(bipolarFlag * 5.f, channel);
			}

			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
					if (lastSheepFirmwares[channel] != channelIsSheep[channel]) {
						generators[channel].set_mode(lastModes[channel]);
						generators[channel].set_range(lastRanges[channel]);
						lastSheepFirmwares[channel] = channelIsSheep[channel];
					}

					int currentLight = LIGHT_CHANNEL_1 + channel * 3;
					lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(channel < channelCount &&
						!channelIsSheep[channel], sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(channel < channelCount &&
						channelIsSheep[channel], sampleTime);
				}

				if (displayChannel >= channelCount) {
					displayChannel = channelCount - 1;
				}

				tides::GeneratorMode displayMode = generators[displayChannel].mode();
				tides::GeneratorRange displayRange = generators[displayChannel].range();

				lights[LIGHT_MODE + 0].setBrightnessSmooth((displayMode == tides::GENERATOR_MODE_AD) *
					kSanguineButtonLightValue, sampleTime);
				lights[LIGHT_MODE + 1].setBrightnessSmooth((displayMode == tides::GENERATOR_MODE_AR) *
					kSanguineButtonLightValue, sampleTime);

				lights[LIGHT_RANGE + 0].setBrightnessSmooth((displayRange == tides::GENERATOR_RANGE_LOW) *
					kSanguineButtonLightValue, sampleTime);
				lights[LIGHT_RANGE + 1].setBrightnessSmooth((displayRange == tides::GENERATOR_RANGE_HIGH) *
					kSanguineButtonLightValue, sampleTime);

				if (samples[displayChannel].flags & tides::FLAG_END_OF_ATTACK) {
					unipolarFlags[displayChannel] *= -1.f;
				}
				lights[LIGHT_PHASE + 0].setBrightnessSmooth(fmaxf(0.f, unipolarFlags[displayChannel]), sampleTime);
				lights[LIGHT_PHASE + 1].setBrightnessSmooth(fmaxf(0.f, -unipolarFlags[displayChannel]), sampleTime);

				lights[LIGHT_SYNC].setBrightnessSmooth((bHaveExternalSync && !(getSystemTimeMs() & 128)) *
					kSanguineButtonLightValue, sampleTime);

				displayModel = aestus::displayModels[static_cast<int>(channelIsSheep[displayChannel])];

				if (!channelIsSheep[displayChannel]) {
					paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[0];
				} else {
					paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[1];
				}
			}
		} else {
			for (int channel = 0; channel < channelCount; ++channel) {
				if (++frames[channel] >= tides::kBlockSize) {
					frames[channel] = 0;
					plotters[channel].Run();
					outputs[OUTPUT_UNI].setVoltage(rescale(
						static_cast<float>(plotters[channel].x()), 0.f, UINT16_MAX, -8.f, 8.f), channel);
					outputs[OUTPUT_BI].setVoltage(rescale(
						static_cast<float>(plotters[channel].y()), 0.f, UINT16_MAX, 8.f, -8.f), channel);
				}
			}

			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
					int currentLight = LIGHT_CHANNEL_1 + channel * 3;
					lights[currentLight + 0].setBrightnessSmooth(channel < channelCount, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
				}

				lights[LIGHT_MODE + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_MODE + 1].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);

				lights[LIGHT_RANGE + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_RANGE + 1].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);

				lights[LIGHT_PHASE + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_PHASE + 1].setBrightnessSmooth(1.f, sampleTime);

				lights[LIGHT_SYNC + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_SYNC + 1].setBrightnessSmooth(0.f, sampleTime);

				displayModel = aestus::displayModels[aestus::kPeacocksString];
			}
		}

		outputs[OUTPUT_HIGH].setChannels(channelCount);
		outputs[OUTPUT_LOW].setChannels(channelCount);
		outputs[OUTPUT_UNI].setChannels(channelCount);
		outputs[OUTPUT_BI].setChannels(channelCount);
	}

	void init() {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			generators[channel].set_mode(tides::GENERATOR_MODE_LOOPING);
			generators[channel].set_range(tides::GENERATOR_RANGE_MEDIUM);
			lastRanges[channel] = tides::GENERATOR_RANGE_MEDIUM;
			lastModes[channel] = tides::GENERATOR_MODE_LOOPING;

			lastGates[channel] = 0;
			lastExternalSyncs[channel] = false;
			frames[channel] = 0;
			channelIsSheep[channel] = false;
			lastSheepFirmwares[channel] = false;
		}
		params[PARAM_MODEL].setValue(0.f);
	}

	void onReset(const ResetEvent& e) override {
		init();
	}

	void onRandomize(const RandomizeEvent& e) override {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			generators[channel].set_range(tides::GeneratorRange(random::u32() % 3));
			generators[channel].set_mode(tides::GeneratorMode(random::u32() % 3));
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "mode", static_cast<int>(selectedMode));
		setJsonInt(rootJ, "range", static_cast<int>(selectedRange));
		setJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);
		setJsonBoolean(rootJ, "wantPeacocksEgg", bWantPeacocks);
		setJsonInt(rootJ, "displayChannel", displayChannel);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "mode", intValue)) {
			selectedMode = static_cast<tides::GeneratorMode>(intValue);
		}

		if (getJsonInt(rootJ, "range", intValue)) {
			selectedRange = static_cast<tides::GeneratorRange>(intValue);
		}

		getJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);
		getJsonBoolean(rootJ, "wantPeacocksEgg", bWantPeacocks);

		if (getJsonInt(rootJ, "displayChannel", intValue)) {
			displayChannel = intValue;
		}
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		log2SampleRate = log2f(48000.f / e.sampleRate);
	}

	void setModel(int modelNum) {
		params[PARAM_MODEL].setValue(modelNum);
	}

	void setMode(int modeNum) {
		selectedMode = static_cast<tides::GeneratorMode>(modeNum);
	}

	void setRange(int rangeNum) {
		selectedRange = static_cast<tides::GeneratorRange>(rangeNum);
	}
};

struct AestusWidget : SanguineModuleWidget {
	explicit AestusWidget(Aestus* module) {
		setModule(module);

		moduleName = "aestus";
		panelSize = sanguineThemes::SIZE_14;
		backplateColor = sanguineThemes::PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* aestusFrameBuffer = new FramebufferWidget();
		addChild(aestusFrameBuffer);

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(5.171, 17.302), module, Aestus::INPUT_MODEL));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(16.25, 17.302), module, Aestus::PARAM_MODEL));

		SanguineTinyNumericDisplay* displayModel = new SanguineTinyNumericDisplay(1, module, 27.524, 17.302);
		displayModel->displayType = DISPLAY_STRING;
		aestusFrameBuffer->addChild(displayModel);
		displayModel->fallbackString = aestus::displayModels[0];

		if (module) {
			displayModel->values.displayText = &module->displayModel;
		}

		const float kFirstRowY = 15.972f;
		const float kSecondRowY = 18.632f;
		const float kDeltaX = 2.656f;

		float currentX = 35.611f;

		const int kLightOffset = 8;

		for (int light = 0; light < kLightOffset; ++light) {
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, kFirstRowY),
				module, Aestus::LIGHT_CHANNEL_1 + light * 3));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, kSecondRowY),
				module, Aestus::LIGHT_CHANNEL_1 + (light + kLightOffset) * 3));
			currentX += kDeltaX;
		}

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(5.171, 29.512), module, Aestus::INPUT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(20.888, 29.512),
			module, Aestus::PARAM_MODE, Aestus::LIGHT_MODE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(10.671, 38.54), module,
			Aestus::PARAM_SYNC, Aestus::LIGHT_SYNC));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(20.888, 38.54), module, Aestus::LIGHT_PHASE));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(35.56, 38.54), module, Aestus::PARAM_FREQUENCY));

		addParam(createParamCentered<Sanguine2PSRed>(millimetersToPixelsVec(59.142, 38.54), module, Aestus::PARAM_FM));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(5.171, 47.569), module, Aestus::INPUT_RANGE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(20.888, 47.569),
			module, Aestus::PARAM_RANGE, Aestus::LIGHT_RANGE));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(11.966, 65.455), module, Aestus::PARAM_SHAPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(35.56, 65.455), module, Aestus::PARAM_SLOPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(59.142, 65.455), module, Aestus::PARAM_SMOOTHNESS));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(11.966, 81.595), module, Aestus::INPUT_SHAPE));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(35.56, 81.595), module, Aestus::INPUT_SLOPE));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(59.142, 81.595), module, Aestus::INPUT_SMOOTHNESS));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.665, 98.26), module, Aestus::INPUT_TRIGGER));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(21.11, 98.26), module, Aestus::INPUT_FREEZE));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(35.554, 98.26), module, Aestus::INPUT_PITCH));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(49.998, 98.26), module, Aestus::INPUT_FM));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(64.442, 98.26), module, Aestus::INPUT_LEVEL));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.665, 111.643), module, Aestus::INPUT_CLOCK));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(21.11, 111.643), module, Aestus::OUTPUT_HIGH));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(35.554, 111.643), module, Aestus::OUTPUT_LOW));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(49.998, 111.643), module, Aestus::OUTPUT_UNI));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(64.442, 111.643), module, Aestus::OUTPUT_BI));
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

			if (!module->bSheepSelected) {
				menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[0], aestusCommon::modeMenuLabels,
					[=]() { return module->selectedMode; },
					[=](int i) { module->setMode(i); }
				));
			} else {
				menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[1], aestusCommon::sheepMenuLabels,
					[=]() { return module->selectedMode; },
					[=](int i) { module->setMode(i); }
				));
			}

			menu->addChild(createIndexSubmenuItem("Range", aestusCommon::rangeMenuLabels,
				[=]() { return module->selectedRange; },
				[=](int i) { module->setRange(i); }
			));

			menu->addChild(new MenuSeparator);

			menu->addChild(createSubmenuItem("Options", "",
				[=](Menu* menu) {
					std::vector<std::string> availableChannels;
					for (int i = 0; i < module->channelCount; ++i) {
						availableChannels.push_back(channelNumbers[i]);
					}
					menu->addChild(createIndexSubmenuItem("Display channel", availableChannels,
						[=]() {return module->displayChannel; },
						[=](int i) {module->displayChannel = i; }
					));
				}
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