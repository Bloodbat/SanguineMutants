#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "rings/dsp/part.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "array"

#include "anuli.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Anuli : SanguineModule {
	enum ParamIds {
		PARAM_FREQUENCY,
		PARAM_STRUCTURE,
		PARAM_BRIGHTNESS,
		PARAM_DAMPING,
		PARAM_POSITION,

		PARAM_BRIGHTNESS_MOD,
		PARAM_FREQUENCY_MOD,
		PARAM_DAMPING_MOD,
		PARAM_STRUCTURE_MOD,
		PARAM_POSITION_MOD,

		PARAM_MODE,
		PARAM_POLYPHONY,
		PARAM_FX,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_BRIGHTNESS_CV,
		INPUT_FREQUENCY_CV,
		INPUT_DAMPING_CV,
		INPUT_STRUCTURE_CV,
		INPUT_POSITION_CV,

		INPUT_STRUM,
		INPUT_PITCH,
		INPUT_IN,
		INPUT_MODE,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_ODD,
		OUTPUT_EVEN,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_POLYPHONY, 2),
		ENUMS(LIGHT_RESONATOR, PORT_MAX_CHANNELS * 3),
		ENUMS(LIGHT_FX, 2),
		LIGHTS_COUNT
	};

	dsp::SampleRateConverter<1> inputSrc[PORT_MAX_CHANNELS];
	dsp::SampleRateConverter<2> outputSrc[PORT_MAX_CHANNELS];
	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> inputBuffer[PORT_MAX_CHANNELS];
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer[PORT_MAX_CHANNELS];

	dsp::ClockDivider lightsDivider;

	uint16_t reverbBuffer[PORT_MAX_CHANNELS][32768] = {};
	rings::Part part[PORT_MAX_CHANNELS];
	rings::StringSynthPart stringSynth[PORT_MAX_CHANNELS];
	rings::Strummer strummer[PORT_MAX_CHANNELS];

	bool bStrum[PORT_MAX_CHANNELS] = {};
	bool bLastStrum[PORT_MAX_CHANNELS] = {};

	bool bNotesModeSelection = false;

	bool bUseFrequencyOffset = true;

	int channelCount = 0;
	int polyphonyMode = 1;
	int strummingFlagCounter = 0;
	int strummingFlagInterval = 0;

	int displayChannel = 0;

	std::array<int, PORT_MAX_CHANNELS> channelModes = {};

	rings::ResonatorModel resonatorModel[PORT_MAX_CHANNELS] = {};
	rings::FxType fxModel = rings::FX_FORMANT;

	std::string displayText = "";

	static const int kLightsFrequency = 64;

	struct ParameterInfo {
		float structureMod;
		float brightnessMod;
		float dampingMod;
		float positionMod;

		float frequencyMod;

		bool useInternalExciter;
		bool useInternalStrum;
		bool useInternalNote;
	};

	Anuli() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODE, 0.f, 6.f, 0.f, "Resonator model", anuliMenuLabels);

		configSwitch(PARAM_FX, 0.f, 5.f, 0.f, "Disastrous peace FX", anuliFxLabels);

		configParam(PARAM_POLYPHONY, 1.f, 4.f, 1.f, "Note polyphony");
		paramQuantities[PARAM_POLYPHONY]->snapEnabled = true;

		configParam(PARAM_FREQUENCY, 0.f, 60.f, 30.f, "Frequency");
		configParam(PARAM_STRUCTURE, 0.f, 1.f, 0.5f, "Structure");
		configParam(PARAM_BRIGHTNESS, 0.f, 1.f, 0.5f, "Brightness");
		configParam(PARAM_DAMPING, 0.f, 1.f, 0.5f, "Damping");
		configParam(PARAM_POSITION, 0.f, 1.f, 0.5f, "Position");
		configParam(PARAM_BRIGHTNESS_MOD, -1.f, 1.f, 0.f, "Brightness CV");
		configParam(PARAM_FREQUENCY_MOD, -1.f, 1.f, 0.f, "Frequency CV");
		configParam(PARAM_DAMPING_MOD, -1.f, 1.f, 0.f, "Damping CV");
		configParam(PARAM_STRUCTURE_MOD, -1.f, 1.f, 0.f, "Structure CV");
		configParam(PARAM_POSITION_MOD, -1.f, 1.f, 0.f, "Position CV");

		configInput(INPUT_BRIGHTNESS_CV, "Brightness");
		configInput(INPUT_FREQUENCY_CV, "Frequency");
		configInput(INPUT_DAMPING_CV, "Damping");
		configInput(INPUT_STRUCTURE_CV, "Structure");
		configInput(INPUT_POSITION_CV, "Position");
		configInput(INPUT_STRUM, "Strum");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");
		configInput(INPUT_IN, "Audio");

		configInput(INPUT_MODE, "Mode");

		configOutput(OUTPUT_ODD, "Odd");
		configOutput(OUTPUT_EVEN, "Even");

		configBypass(INPUT_IN, OUTPUT_ODD);
		configBypass(INPUT_IN, OUTPUT_EVEN);

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&strummer[channel], 0, sizeof(rings::Strummer));
			memset(&part[channel], 0, sizeof(rings::Part));
			memset(&stringSynth[channel], 0, sizeof(rings::StringSynthPart));
			strummer[channel].Init(0.01f, 44100.f / kAnuliBlockSize);
			part[channel].Init(reverbBuffer[channel]);
			stringSynth[channel].Init(reverbBuffer[channel]);
		}

		lightsDivider.setDivision(kLightsFrequency);
	}

	void process(const ProcessArgs& args) override {
		bool bIsLightsTurn = lightsDivider.process();

		bool bIsTrianglePulse = false;

		bool bWithDisastrousPeace = false;

		long long systemTimeMs;

		float sampleTime = 0.f;

		if (bIsLightsTurn) {
			sampleTime = kLightsFrequency * args.sampleTime;

			systemTimeMs = getSystemTimeMs();

			uint8_t pulseWidthModulationCounter = systemTimeMs & 15;
			uint8_t triangle = (systemTimeMs >> 5) & 31;
			triangle = triangle < 16 ? triangle : 31 - triangle;
			bIsTrianglePulse = pulseWidthModulationCounter < triangle;
		}

		channelCount = std::max(std::max(std::max(inputs[INPUT_STRUM].getChannels(), inputs[INPUT_PITCH].getChannels()),
			inputs[INPUT_IN].getChannels()), 1);

		polyphonyMode = params[PARAM_POLYPHONY].getValue();

		fxModel = static_cast<rings::FxType>(params[PARAM_FX].getValue());

		channelModes.fill(static_cast<int>(params[PARAM_MODE].getValue()));

		ParameterInfo parameterInfo = {};

		parameterInfo.structureMod = dsp::quadraticBipolar(params[PARAM_STRUCTURE_MOD].getValue());
		parameterInfo.brightnessMod = dsp::quadraticBipolar(params[PARAM_BRIGHTNESS_MOD].getValue());
		parameterInfo.dampingMod = dsp::quadraticBipolar(params[PARAM_DAMPING_MOD].getValue());
		parameterInfo.positionMod = dsp::quadraticBipolar(params[PARAM_POSITION_MOD].getValue());

		parameterInfo.frequencyMod = dsp::quarticBipolar(params[PARAM_FREQUENCY_MOD].getValue());

		parameterInfo.useInternalExciter = !inputs[INPUT_IN].isConnected();
		parameterInfo.useInternalStrum = !inputs[INPUT_STRUM].isConnected();
		parameterInfo.useInternalNote = !inputs[INPUT_PITCH].isConnected();

		bool bHaveBothOutputs = outputs[OUTPUT_ODD].isConnected() && outputs[OUTPUT_EVEN].isConnected();
		bool bHaveModeCable = inputs[INPUT_MODE].isConnected();
		bool bChannelIsEasterEgg = false;

		if (bHaveModeCable) {
			if (bNotesModeSelection) {
				for (int channel = 0; channel < channelCount; ++channel) {
					float modeVoltage = inputs[INPUT_MODE].getVoltage(channel);
					modeVoltage = roundf(modeVoltage * 12.f);
					channelModes[channel] = clamp(static_cast<int>(modeVoltage), 0, 6);

					setupChannel(channel, bWithDisastrousPeace, bChannelIsEasterEgg);

					renderFrames(channel, parameterInfo, bChannelIsEasterEgg, args.sampleRate);

					setOutputs(channel, bHaveBothOutputs);

					drawChannelLight(channel, bIsTrianglePulse, bIsLightsTurn, sampleTime);
				}
			} else {
				for (int channel = 0; channel < channelCount; ++channel) {
					float modeVoltage = inputs[INPUT_MODE].getVoltage(channel);
					channelModes[channel] = clamp(static_cast<int>(modeVoltage), 0, 6);

					setupChannel(channel, bWithDisastrousPeace, bChannelIsEasterEgg);

					renderFrames(channel, parameterInfo, bChannelIsEasterEgg, args.sampleRate);

					setOutputs(channel, bHaveBothOutputs);

					drawChannelLight(channel, bIsTrianglePulse, bIsLightsTurn, sampleTime);
				}
			}
		} else {
			for (int channel = 0; channel < channelCount; ++channel) {
				setupChannel(channel, bWithDisastrousPeace, bChannelIsEasterEgg);

				renderFrames(channel, parameterInfo, bChannelIsEasterEgg, args.sampleRate);

				setOutputs(channel, bHaveBothOutputs);

				drawChannelLight(channel, bIsTrianglePulse, bIsLightsTurn, sampleTime);
			}
		}

		outputs[OUTPUT_ODD].setChannels(channelCount);

		outputs[OUTPUT_EVEN].setChannels(channelCount);

		if (bIsLightsTurn) {
			if (displayChannel >= channelCount) {
				displayChannel = channelCount - 1;
			}

			displayText = anuliModeLabels[channelModes[displayChannel]];

			for (int channel = channelCount; channel < PORT_MAX_CHANNELS; ++channel) {
				const int currentLight = LIGHT_RESONATOR + channel * 3;

				for (int light = 0; light < 3; ++light) {
					lights[currentLight + light].setBrightnessSmooth(0.f, sampleTime);
				}
			}

			if (bWithDisastrousPeace) {
				for (int light = 0; light < 2; ++light) {
					drawLight(LIGHT_FX + light, anuliFxModeLights[static_cast<int>(fxModel)][light], bIsTrianglePulse, sampleTime);
				}
			} else {
				for (int light = 0; light < 2; ++light) {
					lights[LIGHT_FX + light].setBrightnessSmooth(0.f, sampleTime);
				}
			}

			lights[LIGHT_POLYPHONY + 0].setBrightness(polyphonyMode <= 3 ? 1.f : 0.f);
			lights[LIGHT_POLYPHONY + 1].setBrightness((polyphonyMode != 3 && polyphonyMode & 0x06) ||
				(polyphonyMode == 3 && bIsTrianglePulse) ? 1.f : 0.f);

			++strummingFlagInterval;
			if (strummingFlagCounter) {
				--strummingFlagCounter;
				lights[LIGHT_POLYPHONY + 0].setBrightness(0.f);
				lights[LIGHT_POLYPHONY + 1].setBrightness(0.f);
			}
		}
	}

	void setupPatch(const int channel, rings::Patch& patch, float& structure, const ParameterInfo& parameterInfo) {
		using simd::float_4;

		float_4 voltages;

		voltages[0] = inputs[INPUT_STRUCTURE_CV].getVoltage(channel);
		voltages[1] = inputs[INPUT_BRIGHTNESS_CV].getVoltage(channel);
		voltages[2] = inputs[INPUT_DAMPING_CV].getVoltage(channel);
		voltages[3] = inputs[INPUT_POSITION_CV].getVoltage(channel);

		voltages /= 5.f;
		voltages *= 3.3f;

		structure = params[PARAM_STRUCTURE].getValue() + parameterInfo.structureMod * voltages[0];
		patch.structure = clamp(structure, 0.f, 0.9995f);
		patch.brightness = clamp(params[PARAM_BRIGHTNESS].getValue() + parameterInfo.brightnessMod * voltages[1], 0.f, 1.f);
		patch.damping = clamp(params[PARAM_DAMPING].getValue() + parameterInfo.dampingMod * voltages[2], 0.f, 0.9995f);
		patch.position = clamp(params[PARAM_POSITION].getValue() + parameterInfo.positionMod * voltages[3], 0.f, 0.9995f);
	}

	void setupPerformance(const int channel, rings::PerformanceState& performanceState, const float structure,
		const ParameterInfo& parameterInfo) {
		float note = inputs[INPUT_PITCH].getVoltage(channel) +
			anuliFrequencyOffsets[static_cast<int>(bUseFrequencyOffset)];
		performanceState.note = 12.f * note;

		float transpose = params[PARAM_FREQUENCY].getValue();
		// Quantize transpose if pitch input is connected
		if (inputs[INPUT_PITCH].isConnected()) {
			transpose = roundf(transpose);
		}
		performanceState.tonic = 12.f + clamp(transpose, 0.f, 60.f);

		performanceState.fm = clamp(48.f * 3.3f * parameterInfo.frequencyMod *
			inputs[INPUT_FREQUENCY_CV].getNormalVoltage(kAnuliVoltPerOctave, channel) / 5.f, -48.f, 48.f);

		performanceState.internal_exciter = parameterInfo.useInternalExciter;
		performanceState.internal_strum = parameterInfo.useInternalStrum;
		performanceState.internal_note = parameterInfo.useInternalNote;

		// TODO: "Normalized to a step detector on the V/OCT input and a transient detector on the IN input."
		performanceState.strum = bStrum[channel] && !bLastStrum[channel];
		bLastStrum[channel] = bStrum[channel];
		bStrum[channel] = false;
		if (channel == 0) {
			setStrummingFlag(performanceState.strum);
		}

		performanceState.chord = clamp(static_cast<int>(roundf(structure * (rings::kNumChords - 1))),
			0, rings::kNumChords - 1);
	}

	void setOutputs(const int channel, const bool withBothOutputs) {
		if (!outputBuffer[channel].empty()) {
			dsp::Frame<2> outputFrame = outputBuffer[channel].shift();
			/* "Note: you need to insert a jack into each output to split the signals:
				when only one jack is inserted, both signals are mixed together." */
			if (withBothOutputs) {
				outputs[OUTPUT_ODD].setVoltage(clamp(outputFrame.samples[0], -1.f, 1.f) * 5.f, channel);
				outputs[OUTPUT_EVEN].setVoltage(clamp(outputFrame.samples[1], -1.f, 1.f) * 5.f, channel);
			} else {
				float outVoltage = clamp(outputFrame.samples[0] + outputFrame.samples[1], -1.f, 1.f) * 5.f;
				outputs[OUTPUT_ODD].setVoltage(outVoltage, channel);
				outputs[OUTPUT_EVEN].setVoltage(outVoltage, channel);
			}
		}
	}

	void setupChannel(const int channel, bool& haveDisastrousPeace, bool& isEasterEgg) {
		isEasterEgg = channelModes[channel] > 5;

		haveDisastrousPeace = haveDisastrousPeace || isEasterEgg;

		resonatorModel[channel] = static_cast<rings::ResonatorModel>(channelModes[channel]);

		// TODO: "Normalized to a pulse/burst generator that reacts to note changes on the V/OCT input."
		if (!inputBuffer[channel].full()) {
			dsp::Frame<1> frame;
			frame.samples[0] = inputs[INPUT_IN].getVoltage(channel) / 5.f;
			inputBuffer[channel].push(frame);
		}

		if (!bStrum[channel]) {
			bStrum[channel] = inputs[INPUT_STRUM].getVoltage(channel) >= 1.f;
		}
	}

	void renderFrames(const int channel, const ParameterInfo& parameterInfo, const bool isEasterEgg, const float sampleRate) {
		if (outputBuffer[channel].empty()) {
			float in[kAnuliBlockSize] = {};

			// Convert input buffer
			inputSrc[channel].setRates(static_cast<int>(sampleRate), 48000);
			int inLen = inputBuffer[channel].size();
			int outLen = kAnuliBlockSize;
			inputSrc[channel].process(inputBuffer[channel].startData(), &inLen,
				reinterpret_cast<dsp::Frame<1>*>(in), &outLen);
			inputBuffer[channel].startIncr(inLen);

			float out[kAnuliBlockSize];
			float aux[kAnuliBlockSize];

			if (isEasterEgg) {
				stringSynth[channel].set_polyphony(polyphonyMode);

				stringSynth[channel].set_fx(rings::FxType(fxModel));

				rings::Patch patch;
				float structure;
				setupPatch(channel, patch, structure, parameterInfo);
				rings::PerformanceState performanceState;
				setupPerformance(channel, performanceState, structure, parameterInfo);

				// Process audio
				strummer[channel].Process(NULL, kAnuliBlockSize, &performanceState);
				stringSynth[channel].Process(performanceState, patch, in, out, aux, kAnuliBlockSize);
			} else {
				if (part[channel].polyphony() != polyphonyMode) {
					part[channel].set_polyphony(polyphonyMode);
				}

				part[channel].set_model(resonatorModel[channel]);

				rings::Patch patch;
				float structure;
				setupPatch(channel, patch, structure, parameterInfo);
				rings::PerformanceState performanceState;
				setupPerformance(channel, performanceState, structure, parameterInfo);

				// Process audio
				strummer[channel].Process(in, kAnuliBlockSize, &performanceState);
				part[channel].Process(performanceState, patch, in, out, aux, kAnuliBlockSize);
			}

			// Convert output buffer
			dsp::Frame<2> outputFrames[kAnuliBlockSize];
			for (int frame = 0; frame < kAnuliBlockSize; ++frame) {
				outputFrames[frame].samples[0] = out[frame];
				outputFrames[frame].samples[1] = aux[frame];
			}

			outputSrc[channel].setRates(48000, static_cast<int>(sampleRate));
			int inCount = kAnuliBlockSize;
			int outCount = outputBuffer[channel].capacity();
			outputSrc[channel].process(outputFrames, &inCount, outputBuffer[channel].endData(), &outCount);
			outputBuffer[channel].endIncr(outCount);
		}
	}

	void drawChannelLight(const int channel, const bool trianglePulse, const bool isLightsTurn, const float sampleTime) {
		if (isLightsTurn) {
			const int currentLight = LIGHT_RESONATOR + channel * 3;

			for (int light = 0; light < 3; ++light) {
				LightModes lightMode = anuliModeLights[channelModes[channel]][light];
				drawLight(currentLight + light, lightMode, trianglePulse, sampleTime);
			}
		}
	}

	void setStrummingFlag(bool flag) {
		if (flag) {
			// Make sure the LED is off for a short enough time (ui.cc).
			strummingFlagCounter = std::min(50, strummingFlagInterval >> 2);
			strummingFlagInterval = 0;
		}
	}

	void drawLight(const int lightNum, const LightModes lightMode, const bool trianglePulse, const float sampleTime) {
		float lightValue;

		switch (lightMode) {
		case LIGHT_ON:
			lightValue = 1.f;
			break;

		case LIGHT_BLINK:
			lightValue = static_cast<float>(trianglePulse);
			break;

		default:
			lightValue = 0.f;
			break;
		}

		lights[lightNum].setBrightnessSmooth(lightValue, sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "NotesModeSelection", json_boolean(bNotesModeSelection));

		json_object_set_new(rootJ, "displayChannel", json_integer(displayChannel));

		json_object_set_new(rootJ, "useFrequencyOffset", json_boolean(bUseFrequencyOffset));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		if (json_t* notesModeSelectionJ = json_object_get(rootJ, "NotesModeSelection")) {
			bNotesModeSelection = json_boolean_value(notesModeSelectionJ);
		}

		json_t* displayChannelJ = json_object_get(rootJ, "displayChannel");
		if (displayChannelJ) {
			displayChannel = json_integer_value(displayChannelJ);
		}

		if (json_t* useFrequencyOffsetJ = json_object_get(rootJ, "useFrequencyOffset")) {
			bUseFrequencyOffset = json_boolean_value(useFrequencyOffsetJ);
		}
	}

	void setMode(int modeNum) {
		channelModes[0] = modeNum;
		resonatorModel[0] = modeNum < 6 ? rings::ResonatorModel(modeNum) : resonatorModel[0];
		params[PARAM_MODE].setValue(modeNum);
	}
};

