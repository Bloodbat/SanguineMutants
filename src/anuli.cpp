#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "sanguinejson.hpp"

#include "rings/dsp/part.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"

#include "array"

#include "anuli.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

using namespace sanguineCommonCode;

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

	dsp::SampleRateConverter<1> srcInputs[PORT_MAX_CHANNELS];
	dsp::SampleRateConverter<2> srcOutputs[PORT_MAX_CHANNELS];
	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> drbInputBuffers[PORT_MAX_CHANNELS];
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> drbOutputBuffers[PORT_MAX_CHANNELS];

	dsp::ClockDivider lightsDivider;

	uint16_t reverbBuffers[PORT_MAX_CHANNELS][32768] = {};
	rings::Part parts[PORT_MAX_CHANNELS];
	rings::StringSynthPart stringSynths[PORT_MAX_CHANNELS];
	rings::Strummer strummers[PORT_MAX_CHANNELS];
	rings::PerformanceState performanceStates[PORT_MAX_CHANNELS] = {};

	bool strums[PORT_MAX_CHANNELS] = {};
	bool lastStrums[PORT_MAX_CHANNELS] = {};

	bool bNotesModeSelection = false;

	bool bUseFrequencyOffset = true;

	int channelCount = 0;
	int polyphonyMode = 1;
	int strummingFlagCounter = 0;
	int strummingFlagInterval = 0;

	int displayChannel = 0;

	std::array<int, PORT_MAX_CHANNELS> channelModes = {};

	rings::ResonatorModel resonatorModels[PORT_MAX_CHANNELS] = {};
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

		configSwitch(PARAM_MODE, 0.f, 6.f, 0.f, "Resonator model", anuli::menuModeLabels);

		configSwitch(PARAM_FX, 0.f, 5.f, 0.f, "Disastrous peace FX", anuli::fxLabels);

		configParam(PARAM_POLYPHONY, 1.f, 4.f, 1.f, "Note polyphony");
		paramQuantities[PARAM_POLYPHONY]->snapEnabled = true;

		configParam(PARAM_FREQUENCY, 0.f, 60.f, 30.f, "Frequency", " semitones", 0.f, 1.f, -30.f);
		configParam(PARAM_STRUCTURE, 0.f, 1.f, 0.5f, "Structure", "%", 0.f, 100.f);
		configParam(PARAM_BRIGHTNESS, 0.f, 1.f, 0.5f, "Brightness", "%", 0.f, 100.f);
		configParam(PARAM_DAMPING, 0.f, 1.f, 0.5f, "Damping", "%", 0.f, 100.f);
		configParam(PARAM_POSITION, 0.f, 1.f, 0.5f, "Position", "%", 0.f, 100.f);
		configParam(PARAM_BRIGHTNESS_MOD, -1.f, 1.f, 0.f, "Brightness CV", "%", 0.f, 100.f);
		configParam(PARAM_FREQUENCY_MOD, -1.f, 1.f, 0.f, "Frequency CV", "%", 0.f, 100.f);
		configParam(PARAM_DAMPING_MOD, -1.f, 1.f, 0.f, "Damping CV", "%", 0.f, 100.f);
		configParam(PARAM_STRUCTURE_MOD, -1.f, 1.f, 0.f, "Structure CV", "%", 0.f, 100.f);
		configParam(PARAM_POSITION_MOD, -1.f, 1.f, 0.f, "Position CV", "%", 0.f, 100.f);

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
			memset(&strummers[channel], 0, sizeof(rings::Strummer));
			memset(&parts[channel], 0, sizeof(rings::Part));
			memset(&stringSynths[channel], 0, sizeof(rings::StringSynthPart));
			strummers[channel].Init(0.01f, 44100.f / anuli::kBlockSize);
			parts[channel].Init(reverbBuffers[channel]);
			stringSynths[channel].Init(reverbBuffers[channel]);
		}

		lightsDivider.setDivision(kLightsFrequency);
	}

	void process(const ProcessArgs& args) override {
		bool bWithDisastrousPeace = false;

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

		if (bHaveModeCable) {
			if (bNotesModeSelection) {
				for (int channel = 0; channel < channelCount; ++channel) {
					float modeVoltage = inputs[INPUT_MODE].getVoltage(channel);
					modeVoltage = roundf(modeVoltage * 12.f);
					channelModes[channel] = clamp(static_cast<int>(modeVoltage), 0, 6);

					setupChannel(channel, bWithDisastrousPeace);

					renderFrames(channel, parameterInfo, args.sampleRate);

					setOutputs(channel, bHaveBothOutputs);
				}
			} else {
				for (int channel = 0; channel < channelCount; ++channel) {
					float modeVoltage = inputs[INPUT_MODE].getVoltage(channel);
					channelModes[channel] = clamp(static_cast<int>(modeVoltage), 0, 6);

					setupChannel(channel, bWithDisastrousPeace);

					renderFrames(channel, parameterInfo, args.sampleRate);

					setOutputs(channel, bHaveBothOutputs);
				}
			}
		} else {
			for (int channel = 0; channel < channelCount; ++channel) {
				setupChannel(channel, bWithDisastrousPeace);

				renderFrames(channel, parameterInfo, args.sampleRate);

				setOutputs(channel, bHaveBothOutputs);
			}
		}

		setStrummingFlag(performanceStates[displayChannel].strum);

		outputs[OUTPUT_ODD].setChannels(channelCount);

		outputs[OUTPUT_EVEN].setChannels(channelCount);

		if (lightsDivider.process()) {
			const float sampleTime = kLightsFrequency * args.sampleTime;

			long long systemTimeMs = getSystemTimeMs();

			uint8_t pulseWidthModulationCounter = systemTimeMs & 15;
			uint8_t triangle = (systemTimeMs >> 5) & 31;
			triangle = triangle < 16 ? triangle : 31 - triangle;
			bool bIsTrianglePulse = pulseWidthModulationCounter < triangle;

			if (displayChannel >= channelCount) {
				displayChannel = channelCount - 1;
			}

			displayText = anuli::modeLabels[channelModes[displayChannel]];

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				const int currentLight = LIGHT_RESONATOR + channel * 3;

				for (int light = 0; light < 3; ++light) {
					LightModes lightMode = anuli::modeLights[channelModes[channel]][light];

					float lightValue = static_cast<float>(channel < channelCount &&
						(lightMode == LIGHT_ON || (lightMode == LIGHT_BLINK && bIsTrianglePulse)));

					lights[currentLight + light].setBrightnessSmooth(lightValue, sampleTime);
				}
			}

			for (int light = 0; light < 2; ++light) {
				float lightValue = bWithDisastrousPeace && (anuli::fxModeLights[static_cast<int>(fxModel)][light] == LIGHT_ON ||
					(anuli::fxModeLights[static_cast<int>(fxModel)][light] == LIGHT_BLINK && bIsTrianglePulse)) ?
					kSanguineButtonLightValue : 0.f;
				lights[LIGHT_FX + light].setBrightnessSmooth(lightValue, sampleTime);
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

		patch.brightness = clamp(params[PARAM_BRIGHTNESS].getValue() + parameterInfo.brightnessMod * voltages[1], 0.f, 1.f);

		structure = params[PARAM_STRUCTURE].getValue() + parameterInfo.structureMod * voltages[0];

		voltages[0] = structure;
		voltages[2] = params[PARAM_DAMPING].getValue() + parameterInfo.dampingMod * voltages[2];
		voltages[3] = params[PARAM_POSITION].getValue() + parameterInfo.positionMod * voltages[3];

		voltages = simd::clamp(voltages, 0.f, 0.9995f);

		patch.structure = voltages[0];
		patch.damping = voltages[2];
		patch.position = voltages[3];
	}

	void setupPerformance(const int channel, rings::PerformanceState& performanceState, const float structure,
		const ParameterInfo& parameterInfo) {
		float note = std::fmaxf(inputs[INPUT_PITCH].getVoltage(channel), -6.f) +
			anuli::frequencyOffsets[static_cast<int>(bUseFrequencyOffset)];
		performanceState.note = 12.f * note;

		float transpose = params[PARAM_FREQUENCY].getValue();
		// Quantize transpose if pitch input is connected
		if (inputs[INPUT_PITCH].isConnected()) {
			transpose = roundf(transpose);
		}
		performanceState.tonic = 12.f + clamp(transpose, 0.f, 60.f);

		performanceState.fm = clamp(48.f * 3.3f * parameterInfo.frequencyMod *
			inputs[INPUT_FREQUENCY_CV].getNormalVoltage(anuli::kVoltPerOctave, channel) / 5.f, -48.f, 48.f);

		performanceState.internal_exciter = parameterInfo.useInternalExciter;
		performanceState.internal_strum = parameterInfo.useInternalStrum;
		performanceState.internal_note = parameterInfo.useInternalNote;

		// TODO: "Normalized to a step detector on the V/OCT input and a transient detector on the IN input."
		performanceState.strum = strums[channel] && !lastStrums[channel];
		lastStrums[channel] = strums[channel];
		strums[channel] = false;

		performanceState.chord = clamp(static_cast<int>(roundf(structure * (rings::kNumChords - 1))),
			0, rings::kNumChords - 1);
	}

	void setOutputs(const int channel, const bool withBothOutputs) {
		if (!drbOutputBuffers[channel].empty()) {
			dsp::Frame<2> outputFrame = drbOutputBuffers[channel].shift();
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

	void setupChannel(const int channel, bool& haveDisastrousPeace) {
		bool bIsEasterEgg = channelModes[channel] > 5;

		haveDisastrousPeace = haveDisastrousPeace || bIsEasterEgg;

		resonatorModels[channel] = bIsEasterEgg ? rings::RESONATOR_MODEL_MODAL :
			static_cast<rings::ResonatorModel>(channelModes[channel]);

		// TODO: "Normalized to a pulse/burst generator that reacts to note changes on the V/OCT input."
		if (!drbInputBuffers[channel].full()) {
			dsp::Frame<1> frame;
			frame.samples[0] = inputs[INPUT_IN].getVoltage(channel) / 5.f;
			drbInputBuffers[channel].push(frame);
		}

		if (!strums[channel]) {
			strums[channel] = inputs[INPUT_STRUM].getVoltage(channel) >= 1.f;
		}
	}

	void renderFrames(const int channel, const ParameterInfo& parameterInfo, const float sampleRate) {
		if (drbOutputBuffers[channel].empty()) {
			float in[anuli::kBlockSize] = {};

			// Convert input buffer
			srcInputs[channel].setRates(static_cast<int>(sampleRate), 48000);
			int inLen = drbInputBuffers[channel].size();
			int outLen = anuli::kBlockSize;
			srcInputs[channel].process(drbInputBuffers[channel].startData(), &inLen,
				reinterpret_cast<dsp::Frame<1>*>(in), &outLen);
			drbInputBuffers[channel].startIncr(inLen);

			float out[anuli::kBlockSize];
			float aux[anuli::kBlockSize];

			rings::Patch patch;
			float structure;

			switch (channelModes[channel]) {
			case 6: // Disastrous peace.
				stringSynths[channel].set_polyphony(polyphonyMode);

				stringSynths[channel].set_fx(rings::FxType(fxModel));

				setupPatch(channel, patch, structure, parameterInfo);
				setupPerformance(channel, performanceStates[channel], structure, parameterInfo);

				// Process audio
				strummers[channel].Process(NULL, anuli::kBlockSize, &performanceStates[channel]);
				stringSynths[channel].Process(performanceStates[channel], patch, in, out, aux, anuli::kBlockSize);
				break;

			default:
				if (parts[channel].polyphony() != polyphonyMode) {
					parts[channel].set_polyphony(polyphonyMode);
				}

				parts[channel].set_model(resonatorModels[channel]);

				setupPatch(channel, patch, structure, parameterInfo);
				setupPerformance(channel, performanceStates[channel], structure, parameterInfo);

				// Process audio
				strummers[channel].Process(in, anuli::kBlockSize, &performanceStates[channel]);
				parts[channel].Process(performanceStates[channel], patch, in, out, aux, anuli::kBlockSize);
				break;
			}

			// Convert output buffer
			dsp::Frame<2> outputFrames[anuli::kBlockSize];
			for (int frame = 0; frame < anuli::kBlockSize; ++frame) {
				outputFrames[frame].samples[0] = out[frame];
				outputFrames[frame].samples[1] = aux[frame];
			}

			srcOutputs[channel].setRates(48000, static_cast<int>(sampleRate));
			int inCount = anuli::kBlockSize;
			int outCount = drbOutputBuffers[channel].capacity();
			srcOutputs[channel].process(outputFrames, &inCount, drbOutputBuffers[channel].endData(), &outCount);
			drbOutputBuffers[channel].endIncr(outCount);
		}
	}

	void setStrummingFlag(bool flag) {
		if (flag) {
			// Make sure the LED is off for a short enough time (ui.cc).
			// strummingFlagCounter = std::min(50, strummingFlagInterval >> 2);
			strummingFlagCounter = kLightsFrequency;
			strummingFlagInterval = 0;
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonBoolean(rootJ, "NotesModeSelection", bNotesModeSelection);
		setJsonBoolean(rootJ, "useFrequencyOffset", bUseFrequencyOffset);
		setJsonInt(rootJ, "displayChannel", displayChannel);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		getJsonBoolean(rootJ, "NotesModeSelection", bNotesModeSelection);
		getJsonBoolean(rootJ, "useFrequencyOffset", bUseFrequencyOffset);

		json_int_t intValue;

		if (getJsonInt(rootJ, "displayChannel", intValue)) {
			displayChannel = intValue;
		}
	}

	void setMode(int modeNum) {
		channelModes[0] = modeNum;
		resonatorModels[0] = modeNum < 6 ? rings::ResonatorModel(modeNum) : resonatorModels[0];
		params[PARAM_MODE].setValue(modeNum);
	}
};

struct AnuliWidget : SanguineModuleWidget {
	explicit AnuliWidget(Anuli* module) {
		setModule(module);

		moduleName = "anuli";
		panelSize = sanguineThemes::SIZE_21;
		backplateColor = sanguineThemes::PLATE_PURPLE;

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

		FramebufferWidget* anuliFramebuffer = new FramebufferWidget();
		addChild(anuliFramebuffer);

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 53.34f, 22.087f);
		anuliFramebuffer->addChild(displayModel);
		displayModel->fallbackString = anuli::modeLabels[0];

		addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(98.297, 22.087), module, Anuli::PARAM_MODE));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.383, 35.904), module, Anuli::INPUT_FREQUENCY_CV));

		SanguineTinyNumericDisplay* displayPolyphony = new SanguineTinyNumericDisplay(2, module, 53.34f, 37.486f);
		anuliFramebuffer->addChild(displayPolyphony);
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

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(53.15, 101.964), module, Anuli::PARAM_DAMPING_MOD));

#ifndef METAMODULE
		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 32.288, 101.019);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 94.721, 99.605);
		addChild(mutantsLogo);
#endif

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

		menu->addChild(createIndexSubmenuItem("Mode", anuli::menuModeLabels,
			[=]() { return module->params[Anuli::PARAM_MODE].getValue(); },
			[=](int i) { module->setMode(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Disastrous Peace FX", anuli::fxLabels,
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