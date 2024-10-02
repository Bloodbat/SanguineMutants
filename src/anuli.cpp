#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "rings/dsp/part.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

static const std::vector<std::string> anuliModeLabels = {
	"Modal Reso.",
	"Sym. Strings",
	"M. I. String",
	"FM voice",
	"Q. Sym. Str.",
	"Rev. String",
	"Disas. Peace"
};

static const std::vector<std::string> anuliFxLabels = {
	"Formant filter",
	"Rolandish chorus",
	"Caveman reverb",
	"Formant filter (less abrasive variant)",
	"Rolandish chorus (Solinaish ensemble)",
	"Caveman reverb (shinier variant)"
};

static const std::vector<std::string> anuliMenuLabels = {
	"Modal resonator",
	"Sympathetic strings",
	"Modulated/inharmonic string",
	"FM voice",
	"Quantized sympathetic strings",
	"Reverb string",
	"Disastrous Peace"
};

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
		ENUMS(LIGHT_RESONATOR, 16 * 3),
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

	int channelCount = 0;
	int polyphonyMode = 1;
	int strummingFlagCounter = 0;
	int strummingFlagInterval = 0;

	rings::ResonatorModel resonatorModel[PORT_MAX_CHANNELS];
	rings::ResonatorModel fxModel = rings::RESONATOR_MODEL_MODAL;
	bool bEasterEgg[PORT_MAX_CHANNELS] = {};

	std::string displayText = "";

	const int kDividerFrequency = 64;

	Anuli() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODE, 0.f, 6.f, 0.f, "Resonator model", anuliMenuLabels);

		configSwitch(PARAM_FX, 0.f, 5.f, 0.f, "Disastrous peace FX", anuliFxLabels);

		configParam(PARAM_POLYPHONY, 1.f, 4.f, 1.f, "Polyphony");
		paramQuantities[PARAM_POLYPHONY]->snapEnabled = true;

		configParam(PARAM_FREQUENCY, 0.0, 60.0, 30.0, "Frequency");
		configParam(PARAM_STRUCTURE, 0.0, 1.0, 0.5, "Structure");
		configParam(PARAM_BRIGHTNESS, 0.0, 1.0, 0.5, "Brightness");
		configParam(PARAM_DAMPING, 0.0, 1.0, 0.5, "Damping");
		configParam(PARAM_POSITION, 0.0, 1.0, 0.5, "Position");
		configParam(PARAM_BRIGHTNESS_MOD, -1.0, 1.0, 0.0, "Brightness CV");
		configParam(PARAM_FREQUENCY_MOD, -1.0, 1.0, 0.0, "Frequency CV");
		configParam(PARAM_DAMPING_MOD, -1.0, 1.0, 0.0, "Damping CV");
		configParam(PARAM_STRUCTURE_MOD, -1.0, 1.0, 0.0, "Structure CV");
		configParam(PARAM_POSITION_MOD, -1.0, 1.0, 0.0, "Position CV");

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
			strummer[i].Init(0.01, 44100.0 / 24);
			memset(&part[i], 0, sizeof(rings::Part));
			part[i].Init(reverbBuffer[i]);
			memset(&stringSynth[i], 0, sizeof(rings::StringSynthPart));
			stringSynth[i].Init(reverbBuffer[i]);

			resonatorModel[i] = rings::RESONATOR_MODEL_MODAL;
		}

		clockDivider.setDivision(kDividerFrequency);
	}

	void process(const ProcessArgs& args) override {
		const float sampleTime = kDividerFrequency * args.sampleTime;

		bool bDividerTurn = clockDivider.process();

		uint8_t disastrousCount = 0;

		channelCount = std::max(std::max(std::max(inputs[INPUT_STRUM].getChannels(), inputs[INPUT_PITCH].getChannels()), inputs[INPUT_IN].getChannels()), 1);

		outputs[OUTPUT_ODD].setChannels(channelCount);
		outputs[OUTPUT_EVEN].setChannels(channelCount);

		polyphonyMode = params[PARAM_POLYPHONY].getValue();

		fxModel = rings::ResonatorModel(params[PARAM_FX].getValue());

		for (int channel = 0; channel < channelCount; channel++) {
			int modeNum = 0;

			if (!inputs[INPUT_MODE].isConnected()) {
				modeNum = int(params[PARAM_MODE].getValue());
			}
			else {
				modeNum = clamp(int(inputs[INPUT_MODE].getVoltage(channel)), 0, 6);
			}

			if (modeNum < 6) {
				bEasterEgg[channel] = false;
				resonatorModel[channel] = rings::ResonatorModel(modeNum);
			}
			else {
				bEasterEgg[channel] = true;
				++disastrousCount;
			}

			// TODO
			// "Normalized to a pulse/burst generator that reacts to note changes on the V/OCT input."
			// Get input
			if (!inputBuffer[channel].full()) {
				dsp::Frame<1> frame;
				frame.samples[0] = inputs[INPUT_IN].getVoltage(channel) / 5.0;
				inputBuffer[channel].push(frame);
			}

			if (!bStrum[channel]) {
				bStrum[channel] = inputs[INPUT_STRUM].getVoltage(channel) >= 1.0;
			}

			// Render frames
			if (outputBuffer[channel].empty()) {
				float in[24] = {};
				// Convert input buffer
				{
					inputSrc[channel].setRates(args.sampleRate, 48000);
					int inLen = inputBuffer[channel].size();
					int outLen = 24;
					inputSrc[channel].process(inputBuffer[channel].startData(), &inLen, (dsp::Frame<1>*) in, &outLen);
					inputBuffer[channel].startIncr(inLen);
				}

				// Polyphony			
				if (part[channel].polyphony() != polyphonyMode)
					part[channel].set_polyphony(polyphonyMode);
				// Model
				if (bEasterEgg[channel])
					stringSynth[channel].set_fx(rings::FxType(fxModel));
				else
					part[channel].set_model(resonatorModel[channel]);

				// Patch
				rings::Patch patch;
				float structure = params[PARAM_STRUCTURE].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_STRUCTURE_MOD].getValue()) *
					inputs[INPUT_STRUCTURE_CV].getVoltage(channel) / 5.0;
				patch.structure = clamp(structure, 0.0f, 0.9995f);
				patch.brightness = clamp(params[PARAM_BRIGHTNESS].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_BRIGHTNESS_MOD].getValue()) *
					inputs[INPUT_BRIGHTNESS_CV].getVoltage(channel) / 5.0, 0.0f, 1.0f);
				patch.damping = clamp(params[PARAM_DAMPING].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_DAMPING_MOD].getValue()) *
					inputs[INPUT_DAMPING_CV].getVoltage(channel) / 5.0, 0.0f, 0.9995f);
				patch.position = clamp(params[PARAM_POSITION].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_POSITION_MOD].getValue()) *
					inputs[INPUT_POSITION_CV].getVoltage(channel) / 5.0, 0.0f, 0.9995f);

				// Performance
				rings::PerformanceState performanceState;
				performanceState.note = 12.0 * inputs[INPUT_PITCH].getNormalVoltage(1 / 12.0, channel);
				float transpose = params[PARAM_FREQUENCY].getValue();
				// Quantize transpose if pitch input is connected
				if (inputs[INPUT_PITCH].isConnected()) {
					transpose = roundf(transpose);
				}
				performanceState.tonic = 12.0 + clamp(transpose, 0.0f, 60.0f);
				performanceState.fm = clamp(48.0 * 3.3 * dsp::quarticBipolar(params[PARAM_FREQUENCY_MOD].getValue()) *
					inputs[INPUT_FREQUENCY_CV].getNormalVoltage(1.0, channel) / 5.0, -48.0f, 48.0f);

				performanceState.internal_exciter = !inputs[INPUT_IN].isConnected();
				performanceState.internal_strum = !inputs[INPUT_STRUM].isConnected();
				performanceState.internal_note = !inputs[INPUT_PITCH].isConnected();

				// TODO
				// "Normalized to a step detector on the V/OCT input and a transient detector on the IN input."
				performanceState.strum = bStrum[channel] && !bLastStrum[channel];
				bLastStrum[channel] = bStrum[channel];
				bStrum[channel] = false;
				if (channel == 0) {
					setStrummingFlag(performanceState.strum);
				}

				performanceState.chord = clamp(int(roundf(structure * (rings::kNumChords - 1))), 0, rings::kNumChords - 1);

				// Process audio
				float out[24];
				float aux[24];
				if (bEasterEgg[channel]) {
					strummer[channel].Process(NULL, 24, &performanceState);
					stringSynth[channel].Process(performanceState, patch, in, out, aux, 24);
				}
				else {
					strummer[channel].Process(in, 24, &performanceState);
					part[channel].Process(performanceState, patch, in, out, aux, 24);
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
				if (outputs[OUTPUT_ODD].isConnected() && outputs[OUTPUT_EVEN].isConnected()) {
					outputs[OUTPUT_ODD].setVoltage(clamp(outputFrame.samples[0], -1.0, 1.0) * 5.0, channel);
					outputs[OUTPUT_EVEN].setVoltage(clamp(outputFrame.samples[1], -1.0, 1.0) * 5.0, channel);
				}
				else {
					float v = clamp(outputFrame.samples[0] + outputFrame.samples[1], -1.0, 1.0) * 5.0;
					outputs[OUTPUT_ODD].setVoltage(v, channel);
					outputs[OUTPUT_EVEN].setVoltage(v, channel);
				}
			}
		}

		if (bDividerTurn) {
			displayText = anuliModeLabels[resonatorModel[0]];

			uint8_t pulseWidthModulationCounter = getSystemTimeMs() & 15;
			uint8_t triangle = (getSystemTimeMs() >> 5) & 31;
			triangle = triangle < 16 ? triangle : 31 - triangle;

			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
				int currentLight = LIGHT_RESONATOR + channel * 3;
				if (channel < channelCount) {
					if (!bEasterEgg[channel]) {
						if (resonatorModel[channel] < 3) {
							lights[currentLight + 0].setBrightnessSmooth(resonatorModel[channel] >= 1 ? 1.f : 0.f, sampleTime);
							lights[currentLight + 1].setBrightnessSmooth(resonatorModel[channel] <= 1 ? 1.f : 0.f, sampleTime);
							lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
						}
						else {
							lights[currentLight + 0].setBrightnessSmooth((resonatorModel[channel] >= 4 && pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
							lights[currentLight + 1].setBrightnessSmooth((resonatorModel[channel] <= 4 && pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
							lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
						}
					}
					else {
						lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
						lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
						lights[currentLight + 2].setBrightnessSmooth(pulseWidthModulationCounter < triangle ? 1.f : 0.f, sampleTime);
					}
				}
				else {
					lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
				}


				if (disastrousCount < 1) {
					lights[LIGHT_FX + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[LIGHT_FX + 1].setBrightnessSmooth(0.f, sampleTime);
				}
				else {
					if (fxModel < 3) {
						lights[LIGHT_FX + 0].setBrightnessSmooth(fxModel <= 1 ? 1.f : 0.f, sampleTime);
						lights[LIGHT_FX + 1].setBrightnessSmooth(fxModel >= 1 ? 1.f : 0.f, sampleTime);
					}
					else {
						lights[LIGHT_FX + 0].setBrightnessSmooth((fxModel <= 4 && pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
						lights[LIGHT_FX + 1].setBrightnessSmooth((fxModel >= 4 && pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
					}
				}

				if (polyphonyMode != 3) {
					lights[LIGHT_POLYPHONY + 0].setBrightness(polyphonyMode <= 2 ? 1.f : 0.f);
					lights[LIGHT_POLYPHONY + 1].setBrightness(polyphonyMode >= 2 ? 1.f : 0.f);
				}
				else {
					lights[LIGHT_POLYPHONY + 0].setBrightness(1.f);
					lights[LIGHT_POLYPHONY + 1].setBrightness(pulseWidthModulationCounter < triangle ? 1.f : 0.f);
				}

				++strummingFlagInterval;
				if (strummingFlagCounter) {
					--strummingFlagCounter;
					lights[LIGHT_POLYPHONY + 0].setBrightness(0.f);
					lights[LIGHT_POLYPHONY + 1].setBrightness(0.f);
				}
			}
		}
	}

	void setMode(int modeNum) {
		if (modeNum < 6) {
			bEasterEgg[0] = false;
			resonatorModel[0] = rings::ResonatorModel(modeNum);
			params[PARAM_MODE].setValue(modeNum);
		}
		else {
			bEasterEgg[0] = true;
			params[PARAM_MODE].setValue(modeNum);
		}
	}

	void setStrummingFlag(bool flag) {
		if (flag) {
			// Make sure the LED is off for a short enough time (ui.cc).
			strummingFlagCounter = std::min(50, strummingFlagInterval >> 2);
			strummingFlagInterval = 0;
		}
	}
};

struct AnuliWidget : SanguineModuleWidget {
	AnuliWidget(Anuli* module) {
		setModule(module);

		moduleName = "anuli";
		panelSize = SIZE_21;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

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

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 6.42, 100.55);
		addChild(bloodLogo);

		SanguineTinyNumericDisplay* displayChannelCount = new SanguineTinyNumericDisplay(2, module, 22.578, 100.75);
		anuliFrambuffer->addChild(displayChannelCount);
		displayChannelCount->fallbackNumber = 1;

		if (module) {
			displayChannelCount->values.numberValue = &module->channelCount;
		}

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
	}
};

Model* modelAnuli = createModel<Anuli, AnuliWidget>("Sanguine-Anuli");
