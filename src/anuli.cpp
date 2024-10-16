#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "rings/dsp/part.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"

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

	dsp::ClockDivider clockDivider;

	uint16_t reverbBuffer[PORT_MAX_CHANNELS][32768] = {};
	rings::Part part[PORT_MAX_CHANNELS];
	rings::StringSynthPart stringSynth[PORT_MAX_CHANNELS];
	rings::Strummer strummer[PORT_MAX_CHANNELS];
	bool bStrum[PORT_MAX_CHANNELS] = {};
	bool bLastStrum[PORT_MAX_CHANNELS] = {};
	bool bEasterEgg[PORT_MAX_CHANNELS] = {};

	bool bNotesModeSelection = false;

	int channelCount = 0;
	int polyphonyMode = 1;
	int strummingFlagCounter = 0;
	int strummingFlagInterval = 0;

	int displayChannel = 0;

	rings::ResonatorModel resonatorModel[PORT_MAX_CHANNELS];
	rings::FxType fxModel = rings::FX_FORMANT;

	std::string displayText = "";

	const int kDividerFrequency = 64;

	const float kVoltPerOctave = 1.f / 12.f;

	Anuli() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODE, 0.f, 6.f, 0.f, "Resonator model", anuliMenuLabels);

		configSwitch(PARAM_FX, 0.f, 5.f, 0.f, "Disastrous peace FX", anuliFxLabels);

		configParam(PARAM_POLYPHONY, 1.f, 4.f, 1.f, "Polyphony");
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

		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			memset(&strummer[i], 0, sizeof(rings::Strummer));
			memset(&part[i], 0, sizeof(rings::Part));
			memset(&stringSynth[i], 0, sizeof(rings::StringSynthPart));
			strummer[i].Init(0.01f, 44100.f / 24);
			part[i].Init(reverbBuffer[i]);
			stringSynth[i].Init(reverbBuffer[i]);

			resonatorModel[i] = rings::RESONATOR_MODEL_MODAL;
		}

		clockDivider.setDivision(kDividerFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		bool bDividerTurn = clockDivider.process();

		bool bDisastrousPeace = false;

		channelCount = std::max(std::max(std::max(inputs[INPUT_STRUM].getChannels(), inputs[INPUT_PITCH].getChannels()), inputs[INPUT_IN].getChannels()), 1);

		outputs[OUTPUT_ODD].setChannels(channelCount);
		outputs[OUTPUT_EVEN].setChannels(channelCount);

		polyphonyMode = params[PARAM_POLYPHONY].getValue();

		fxModel = static_cast<rings::FxType>(params[PARAM_FX].getValue());

		int modeNum = static_cast<int>(params[PARAM_MODE].getValue());

		float structureMod = dsp::quadraticBipolar(params[PARAM_STRUCTURE_MOD].getValue());
		float brightnessMod = dsp::quadraticBipolar(params[PARAM_BRIGHTNESS_MOD].getValue());
		float dampingMod = dsp::quadraticBipolar(params[PARAM_DAMPING_MOD].getValue());
		float positionMod = dsp::quadraticBipolar(params[PARAM_POSITION_MOD].getValue());

		float frequencyMod = dsp::quarticBipolar(params[PARAM_FREQUENCY_MOD].getValue());

		bool bInternalExciter = !inputs[INPUT_IN].isConnected();
		bool bInternalStrum = !inputs[INPUT_STRUM].isConnected();
		bool bInternalNote = !inputs[INPUT_PITCH].isConnected();

		bool bHaveBothOutputs = outputs[OUTPUT_ODD].isConnected() && outputs[OUTPUT_EVEN].isConnected();

		for (int channel = 0; channel < channelCount; channel++) {
			if (inputs[INPUT_MODE].isConnected()) {
				if (!bNotesModeSelection) {
					modeNum = clamp(static_cast<int>(inputs[INPUT_MODE].getVoltage(channel)), 0, 6);
				}
				else {
					modeNum = clamp(static_cast<int>(roundf(inputs[INPUT_MODE].getVoltage(channel) * 12.f)), 0, 6);
				}
			}

			bEasterEgg[channel] = modeNum >= 6;
			bDisastrousPeace = bDisastrousPeace || bEasterEgg[channel];
			resonatorModel[channel] = static_cast<rings::ResonatorModel>(clamp(rings::ResonatorModel(modeNum), 0, 6));

			// TODO
			// "Normalized to a pulse/burst generator that reacts to note changes on the V/OCT input."
			// Get input
			if (!inputBuffer[channel].full()) {
				dsp::Frame<1> frame;
				frame.samples[0] = inputs[INPUT_IN].getVoltage(channel) / 5.f;
				inputBuffer[channel].push(frame);
			}

			if (!bStrum[channel]) {
				bStrum[channel] = inputs[INPUT_STRUM].getVoltage(channel) >= 1.f;
			}

			// Render frames
			if (outputBuffer[channel].empty()) {
				float in[24] = {};
				// Convert input buffer
				inputSrc[channel].setRates(args.sampleRate, 48000);
				int inLen = inputBuffer[channel].size();
				int outLen = 24;
				inputSrc[channel].process(inputBuffer[channel].startData(), &inLen, (dsp::Frame<1>*) in, &outLen);
				inputBuffer[channel].startIncr(inLen);

				// Polyphony
				if (part[channel].polyphony() != polyphonyMode) {
					part[channel].set_polyphony(polyphonyMode);
					stringSynth[channel].set_polyphony(polyphonyMode);
				}
				// Model
				stringSynth[channel].set_fx(rings::FxType(fxModel));
				part[channel].set_model(resonatorModel[channel]);

				// Patch
				rings::Patch patch;

				float_4 voltages;

				voltages[0] = inputs[INPUT_STRUCTURE_CV].getVoltage(channel);
				voltages[1] = inputs[INPUT_BRIGHTNESS_CV].getVoltage(channel);
				voltages[2] = inputs[INPUT_DAMPING_CV].getVoltage(channel);
				voltages[3] = inputs[INPUT_POSITION_CV].getVoltage(channel);

				voltages /= 5.f;

				float structure = params[PARAM_STRUCTURE].getValue() + 3.3f * structureMod * voltages[0];
				patch.structure = clamp(structure, 0.f, 0.9995f);
				patch.brightness = clamp(params[PARAM_BRIGHTNESS].getValue() + 3.3f * brightnessMod * voltages[1], 0.f, 1.f);
				patch.damping = clamp(params[PARAM_DAMPING].getValue() + 3.3f * dampingMod * voltages[2], 0.f, 0.9995f);
				patch.position = clamp(params[PARAM_POSITION].getValue() + 3.3f * positionMod * voltages[3], 0.f, 0.9995f);

				// Performance
				rings::PerformanceState performanceState;

				performanceState.note = 12.f * inputs[INPUT_PITCH].getNormalVoltage(kVoltPerOctave, channel);
				float transpose = params[PARAM_FREQUENCY].getValue();
				// Quantize transpose if pitch input is connected
				if (inputs[INPUT_PITCH].isConnected()) {
					transpose = roundf(transpose);
				}
				performanceState.tonic = 12.f + clamp(transpose, 0.f, 60.f);
				performanceState.fm = clamp(48.f * 3.3f * frequencyMod * inputs[INPUT_FREQUENCY_CV].getNormalVoltage(1.f, channel) / 5.f, -48.f, 48.f);

				performanceState.internal_exciter = bInternalExciter;
				performanceState.internal_strum = bInternalStrum;
				performanceState.internal_note = bInternalNote;

				// TODO
				// "Normalized to a step detector on the V/OCT input and a transient detector on the IN input."
				performanceState.strum = bStrum[channel] && !bLastStrum[channel];
				bLastStrum[channel] = bStrum[channel];
				bStrum[channel] = false;
				if (channel == 0) {
					setStrummingFlag(performanceState.strum);
				}

				performanceState.chord = clamp(static_cast<int>(roundf(structure * (rings::kNumChords - 1))), 0, rings::kNumChords - 1);

				// Process audio
				float out[24];
				float aux[24];
				if (!bEasterEgg[channel]) {
					strummer[channel].Process(in, 24, &performanceState);
					part[channel].Process(performanceState, patch, in, out, aux, 24);
				}
				else {
					strummer[channel].Process(NULL, 24, &performanceState);
					stringSynth[channel].Process(performanceState, patch, in, out, aux, 24);
				}

				// Convert output buffer
				{
					dsp::Frame<2> outputFrames[24];
					for (int i = 0; i < 24; i++) {
						outputFrames[i].samples[0] = out[i];
						outputFrames[i].samples[1] = aux[i];
					}

					outputSrc[channel].setRates(48000, args.sampleRate);
					int inLen = 24;
					int outLen = outputBuffer[channel].capacity();
					outputSrc[channel].process(outputFrames, &inLen, outputBuffer[channel].endData(), &outLen);
					outputBuffer[channel].endIncr(outLen);
				}
			}

			// Set output
			if (!outputBuffer[channel].empty()) {
				dsp::Frame<2> outputFrame = outputBuffer[channel].shift();
				// "Note that you need to insert a jack into each output to split the signals: when only one jack is inserted, both signals are mixed together."
				if (bHaveBothOutputs) {
					outputs[OUTPUT_ODD].setVoltage(clamp(outputFrame.samples[0], -1.f, 1.f) * 5.f, channel);
					outputs[OUTPUT_EVEN].setVoltage(clamp(outputFrame.samples[1], -1.f, 1.f) * 5.f, channel);
				}
				else {
					float outVoltage = clamp(outputFrame.samples[0] + outputFrame.samples[1], -1.f, 1.f) * 5.f;
					outputs[OUTPUT_ODD].setVoltage(outVoltage, channel);
					outputs[OUTPUT_EVEN].setVoltage(outVoltage, channel);
				}
			}
		}

		if (bDividerTurn) {
			const float sampleTime = kDividerFrequency * args.sampleTime;

			if (displayChannel >= channelCount) {
				displayChannel = channelCount - 1;
			}

			displayText = bEasterEgg[0] ? anuliModeLabels[6] : anuliModeLabels[resonatorModel[displayChannel]];

			long long systemTimeMs = getSystemTimeMs();

			uint8_t pulseWidthModulationCounter = systemTimeMs & 15;
			uint8_t triangle = (systemTimeMs >> 5) & 31;
			triangle = triangle < 16 ? triangle : 31 - triangle;

			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
				const int currentLight = LIGHT_RESONATOR + channel * 3;

				for (int j = 0; j < 3; j++) {
					lights[currentLight + j].setBrightnessSmooth(0.f, sampleTime);
				}

				if (channel < channelCount) {
					if (!bEasterEgg[channel]) {
						if (resonatorModel[channel] < rings::RESONATOR_MODEL_FM_VOICE) {
							lights[currentLight + 0].setBrightnessSmooth(resonatorModel[channel] & 3 ? 1.f : 0.f, sampleTime);
							lights[currentLight + 1].setBrightnessSmooth(resonatorModel[channel] <= 1 ? 1.f : 0.f, sampleTime);
							lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
						}
						else {
							lights[currentLight + 0].setBrightnessSmooth((resonatorModel[channel] & 4 &&
								pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
							lights[currentLight + 1].setBrightnessSmooth((resonatorModel[channel] <= 4 &&
								pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
							lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
						}
					}
					else {
						lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
						lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
						lights[currentLight + 2].setBrightnessSmooth(pulseWidthModulationCounter < triangle ? 1.f : 0.f, sampleTime);
					}
				}
			}

			if (!bDisastrousPeace) {
				lights[LIGHT_FX + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_FX + 1].setBrightnessSmooth(0.f, sampleTime);
			}
			else {
				if (fxModel < rings::FX_FORMANT_2) {
					lights[LIGHT_FX + 0].setBrightnessSmooth(fxModel <= 1 ? 0.75f : 0.f, sampleTime);
					lights[LIGHT_FX + 1].setBrightnessSmooth(fxModel >= 1 ? 0.75f : 0.f, sampleTime);
				}
				else {
					lights[LIGHT_FX + 0].setBrightnessSmooth((fxModel <= 4 &&
						pulseWidthModulationCounter < triangle) ? 0.75f : 0.f, sampleTime);
					lights[LIGHT_FX + 1].setBrightnessSmooth((fxModel >= 4 &&
						pulseWidthModulationCounter < triangle) ? 0.75f : 0.f, sampleTime);
				}
			}

			lights[LIGHT_POLYPHONY + 0].setBrightness(polyphonyMode <= 3 ? 1.f : 0.f);
			lights[LIGHT_POLYPHONY + 1].setBrightness((polyphonyMode != 3 && polyphonyMode & 0x06) ||
				(polyphonyMode == 3 && pulseWidthModulationCounter < triangle) ? 1.f : 0.f);

			++strummingFlagInterval;
			if (strummingFlagCounter) {
				--strummingFlagCounter;
				lights[LIGHT_POLYPHONY + 0].setBrightness(0.f);
				lights[LIGHT_POLYPHONY + 1].setBrightness(0.f);
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

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "NotesModeSelection", json_boolean(bNotesModeSelection));

		json_object_set_new(rootJ, "displayChannel", json_integer(displayChannel));

		return rootJ;

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
	}

	void setMode(int modeNum) {
		resonatorModel[0] = modeNum < 6 ? rings::ResonatorModel(modeNum) : resonatorModel[0];
		bEasterEgg[0] = modeNum > 5;
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

		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentXA, 14.973),
				module, Anuli::LIGHT_RESONATOR + i * 3));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentXB, 14.973),
				module, Anuli::LIGHT_RESONATOR + ((i + lightIdOffset) * 3)));
			currentXA += xDelta;
			currentXB += xDelta;
		}


		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(9.021, 22.087), module, Anuli::INPUT_MODE));

		FramebufferWidget* anuliFrambuffer = new FramebufferWidget();
		addChild(anuliFrambuffer);

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 53.34f, 22.087f);
		anuliFrambuffer->addChild(displayModel);
		displayModel->fallbackString = anuliModeLabels[0];

		if (module) {
			displayModel->values.displayText = &module->displayText;
		}

		addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(98.297, 22.087), module, Anuli::PARAM_MODE));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.383, 35.904), module, Anuli::INPUT_FREQUENCY_CV));

		addParam(createParamCentered<Sanguine1PRed>(millimetersToPixelsVec(53.34, 37.683), module, Anuli::PARAM_POLYPHONY));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(98.297, 35.904), module, Anuli::INPUT_STRUCTURE_CV));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.415, 42.833), module, Anuli::PARAM_FREQUENCY_MOD));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(33.006, 49.715), module, Anuli::PARAM_FREQUENCY));

		addParam(createParamCentered<Sanguine3PSGreen>(millimetersToPixelsVec(73.674, 49.715), module, Anuli::PARAM_STRUCTURE));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(87.986, 42.833), module, Anuli::PARAM_STRUCTURE_MOD));

		SanguineTinyNumericDisplay* displayPolyphony = new SanguineTinyNumericDisplay(2, module, 53.34, 54.795);
		anuliFrambuffer->addChild(displayPolyphony);
		displayPolyphony->fallbackNumber = 1;

		if (module) {
			displayPolyphony->values.numberValue = &module->polyphonyMode;
		}

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(33.006, 72.385), module, Anuli::PARAM_BRIGHTNESS));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(53.34f, 67.085f), module, Anuli::LIGHT_POLYPHONY));

		addParam(createParamCentered<Sanguine1PSYellow>(millimetersToPixelsVec(73.674, 72.385), module, Anuli::PARAM_POSITION));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.415, 81.324), module, Anuli::PARAM_BRIGHTNESS_MOD));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(87.986, 81.324), module, Anuli::PARAM_POSITION_MOD));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.383, 86.197), module, Anuli::INPUT_BRIGHTNESS_CV));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(53.34, 84.417), module, Anuli::PARAM_DAMPING));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(98.297, 86.197), module, Anuli::INPUT_POSITION_CV));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 22.578, 100.55);
		addChild(bloodLogo);

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(53.15, 101.964), module, Anuli::PARAM_DAMPING_MOD));

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 94.721, 99.605);
		addChild(mutantsLogo);

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(53.34, 112.736), module, Anuli::INPUT_DAMPING_CV));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(8.728, 116.807), module, Anuli::INPUT_STRUM));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(22.58, 116.807), module, Anuli::INPUT_PITCH));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(36.382, 116.807), module, Anuli::INPUT_IN));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(71.828, 116.609),
			module, Anuli::PARAM_FX, Anuli::LIGHT_FX));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(84.046, 116.807), module, Anuli::OUTPUT_ODD));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(97.898, 116.807), module, Anuli::OUTPUT_EVEN));
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

		std::vector<std::string> availableChannels;
		for (int i = 0; i < module->channelCount; i++) {
			availableChannels.push_back(channelNumbers[i]);
		}
		menu->addChild(createIndexSubmenuItem("Display channel", availableChannels,
			[=]() {return module->displayChannel; },
			[=](int i) {module->displayChannel = i; }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("C4-F#4 direct mode selection", "", &module->bNotesModeSelection));
	}
};

Model* modelAnuli = createModel<Anuli, AnuliWidget>("Sanguine-Anuli");