struct AnuliWidget : SanguineModuleWidget {
	AnuliWidget(Anuli* module) {
		setModule(module);

		moduleName = "anuli";
		panelSize = SIZE_21;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		const float xDelta = 3.71f;
		const int lightIdOffset = 8;

		float currentXA = 23.989f;
		float currentXB = 56.725f;

		for (int component = 0; component < 8; ++component) {
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentXA, 14.973),
				module, Anuli::LIGHT_RESONATOR + component * 3));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentXB, 14.973),
				module, Anuli::LIGHT_RESONATOR + ((component + lightIdOffset) * 3)));
			currentXA += xDelta;
			currentXB += xDelta;
		}


		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(9.021, 22.087), module, Anuli::INPUT_MODE));

		FramebufferWidget* anuliFrambuffer = new FramebufferWidget();
		addChild(anuliFrambuffer);

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 53.34f, 22.087f);
		anuliFrambuffer->addChild(displayModel);
		displayModel->fallbackString = anuliModeLabels[0];

		addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(98.297, 22.087), module, Anuli::PARAM_MODE));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.383, 35.904), module, Anuli::INPUT_FREQUENCY_CV));

		SanguineTinyNumericDisplay* displayPolyphony = new SanguineTinyNumericDisplay(2, module, 53.34f, 37.486f);
		anuliFrambuffer->addChild(displayPolyphony);
		displayPolyphony->fallbackNumber = 1;

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(98.297, 35.904), module, Anuli::INPUT_STRUCTURE_CV));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.415, 42.833), module, Anuli::PARAM_FREQUENCY_MOD));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(33.006, 49.715), module, Anuli::PARAM_FREQUENCY));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(53.34f, 30.245f), module, Anuli::LIGHT_POLYPHONY));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(53.34, 54.784), module, Anuli::PARAM_POLYPHONY));

		addParam(createParamCentered<Sanguine3PSGreen>(millimetersToPixelsVec(73.674, 49.715), module, Anuli::PARAM_STRUCTURE));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(87.986, 42.833), module, Anuli::PARAM_STRUCTURE_MOD));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(33.006, 72.385), module, Anuli::PARAM_BRIGHTNESS));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(53.34, 70.654),
			module, Anuli::PARAM_FX, Anuli::LIGHT_FX));

		addParam(createParamCentered<Sanguine1PSYellow>(millimetersToPixelsVec(73.674, 72.385), module, Anuli::PARAM_POSITION));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.415, 81.324), module, Anuli::PARAM_BRIGHTNESS_MOD));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(87.986, 81.324), module, Anuli::PARAM_POSITION_MOD));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.383, 86.197), module, Anuli::INPUT_BRIGHTNESS_CV));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(53.34, 84.417), module, Anuli::PARAM_DAMPING));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(98.297, 86.197), module, Anuli::INPUT_POSITION_CV));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 32.288, 101.019);
		addChild(bloodLogo);

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(53.15, 101.964), module, Anuli::PARAM_DAMPING_MOD));

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 94.721, 99.605);
		addChild(mutantsLogo);

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(53.34, 112.736), module, Anuli::INPUT_DAMPING_CV));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(8.728, 116.807), module, Anuli::INPUT_STRUM));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(22.58, 116.807), module, Anuli::INPUT_PITCH));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(36.382, 116.807), module, Anuli::INPUT_IN));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(84.046, 116.807), module, Anuli::OUTPUT_ODD));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(97.898, 116.807), module, Anuli::OUTPUT_EVEN));

		if (module) {
			displayModel->values.displayText = &module->displayText;
			displayPolyphony->values.numberValue = &module->polyphonyMode;
		}
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Anuli* module = dynamic_cast<Anuli*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Mode", anuliMenuLabels,
			[=]() { return module->params[Anuli::PARAM_MODE].getValue(); },
			[=](int i) { module->setMode(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Disastrous Peace FX", anuliFxLabels,
			[=]() { return module->params[Anuli::PARAM_FX].getValue(); },
			[=](int i) { module->params[Anuli::PARAM_FX].setValue(i); }
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

				menu->addChild(new MenuSeparator);

				menu->addChild(createBoolPtrMenuItem("C4-F#4 direct mode selection", "", &module->bNotesModeSelection));
			}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Compatibility options", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("Frequency knob center is C", "", &module->bUseFrequencyOffset));
			}
		));
	}
};

Model* modelAnuli = createModel<Anuli, AnuliWidget>("Sanguine-Anuli");