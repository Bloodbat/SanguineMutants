#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "rings/dsp/part.h"
#include "rings/dsp/strummer.h"
#include "rings/dsp/string_synth_part.h"
#include "sanguinehelpers.hpp"

static const std::vector<std::string> anuliModeLabels = {
	"Modal. Reso",
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
	"Caveman",
	"Formant filter (variant)",
	"Rolandish chorus (variant)",
	"Caveman (variant)"
};

struct Anuli : Module {
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
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_ODD,
		OUTPUT_EVEN,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_POLYPHONY, 2),
		ENUMS(LIGHT_RESONATOR, 3),
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
	bool bStrum[PORT_MAX_CHANNELS];
	bool bLastStrum[PORT_MAX_CHANNELS];

	int channelCount = 0;
	int polyphonyMode = 1;
	int strummingFlagCounter;
	int strummingFlagInterval;

	rings::ResonatorModel resonatorModel = rings::RESONATOR_MODEL_MODAL;
	rings::ResonatorModel fxModel = rings::RESONATOR_MODEL_MODAL;
	bool bEasterEgg = false;

	std::string displayText = "";

	const int kDividerFrequency = 64;

	Anuli() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_MODE, 0.f, 6.f, 0.f, "Resonator model");
		paramQuantities[PARAM_MODE]->snapEnabled = true;

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

		configOutput(OUTPUT_ODD, "Odd");
		configOutput(OUTPUT_EVEN, "Even");

		configBypass(INPUT_IN, OUTPUT_ODD);
		configBypass(INPUT_IN, OUTPUT_EVEN);

		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			strummer[i].Init(0.01, 44100.0 / 24);
			part[i].Init(reverbBuffer[i]);
			stringSynth[i].Init(reverbBuffer[i]);

			bStrum[i] = false;
			bLastStrum[i] = false;
		}

		clockDivider.setDivision(kDividerFrequency);
	}

	void process(const ProcessArgs& args) override {
		const float sampleTime = kDividerFrequency * args.sampleTime;

		bool dividerTurn = clockDivider.process();

		if (dividerTurn) {
			if (params[PARAM_MODE].getValue() < 6) {
				bEasterEgg = false;
				resonatorModel = rings::ResonatorModel(params[PARAM_MODE].getValue());
			}
			else {
				bEasterEgg = true;
			}
			displayText = anuliModeLabels[params[PARAM_MODE].getValue()];

			polyphonyMode = params[PARAM_POLYPHONY].getValue();

			fxModel = rings::ResonatorModel(params[PARAM_FX].getValue());
		}

		channelCount = std::max(std::max(std::max(inputs[INPUT_STRUM].getChannels(), inputs[INPUT_PITCH].getChannels()), inputs[INPUT_IN].getChannels()), 1);

		outputs[OUTPUT_ODD].setChannels(channelCount);
		outputs[OUTPUT_EVEN].setChannels(channelCount);

		for (int channel = 0; channel < channelCount; channel++) {
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
				if (bEasterEgg)
					stringSynth[channel].set_fx(rings::FxType(fxModel));
				else
					part[channel].set_model(resonatorModel);

				// Patch
				rings::Patch patch;
				float structure = params[PARAM_STRUCTURE].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_STRUCTURE_MOD].getValue()) * inputs[INPUT_STRUCTURE_CV].getVoltage(channel) / 5.0;
				patch.structure = clamp(structure, 0.0f, 0.9995f);
				patch.brightness = clamp(params[PARAM_BRIGHTNESS].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_BRIGHTNESS_MOD].getValue()) * inputs[INPUT_BRIGHTNESS_CV].getVoltage(channel) / 5.0, 0.0f, 1.0f);
				patch.damping = clamp(params[PARAM_DAMPING].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_DAMPING_MOD].getValue()) * inputs[INPUT_DAMPING_CV].getVoltage(channel) / 5.0, 0.0f, 0.9995f);
				patch.position = clamp(params[PARAM_POSITION].getValue() + 3.3 * dsp::quadraticBipolar(params[PARAM_POSITION_MOD].getValue()) * inputs[INPUT_POSITION_CV].getVoltage(channel) / 5.0, 0.0f, 0.9995f);

				// Performance
				rings::PerformanceState performanceState;
				performanceState.note = 12.0 * inputs[INPUT_PITCH].getNormalVoltage(1 / 12.0, channel);
				float transpose = params[PARAM_FREQUENCY].getValue();
				// Quantize transpose if pitch input is connected
				if (inputs[INPUT_PITCH].isConnected()) {
					transpose = roundf(transpose);
				}
				performanceState.tonic = 12.0 + clamp(transpose, 0.0f, 60.0f);
				performanceState.fm = clamp(48.0 * 3.3 * dsp::quarticBipolar(params[PARAM_FREQUENCY_MOD].getValue()) * inputs[INPUT_FREQUENCY_CV].getNormalVoltage(1.0, channel) / 5.0, -48.0f, 48.0f);

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
				if (bEasterEgg) {
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

		if (dividerTurn) {
			uint8_t pulseWidthModulationCounter = getSystemTimeMs() & 15;
			uint8_t triangle = (getSystemTimeMs() >> 5) & 31;
			triangle = triangle < 16 ? triangle : 31 - triangle;

			if (!bEasterEgg) {
				if (resonatorModel < 3) {
					lights[LIGHT_RESONATOR + 0].setBrightnessSmooth(resonatorModel >= 1 ? 1.f : 0.f, sampleTime);
					lights[LIGHT_RESONATOR + 1].setBrightnessSmooth(resonatorModel <= 1 ? 1.f : 0.f, sampleTime);
					lights[LIGHT_RESONATOR + 2].setBrightnessSmooth(0.f, sampleTime);
				}
				else {
					lights[LIGHT_RESONATOR + 0].setBrightnessSmooth((resonatorModel >= 4 && pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
					lights[LIGHT_RESONATOR + 1].setBrightnessSmooth((resonatorModel <= 4 && pulseWidthModulationCounter < triangle) ? 1.f : 0.f, sampleTime);
					lights[LIGHT_RESONATOR + 2].setBrightnessSmooth(0.f, sampleTime);
				}

				lights[LIGHT_FX + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_FX + 1].setBrightnessSmooth(0.f, sampleTime);
			}
			else {
				lights[LIGHT_RESONATOR + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_RESONATOR + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[LIGHT_RESONATOR + 2].setBrightnessSmooth(pulseWidthModulationCounter < triangle ? 1.f : 0.f, sampleTime);

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

	long long getSystemTimeMs() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}

	void setMode(int modeNum) {
		if (modeNum < 6) {
			bEasterEgg = false;
			resonatorModel = rings::ResonatorModel(modeNum);
			params[PARAM_MODE].setValue(modeNum);
		}
		else {
			bEasterEgg = true;
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

struct AnuliWidget : ModuleWidget {
	AnuliWidget(Anuli* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_21hp_purple.svg", "res/anuli_faceplate.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(8.383, 22.087), module, Anuli::LIGHT_RESONATOR));

		FramebufferWidget* anuliFrambuffer = new FramebufferWidget();
		addChild(anuliFrambuffer);

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 47.368f, 22.087f);
		anuliFrambuffer->addChild(displayModel);
		displayModel->fallbackString = anuliModeLabels[0];

		if (module) {
			displayModel->values.displayText = &module->displayText;
		}

		addParam(createParamCentered<Sanguine1PGrayCap>(millimetersToPixelsVec(91.161, 22.087), module, Anuli::PARAM_MODE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(101.146, 22.087),
			module, Anuli::PARAM_FX, Anuli::LIGHT_FX));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(8.383, 35.904), module, Anuli::INPUT_FREQUENCY_CV));

		addParam(createParamCentered<Sanguine1PRed>(millimetersToPixelsVec(53.34, 37.683), module, Anuli::PARAM_POLYPHONY));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(98.297, 35.904), module, Anuli::INPUT_STRUCTURE_CV));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.415, 42.833), module, Anuli::PARAM_FREQUENCY_MOD));

		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(33.006, 49.715), module, Anuli::PARAM_FREQUENCY));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(60.761, 61.388), module, Anuli::LIGHT_POLYPHONY));

		addParam(createParamCentered<Sanguine3PSGreen>(millimetersToPixelsVec(73.674, 49.715), module, Anuli::PARAM_STRUCTURE));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(87.986, 42.833), module, Anuli::PARAM_STRUCTURE_MOD));

		SanguineTinyNumericDisplay* displayPolyphony = new SanguineTinyNumericDisplay(2, module, 49.592, 61.388);
		anuliFrambuffer->addChild(displayPolyphony);
		displayPolyphony->fallbackNumber = 1;

		if (module) {
			displayPolyphony->values.numberValue = &module->polyphonyMode;
		}

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(33.006, 72.385), module, Anuli::PARAM_BRIGHTNESS));

		addParam(createParamCentered<Sanguine1PSYellow>(millimetersToPixelsVec(73.674, 72.385), module, Anuli::PARAM_POSITION));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.415, 81.324), module, Anuli::PARAM_BRIGHTNESS_MOD));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 53.34, 72.329);
		addChild(bloodLogo);

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(87.986, 81.324), module, Anuli::PARAM_POSITION_MOD));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(8.383, 86.197), module, Anuli::INPUT_BRIGHTNESS_CV));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(53.34, 84.417), module, Anuli::PARAM_DAMPING));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(98.297, 86.197), module, Anuli::INPUT_POSITION_CV));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(53.15, 101.964), module, Anuli::PARAM_DAMPING_MOD));

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 94.721, 99.605);
		addChild(mutantsLogo);

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(53.34, 112.736), module, Anuli::INPUT_DAMPING_CV));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(8.728, 116.807), module, Anuli::INPUT_STRUM));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(22.58, 116.807), module, Anuli::INPUT_PITCH));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(36.382, 116.807), module, Anuli::INPUT_IN));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(84.046, 116.807), module, Anuli::OUTPUT_ODD));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(97.898, 116.807), module, Anuli::OUTPUT_EVEN));
	}

	void appendContextMenu(Menu* menu) override {
		Anuli* module = dynamic_cast<Anuli*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);

		static const std::vector<std::string> anuliMenuLabels = {
			"Modal resonator",
			"Sympathetic strings",
			"Modulated/inharmonic string",
			"FM voice",
			"Quantized sympathetic strings",
			"Reverb string",
			"Disastrous Peace"
		};

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