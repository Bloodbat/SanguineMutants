#include "plugin.hpp"

#include <array>

#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"
#include "sanguinechannels.hpp"

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

		INPUT_MODEL,
		INPUT_MODE,
		INPUT_RANGE,
		INPUT_QUANTIZER,
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
		LIGHT_QUANTIZER1,
		LIGHT_QUANTIZER2,
		LIGHT_QUANTIZER3,

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
			Temulenti* moduleTemulenti = static_cast<Temulenti*>(module);
			switch (moduleTemulenti->generators[moduleTemulenti->displayChannel].feature_mode_) {
			case bumps::Generator::FEAT_MODE_RANDOM:
				return temulenti::drunksModeLabels[moduleTemulenti->generators[moduleTemulenti->displayChannel].mode()];
				break;
			case bumps::Generator::FEAT_MODE_HARMONIC:
				return temulenti::bumpsModeLabels[moduleTemulenti->generators[moduleTemulenti->displayChannel].mode()];
				break;
			case bumps::Generator::FEAT_MODE_SHEEP:
				return aestusCommon::sheepMenuLabels[moduleTemulenti->generators[moduleTemulenti->displayChannel].mode()];
				break;
			default:
				return aestusCommon::modeMenuLabels[moduleTemulenti->generators[moduleTemulenti->displayChannel].mode()];
				break;
			}
		}
	};

	struct RangeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			Temulenti* moduleTemulenti = static_cast<Temulenti*>(module);
			return aestusCommon::rangeMenuLabels[moduleTemulenti->generators[moduleTemulenti->displayChannel].range()];
		}
	};

	bool bUseCalibrationOffset = true;
	bool lastExternalSyncs[PORT_MAX_CHANNELS];

	static const int kLightsFrequency = 16;
	int channelCount = 1;
	int displayChannel = 0;

	bumps::GeneratorMode selectedMode = bumps::GENERATOR_MODE_LOOPING;
	bumps::GeneratorMode lastModes[PORT_MAX_CHANNELS];
	bumps::GeneratorRange selectedRange = bumps::GENERATOR_RANGE_MEDIUM;
	bumps::GeneratorRange lastRanges[PORT_MAX_CHANNELS];
	bumps::Generator::FeatureMode selectedFeatureMode = bumps::Generator::FEAT_MODE_FUNCTION;
	bumps::Generator::FeatureMode lastFeatureModes[PORT_MAX_CHANNELS];

	uint8_t lastGates[PORT_MAX_CHANNELS];
	std::array<uint8_t, PORT_MAX_CHANNELS> quantizers;

	bumps::Generator generators[PORT_MAX_CHANNELS];
	dsp::SchmittTrigger stMode;
	dsp::SchmittTrigger stRange;
	dsp::ClockDivider lightsDivider;
	std::string displayModel = temulenti::displayModels[0];

	Temulenti() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		configButton<ModeParam>(PARAM_MODE, aestusCommon::modelModeHeaders[0]);
		configButton<RangeParam>(PARAM_RANGE, temulenti::modelRangeHeaders[0]);
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

		configInput(INPUT_MODEL, "Model CV");
		configInput(INPUT_MODE, "Mode CV");
		configInput(INPUT_RANGE, "Range CV");
		configInput(INPUT_QUANTIZER, "Quantizer CV");

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&generators[channel], 0, sizeof(bumps::Generator));
			generators[channel].Init();

			lastGates[channel] = 0;
			quantizers[channel] = 0;
			lastExternalSyncs[channel] = false;
			lastRanges[channel] = bumps::GENERATOR_RANGE_MEDIUM;
			lastModes[channel] = bumps::GENERATOR_MODE_LOOPING;

			lastFeatureModes[channel] = bumps::Generator::FEAT_MODE_FUNCTION;
		}
		lightsDivider.setDivision(kLightsFrequency);
		onReset();
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		channelCount = std::max(std::max(inputs[INPUT_PITCH].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

		bool bHaveExternalSync = static_cast<bool>(params[PARAM_SYNC].getValue());

		if (stMode.process(params[PARAM_MODE].getValue())) {
			selectedMode = static_cast<bumps::GeneratorMode>((static_cast<int>(selectedMode) + 1) % 3);
		}

		if (stRange.process(params[PARAM_RANGE].getValue()) && !bHaveExternalSync) {
			selectedRange = static_cast<bumps::GeneratorRange>((static_cast<int>(selectedRange) - 1 + 3) % 3);
		}

		bool bModelConnected = inputs[INPUT_MODEL].isConnected();
		bool bModeConnected = inputs[INPUT_MODE].isConnected();
		bool bRangeConnected = inputs[INPUT_RANGE].isConnected();
		bool bQuantizerConnected = inputs[INPUT_QUANTIZER].isConnected();

		float knobFrequency = params[PARAM_FREQUENCY].getValue();
		float knobFm = params[PARAM_FM].getValue();
		float knobQuantizer = params[PARAM_QUANTIZER].getValue();
		float knobShape = params[PARAM_SHAPE].getValue();
		float knobSlope = params[PARAM_SLOPE].getValue();
		float knobSmoothness = params[PARAM_SMOOTHNESS].getValue();

		bumps::GeneratorSample samples[PORT_MAX_CHANNELS];
		float unipolarFlags[PORT_MAX_CHANNELS];

		std::array<bumps::GeneratorMode, PORT_MAX_CHANNELS> channelModes;
		std::array<bumps::GeneratorRange, PORT_MAX_CHANNELS> channelRanges;

		channelModes.fill(selectedMode);
		channelRanges.fill(selectedRange);
		quantizers.fill(static_cast<uint8_t>(knobQuantizer));

		if (bModeConnected) {
			float_4 modeVoltages;
			for (int channel = 0; channel < channelCount; channel += 4) {
				modeVoltages = inputs[INPUT_MODE].getVoltageSimd<float_4>(channel);

				modeVoltages = simd::round(modeVoltages);
				modeVoltages = simd::clamp(modeVoltages, 0.f, 3.f);

				channelModes[channel] = static_cast<bumps::GeneratorMode>(modeVoltages[0]);
				channelModes[channel + 1] = static_cast<bumps::GeneratorMode>(modeVoltages[1]);
				channelModes[channel + 2] = static_cast<bumps::GeneratorMode>(modeVoltages[2]);
				channelModes[channel + 3] = static_cast<bumps::GeneratorMode>(modeVoltages[3]);
			}
		}

		if (bRangeConnected) {
			float_4 rangeVoltages;
			for (int channel = 0; channel < channelCount; channel += 4) {
				rangeVoltages = inputs[INPUT_MODE].getVoltageSimd<float_4>(channel);

				rangeVoltages = simd::round(rangeVoltages);
				rangeVoltages = simd::clamp(rangeVoltages, 0.f, 3.f);

				channelRanges[channel] = static_cast<bumps::GeneratorRange>(rangeVoltages[0]);
				channelRanges[channel + 1] = static_cast<bumps::GeneratorRange>(rangeVoltages[1]);
				channelRanges[channel + 2] = static_cast<bumps::GeneratorRange>(rangeVoltages[2]);
				channelRanges[channel + 3] = static_cast<bumps::GeneratorRange>(rangeVoltages[3]);
			}
		}

		if (bQuantizerConnected) {
			float_4 quantizerVoltages;
			for (int channel = 0; channel < channelCount; channel += 4) {
				quantizerVoltages = inputs[INPUT_QUANTIZER].getVoltageSimd<float_4>(channel);

				quantizerVoltages = simd::round(quantizerVoltages);
				quantizerVoltages = simd::clamp(quantizerVoltages, 0.f, 7.f);

				quantizers[channel] = static_cast<uint8_t>(quantizerVoltages[0]);
				quantizers[channel + 1] = static_cast<uint8_t>(quantizerVoltages[1]);
				quantizers[channel + 2] = static_cast<uint8_t>(quantizerVoltages[2]);
				quantizers[channel + 3] = static_cast<uint8_t>(quantizerVoltages[3]);
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

			//Buffer loop.
			if (generators[channel].writable_block()) {
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
				float pitchParam = knobFrequency + (inputs[INPUT_PITCH].getVoltage(channel) +
					aestusCommon::calibrationOffsets[bUseCalibrationOffset]) * 12.f;
				float fm = clamp(inputVoltages[0] * knobFm / 12.f, -1.f, 1.f) * 1536.f;

				pitchParam += 60.f;
				// This is probably not original but seems useful to keep the same frequency as in normal mode.
				if (generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_HARMONIC && !bUseCalibrationOffset) {
					pitchParam -= 12.f;
				}

				// This is equivalent to shifting left by 7 bits.
				int16_t pitch = static_cast<int16_t>(pitchParam * 128);

				if (quantizers[channel]) {
					uint16_t semi = pitch >> 7;
					uint16_t octaves = semi / 12;
					semi -= octaves * 12;
					pitch = octaves * bumps::kOctave + bumps::quantize_lut[quantizers[channel] - 1][semi];
				}

				// Scale to the global sample rate.
				pitch += log2f(48000.f / args.sampleRate) * 12.f * 128;

				if (generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_HARMONIC) {
					generators[channel].set_pitch_high_range(clamp(pitch, -32768, 32767), fm);
				} else {
					generators[channel].set_pitch(clamp(pitch, -32768, 32767), fm);
				}

				if (generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_RANDOM) {
					generators[channel].set_pulse_width(clamp(1.f - -knobFm / 12.f, 0.f, 2.f) * 32767);
				}

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

				generators[channel].FillBuffer();
			}

			// Level.
			uint16_t level = static_cast<uint16_t>(
				clamp(inputs[INPUT_LEVEL].getNormalVoltage(8.f, channel) / 8.f, 0.f, 1.f)) * 65535;
			if (level < 32) {
				level = 0;
			}

			uint8_t gate = 0;

			if (inputs[INPUT_FREEZE].getVoltage(channel) >= 0.7f) {
				gate |= bumps::CONTROL_FREEZE;
			}
			if (inputs[INPUT_TRIGGER].getVoltage(channel) >= 0.7f) {
				gate |= bumps::CONTROL_GATE;
			}
			if (inputs[INPUT_CLOCK].getVoltage(channel) >= 0.7f) {
				gate |= bumps::CONTROL_CLOCK;
			}

			if (!(lastGates[channel] & bumps::CONTROL_CLOCK) && (gate & bumps::CONTROL_CLOCK)) {
				gate |= bumps::CONTROL_CLOCK_RISING;
			}
			if (!(lastGates[channel] & bumps::CONTROL_GATE) && (gate & bumps::CONTROL_GATE)) {
				gate |= bumps::CONTROL_GATE_RISING;
			}
			if ((lastGates[channel] & bumps::CONTROL_GATE) && !(gate & bumps::CONTROL_GATE)) {
				gate |= bumps::CONTROL_GATE_FALLING;
			}
			lastGates[channel] = gate;

			samples[channel] = generators[channel].Process(gate);

			uint32_t uni = samples[channel].unipolar;
			int32_t bi = samples[channel].bipolar;

			uni = uni * level >> 16;
			bi = -bi * level >> 16;
			unipolarFlags[channel] = static_cast<float>(uni) / 65535;
			float bipolarFlag = static_cast<float>(bi) / 32768;

			outputs[OUTPUT_HIGH].setVoltage((samples[channel].flags & bumps::FLAG_END_OF_ATTACK) ? 5.f : 0.f, channel);
			outputs[OUTPUT_LOW].setVoltage((samples[channel].flags & bumps::FLAG_END_OF_RELEASE) ? 5.f : 0.f, channel);
			outputs[OUTPUT_UNI].setVoltage(unipolarFlags[channel] * 8.f, channel);
			outputs[OUTPUT_BI].setVoltage(bipolarFlag * 5.f, channel);
		}

		outputs[OUTPUT_HIGH].setChannels(channelCount);
		outputs[OUTPUT_LOW].setChannels(channelCount);
		outputs[OUTPUT_UNI].setChannels(channelCount);
		outputs[OUTPUT_BI].setChannels(channelCount);

		if (lightsDivider.process()) {
			const float sampleTime = kLightsFrequency * args.sampleTime;

			selectedFeatureMode = bumps::Generator::FeatureMode(params[PARAM_MODEL].getValue());

			std::array<bumps::Generator::FeatureMode, PORT_MAX_CHANNELS> channelModes;
			channelModes.fill(selectedFeatureMode);

			if (bModelConnected) {
				float_4 modelVoltages;
				for (int channel = 0; channel < channelCount; channel += 4) {
					modelVoltages = inputs[INPUT_MODEL].getVoltageSimd<float_4>(channel);

					modelVoltages = simd::round(modelVoltages);
					modelVoltages = simd::clamp(modelVoltages, 0.f, 3.f);

					channelModes[channel] = static_cast<bumps::Generator::FeatureMode>(modelVoltages[0]);
					channelModes[channel + 1] = static_cast<bumps::Generator::FeatureMode>(modelVoltages[1]);
					channelModes[channel + 2] = static_cast<bumps::Generator::FeatureMode>(modelVoltages[2]);
					channelModes[channel + 3] = static_cast<bumps::Generator::FeatureMode>(modelVoltages[3]);
				}
			}

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				if ((channel < channelCount) && (lastFeatureModes[channel] != channelModes[channel])) {
					generators[channel].feature_mode_ = channelModes[channel];
					generators[channel].set_mode(lastModes[channel]);
					generators[channel].set_range(lastRanges[channel]);
					lastFeatureModes[channel] = channelModes[channel];
				}

				int currentLight = LIGHT_CHANNEL_1 + channel * 3;
				lights[currentLight + 0].setBrightnessSmooth(channel < channelCount &&
					(generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_RANDOM ||
						generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_HARMONIC), sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(channel < channelCount &&
					(generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_FUNCTION ||
						generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_HARMONIC), sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(channel < channelCount &&
					generators[channel].feature_mode_ == bumps::Generator::FEAT_MODE_SHEEP, sampleTime);
			}

			if (displayChannel >= channelCount) {
				displayChannel = channelCount - 1;
			}

			bumps::Generator::FeatureMode displayFeatureMode = generators[displayChannel].feature_mode_;
			bumps::GeneratorMode displayMode = generators[displayChannel].mode();
			bumps::GeneratorRange displayRange = generators[displayChannel].range();


			lights[LIGHT_MODE + 0].setBrightnessSmooth((displayMode == bumps::GENERATOR_MODE_AD) *
				kSanguineButtonLightValue, sampleTime);
			lights[LIGHT_MODE + 1].setBrightnessSmooth((displayMode == bumps::GENERATOR_MODE_AR) *
				kSanguineButtonLightValue, sampleTime);

			lights[LIGHT_RANGE + 0].setBrightnessSmooth((displayRange == bumps::GENERATOR_RANGE_LOW &&
				!bHaveExternalSync) * kSanguineButtonLightValue, sampleTime);
			lights[LIGHT_RANGE + 1].setBrightnessSmooth((displayRange == bumps::GENERATOR_RANGE_HIGH &&
				!bHaveExternalSync) * kSanguineButtonLightValue, sampleTime);

			if (samples[displayChannel].flags & bumps::FLAG_END_OF_ATTACK) {
				unipolarFlags[displayChannel] *= -1.f;
			}
			lights[LIGHT_PHASE + 0].setBrightnessSmooth(fmaxf(0.f, unipolarFlags[displayChannel]), sampleTime);
			lights[LIGHT_PHASE + 1].setBrightnessSmooth(fmaxf(0.f, -unipolarFlags[displayChannel]), sampleTime);

			lights[LIGHT_SYNC].setBrightnessSmooth((bHaveExternalSync && !(getSystemTimeMs() & 128)) *
				kSanguineButtonLightValue, sampleTime);

			lights[LIGHT_QUANTIZER1].setBrightnessSmooth(quantizers[displayChannel] & 1, sampleTime);
			lights[LIGHT_QUANTIZER2].setBrightnessSmooth(quantizers[displayChannel] & 2, sampleTime);
			lights[LIGHT_QUANTIZER3].setBrightnessSmooth(quantizers[displayChannel] & 4, sampleTime);

			displayModel = temulenti::displayModels[displayFeatureMode];

			switch (displayFeatureMode) {
			case bumps::Generator::FEAT_MODE_HARMONIC:
				paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[2];
				paramQuantities[PARAM_RANGE]->name = temulenti::modelRangeHeaders[1];
				break;
			case bumps::Generator::FEAT_MODE_SHEEP:
				paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[1];
				paramQuantities[PARAM_RANGE]->name = temulenti::modelRangeHeaders[0];
				break;
			default:
				paramQuantities[PARAM_MODE]->name = aestusCommon::modelModeHeaders[0];
				paramQuantities[PARAM_RANGE]->name = temulenti::modelRangeHeaders[0];
				break;
			}
		}
	}

	void onReset() override {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			generators[channel].set_mode(bumps::GENERATOR_MODE_LOOPING);
			generators[channel].set_range(bumps::GENERATOR_RANGE_MEDIUM);
			lastRanges[channel] = bumps::GENERATOR_RANGE_MEDIUM;
			lastModes[channel] = bumps::GENERATOR_MODE_LOOPING;
		}
		params[PARAM_MODEL].setValue(0.f);
	}

	void onRandomize() override {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			generators[channel].set_range(bumps::GeneratorRange(random::u32() % 3));
			generators[channel].set_mode(bumps::GeneratorMode(random::u32() % 3));
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "mode", static_cast<int>(selectedMode));
		setJsonInt(rootJ, "range", static_cast<int>(selectedRange));
		setJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);
		setJsonInt(rootJ, "displayChannel", displayChannel);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "mode", intValue)) {
			selectedMode = static_cast<bumps::GeneratorMode>(intValue);
		}

		if (getJsonInt(rootJ, "range", intValue)) {
			selectedRange = static_cast<bumps::GeneratorRange>(intValue);
		}

		getJsonBoolean(rootJ, "useCalibrationOffset", bUseCalibrationOffset);

		if (getJsonInt(rootJ, "displayChannel", intValue)) {
			displayChannel = intValue;
		}
	}

	void setModel(int modelNum) {
		params[PARAM_MODEL].setValue(modelNum);
	}

	void setMode(int modeNum) {
		selectedMode = static_cast<bumps::GeneratorMode>(modeNum);
	}

	void setRange(int rangeNum) {
		selectedRange = static_cast<bumps::GeneratorRange>(rangeNum);
	}

	void setQuantizer(int quantizerNum) {
		params[PARAM_QUANTIZER].setValue(quantizerNum);
	}
};

struct TemulentiWidget : SanguineModuleWidget {
	explicit TemulentiWidget(Temulenti* module) {
		setModule(module);

		moduleName = "temulenti";
		panelSize = sanguineThemes::SIZE_14;
		backplateColor = sanguineThemes::PLATE_RED;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* temulentiFrameBuffer = new FramebufferWidget();

		addChild(temulentiFrameBuffer);

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(5.171, 17.302), module, Temulenti::INPUT_MODEL));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(16.25, 17.302), module, Temulenti::PARAM_MODEL));

		SanguineTinyNumericDisplay* displayModel = new SanguineTinyNumericDisplay(1, module, 27.524, 17.302);
		displayModel->displayType = DISPLAY_STRING;
		temulentiFrameBuffer->addChild(displayModel);
		displayModel->fallbackString = temulenti::displayModels[0];

		if (module) {
			displayModel->values.displayText = &module->displayModel;
		}

		const float kFirstRowY = 16.307f;
		const float kSecondRowY = 18.297f;
		const float kDeltaX = 1.993;

		float currentX = 34.095f;

		const int kLightOffset = 8;

		for (int light = 0; light < kLightOffset; ++light) {
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, kFirstRowY),
				module, Temulenti::LIGHT_CHANNEL_1 + light * 3));
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, kSecondRowY),
				module, Temulenti::LIGHT_CHANNEL_1 + (light + kLightOffset) * 3));
			currentX += kDeltaX;
		}

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(54.875, 17.302), module, Temulenti::PARAM_QUANTIZER));
		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(65.946, 17.302), module, Temulenti::INPUT_QUANTIZER));

		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(52.477, 21.731), module, Temulenti::LIGHT_QUANTIZER1));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(54.875, 21.731), module, Temulenti::LIGHT_QUANTIZER2));
		addChild(createLightCentered<TinyLight<GreenLight>>(millimetersToPixelsVec(57.264, 21.731), module, Temulenti::LIGHT_QUANTIZER3));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(5.171, 29.512), module, Temulenti::INPUT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(20.888, 29.512), module,
			Temulenti::PARAM_MODE, Temulenti::LIGHT_MODE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(10.671, 38.54), module,
			Temulenti::PARAM_SYNC, Temulenti::LIGHT_SYNC));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(20.888, 38.54), module, Temulenti::LIGHT_PHASE));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(35.56, 38.54), module, Temulenti::PARAM_FREQUENCY));

		addParam(createParamCentered<Sanguine2PSRed>(millimetersToPixelsVec(59.142, 38.54), module, Temulenti::PARAM_FM));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(5.171, 47.569), module, Temulenti::INPUT_RANGE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(20.888, 47.569),
			module, Temulenti::PARAM_RANGE, Temulenti::LIGHT_RANGE));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(11.966, 65.455), module, Temulenti::PARAM_SHAPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(35.56, 65.455), module, Temulenti::PARAM_SLOPE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(59.142, 65.455), module, Temulenti::PARAM_SMOOTHNESS));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(11.966, 81.595), module, Temulenti::INPUT_SHAPE));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(35.56, 81.595), module, Temulenti::INPUT_SLOPE));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(59.142, 81.595), module, Temulenti::INPUT_SMOOTHNESS));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.665, 98.26), module, Temulenti::INPUT_TRIGGER));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(21.11, 98.26), module, Temulenti::INPUT_FREEZE));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(35.554, 98.26), module, Temulenti::INPUT_PITCH));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(49.998, 98.26), module, Temulenti::INPUT_FM));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(64.442, 98.26), module, Temulenti::INPUT_LEVEL));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(6.665, 111.643), module, Temulenti::INPUT_CLOCK));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(21.11, 111.643), module, Temulenti::OUTPUT_HIGH));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(35.554, 111.643), module, Temulenti::OUTPUT_LOW));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(49.998, 111.643), module, Temulenti::OUTPUT_UNI));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(64.442, 111.643), module, Temulenti::OUTPUT_BI));
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

		std::string rangeLabel;
		switch (module->selectedFeatureMode)
		{
		case bumps::Generator::FEAT_MODE_RANDOM:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[0], temulenti::drunksModeLabels,
				[=]() { return module->selectedMode; },
				[=](int i) { module->setMode(i); }
			));
			rangeLabel = temulenti::modelRangeHeaders[0];
			break;
		case bumps::Generator::FEAT_MODE_HARMONIC:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[2], temulenti::bumpsModeLabels,
				[=]() { return module->selectedMode; },
				[=](int i) { module->setMode(i); }
			));
			rangeLabel = temulenti::modelRangeHeaders[1];
			break;
		case bumps::Generator::FEAT_MODE_SHEEP:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[1], aestusCommon::sheepMenuLabels,
				[=]() { return module->selectedMode; },
				[=](int i) { module->setMode(i); }
			));
			rangeLabel = temulenti::modelRangeHeaders[0];
			break;
		default:
			menu->addChild(createIndexSubmenuItem(aestusCommon::modelModeHeaders[0], aestusCommon::modeMenuLabels,
				[=]() { return module->selectedMode; },
				[=](int i) { module->setMode(i); }
			));
			rangeLabel = temulenti::modelRangeHeaders[0];
			break;
		}

		menu->addChild(createIndexSubmenuItem(rangeLabel, aestusCommon::rangeMenuLabels,
			[=]() { return module->selectedRange; },
			[=](int i) { module->setRange(i); }
		));

		menu->addChild(createIndexSubmenuItem("Quantizer scale", temulenti::quantizerLabels,
			[=]() { return module->params[Temulenti::PARAM_QUANTIZER].getValue(); },
			[=](int i) { module->setQuantizer(i); }
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
};

Model* modelTemulenti = createModel<Temulenti, TemulentiWidget>("Sanguine-Temulenti");