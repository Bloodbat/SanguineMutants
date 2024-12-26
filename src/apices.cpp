#include "plugin.hpp"
#include "peaks/processors.h"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "apices.hpp"
#include "nix.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Apices : SanguineModule {
	enum ParamIds {
		PARAM_KNOB_1,
		PARAM_KNOB_2,
		PARAM_KNOB_3,
		PARAM_KNOB_4,
		PARAM_EDIT_MODE,
		PARAM_EXPERT_MODE,
		PARAM_CHANNEL_SELECT,
		PARAM_TRIGGER_1,
		PARAM_TRIGGER_2,
		PARAM_MODE,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_GATE_1,
		INPUT_GATE_2,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_OUT_1,
		OUTPUT_OUT_2,
		OUTPUTS_COUNT
	};
	enum LightIds {
		LIGHT_TRIGGER_1,
		LIGHT_TRIGGER_2,
		LIGHT_CHANNEL_1,
		LIGHT_CHANNEL_2,
		ENUMS(LIGHT_CHANNEL_SELECT, 1 * 2),
		LIGHT_SPLIT_MODE,
		LIGHT_EXPERT_MODE,
		ENUMS(LIGHT_KNOBS_MODE, 4 * 3),
		LIGHT_FUNCTION_1,
		LIGHT_FUNCTION_2,
		LIGHT_FUNCTION_3,
		LIGHT_FUNCTION_4,
		LIGHT_EXPANDER,
		LIGHTS_COUNT
	};

	EditMode editMode = EDIT_MODE_TWIN;
	ApicesProcessorFunction processorFunction[kNumChannels] = { FUNCTION_ENVELOPE, FUNCTION_ENVELOPE };
	Settings settings = {};

	uint8_t potValue[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	bool bSnapMode = false;
	bool bSnapped[kNumKnobs] = { false, false, false, false };

	bool bHasExpander = false;

	int32_t adcLp[kNumAdcChannels] = { 0, 0, 0, 0 };
	int32_t adcValue[kNumAdcChannels] = { 0, 0, 0, 0 };
	int32_t adcThreshold[kNumAdcChannels] = { 0, 0, 0, 0 };

	peaks::Processors processors[kNumAdcChannels] = {};

	int16_t output[kBlockSize] = {};
	int16_t brightness[kNumChannels] = { 0, 0 };

	dsp::SchmittTrigger stSwitches[kButtonCount];

	// update descriptions/oleds every 16 samples
	static const int kClockUpdateFrequency = 16;
	dsp::ClockDivider clockDivider;

	peaks::GateFlags gateFlags[kNumChannels] = { 0, 0 };

	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;

	struct Block {
		peaks::GateFlags input[kNumChannels][kBlockSize] = {};
		uint16_t output[kNumChannels][kBlockSize] = {};
	};

	struct Slice {
		Block* block;
		size_t frame_index;
	};

	Block block[kNumBlocks] = {};
	size_t ioFrame = 0;
	size_t ioBlock = 0;
	size_t renderBlock = kNumBlocks / 2;

	std::string displayText1 = "";
	std::string displayText2 = "";

	std::string oledText1 = "";
	std::string oledText2 = "";
	std::string oledText3 = "";
	std::string oledText4 = "";

	const peaks::ProcessorFunction processorFunctionTable[FUNCTION_LAST][kNumChannels] = {
		{ peaks::PROCESSOR_FUNCTION_ENVELOPE, peaks::PROCESSOR_FUNCTION_ENVELOPE },
		{ peaks::PROCESSOR_FUNCTION_LFO, peaks::PROCESSOR_FUNCTION_LFO },
		{ peaks::PROCESSOR_FUNCTION_TAP_LFO, peaks::PROCESSOR_FUNCTION_TAP_LFO },
		{ peaks::PROCESSOR_FUNCTION_BASS_DRUM, peaks::PROCESSOR_FUNCTION_SNARE_DRUM },

		{ peaks::PROCESSOR_FUNCTION_MINI_SEQUENCER, peaks::PROCESSOR_FUNCTION_MINI_SEQUENCER },
		{ peaks::PROCESSOR_FUNCTION_PULSE_SHAPER, peaks::PROCESSOR_FUNCTION_PULSE_SHAPER },
		{ peaks::PROCESSOR_FUNCTION_PULSE_RANDOMIZER, peaks::PROCESSOR_FUNCTION_PULSE_RANDOMIZER },
		{ peaks::PROCESSOR_FUNCTION_FM_DRUM, peaks::PROCESSOR_FUNCTION_FM_DRUM },

		{ peaks::PROCESSOR_FUNCTION_NUMBER_STATION, peaks::PROCESSOR_FUNCTION_NUMBER_STATION},
		{ peaks::PROCESSOR_FUNCTION_BOUNCING_BALL, peaks::PROCESSOR_FUNCTION_BOUNCING_BALL}
	};

	Apices() {

		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_MODE, 0.f, 9.f, 0.f, "Mode", "", 0.f, 1.f, 1.f);
		paramQuantities[PARAM_MODE]->snapEnabled = true;

		configParam(PARAM_KNOB_1, 0.f, 65535.f, 32678.f, "Knob 1", "", 0.f, (1.f / 65535.f) * 100);
		configParam(PARAM_KNOB_2, 0.f, 65535.f, 32678.f, "Knob 2", "", 0.f, (1.f / 65535.f) * 100);
		configParam(PARAM_KNOB_3, 0.f, 65535.f, 32678.f, "Knob 3", "", 0.f, (1.f / 65535.f) * 100);
		configParam(PARAM_KNOB_4, 0.f, 65535.f, 32678.f, "Knob 4", "", 0.f, (1.f / 65535.f) * 100);
		configButton(PARAM_EDIT_MODE, "Toggle split mode");
		configButton(PARAM_CHANNEL_SELECT, "Expert mode channel select");
		configButton(PARAM_EXPERT_MODE, "Toggle expert mode");
		configButton(PARAM_TRIGGER_1, "Trigger 1");
		configButton(PARAM_TRIGGER_2, "Trigger 2");

		settings.editMode = EDIT_MODE_TWIN;
		settings.processorFunction[0] = FUNCTION_ENVELOPE;
		settings.processorFunction[1] = FUNCTION_ENVELOPE;
		settings.snap_mode = false;

		for (size_t channel = 0; channel < kNumChannels; ++channel)
		{
			memset(&processors[channel], 0, sizeof(peaks::Processors));
			processors[channel].Init(channel);
		}

		clockDivider.setDivision(kClockUpdateFrequency);

		init();
	}

	void process(const ProcessArgs& args) override {
		Module* nixExpander = getRightExpander().module;

		float sampleTime = 0.f;

		bHasExpander = (nixExpander && nixExpander->getModel() == modelNix);

		bool bDividerTurn = clockDivider.process();

		// only update knobs / lights every 16 samples
		if (bDividerTurn) {
			pollSwitches(args);
			pollPots();
			updateOleds();

			sampleTime = args.sampleTime * kClockUpdateFrequency;

			lights[LIGHT_EXPANDER].setBrightnessSmooth(bHasExpander ? 0.5f : 0.f, sampleTime);
		}

		ApicesProcessorFunction currentFunction = getProcessorFunction();
		if (params[PARAM_MODE].getValue() != currentFunction) {
			currentFunction = static_cast<ApicesProcessorFunction>(params[PARAM_MODE].getValue());
			setFunction(editMode - EDIT_MODE_FIRST, currentFunction);
			saveState();
		}

		if (bHasExpander) {
			float cvValues[kMaxFunctions * 2] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
			int modulatedValues[kMaxFunctions * 2] = { 0, 0, 0, 0, 0, 0, 0, 0 };

			int channel2Function;

			if (bDividerTurn) {
				Light& channel1LightRed = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1);
				Light& channel1LightGreen = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1 + 1);
				Light& channel1LightBlue = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1 + 2);

				switch (editMode) {
				case EDIT_MODE_FIRST:
				case EDIT_MODE_SECOND:
					channel1LightRed.setBrightnessSmooth(0.f, sampleTime);
					channel1LightGreen.setBrightnessSmooth(0.5f, sampleTime);
					channel1LightBlue.setBrightnessSmooth(0.f, sampleTime);
					switchExpanderChannel2Lights(nixExpander, true, sampleTime);
					break;
				case EDIT_MODE_TWIN:
					channel1LightRed.setBrightnessSmooth(0.5f, sampleTime);
					channel1LightGreen.setBrightnessSmooth(0.f, sampleTime);
					channel1LightBlue.setBrightnessSmooth(0.5f, sampleTime);
					switchExpanderChannel2Lights(nixExpander, false, sampleTime);
					break;
				case EDIT_MODE_SPLIT:
					channel1LightRed.setBrightnessSmooth(0.5f, sampleTime);
					channel1LightGreen.setBrightnessSmooth(0.f, sampleTime);
					channel1LightBlue.setBrightnessSmooth(0.f, sampleTime);
					switchExpanderChannel2Lights(nixExpander, false, sampleTime);
					break;
				default:
					break;
				}
			}

			for (int function = 0; function < kMaxFunctions; ++function) {
				int channel1Input = Nix::INPUT_PARAM_CV_1 + function;

				if (nixExpander->getInput(channel1Input).isConnected()) {
					int channel1Attenuverter = Nix::PARAM_PARAM_CV_1 + function;

					cvValues[function] = rescale(nixExpander->getInput(channel1Input).getVoltage(), -5.f, 5.f, -255.f, 255.f) *
						nixExpander->getParam(channel1Attenuverter).getValue();
				}
				modulatedValues[function] = clamp((potValue[function] + static_cast<int>(cvValues[function])) << 8, 0, 65535);

				if (editMode > EDIT_MODE_SPLIT) {
					int channel2Input = Nix::INPUT_PARAM_CV_CHANNEL_2_1 + function;
					channel2Function = function + kChannel2Offset;

					if (nixExpander->getInput(channel2Input).isConnected()) {
						int channel2Attenuverter = Nix::PARAM_PARAM_CV_CHANNEL_2_1 + function;

						cvValues[channel2Function] = rescale(nixExpander->getInput(channel2Input).getVoltage(), -5.f, 5.f, -255.f, 255.f) *
							nixExpander->getParam(channel2Attenuverter).getValue();
					}

					modulatedValues[channel2Function] = clamp((potValue[channel2Function] +
						static_cast<int>(cvValues[channel2Function])) << 8, 0, 65535);
				}

				switch (editMode) {
				case EDIT_MODE_TWIN:
					processors[0].set_parameter(function, modulatedValues[function]);
					processors[1].set_parameter(function, modulatedValues[function]);

					if (bDividerTurn) {
						Light& currentLightRed = nixExpander->getLight(Nix::LIGHT_PARAM_1 + function * 3);
						Light& currentLightGreen = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 1);
						Light& currentLightBlue = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 2);

						currentLightRed.setBrightnessSmooth(0.5f, sampleTime);
						currentLightGreen.setBrightnessSmooth(0.f, sampleTime);
						currentLightBlue.setBrightnessSmooth(0.5f, sampleTime);
					}
					break;

				case EDIT_MODE_SPLIT:
					if (function < 2) {
						processors[0].set_parameter(function, modulatedValues[function]);
					} else {
						processors[1].set_parameter(function - 2, modulatedValues[function]);
					}

					if (bDividerTurn) {
						if (function < 2) {
							Light& currentLightRed = nixExpander->getLight(Nix::LIGHT_PARAM_1 + function * 3);
							Light& currentLightGreen = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 1);
							Light& currentLightBlue = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 2);

							currentLightRed.setBrightnessSmooth(0.5f, sampleTime);
							currentLightGreen.setBrightnessSmooth(0.f, sampleTime);
							currentLightBlue.setBrightnessSmooth(0.f, sampleTime);
						} else {
							Light& currentLightRed = nixExpander->getLight(Nix::LIGHT_PARAM_1 + function * 3);
							Light& currentLightGreen = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 1);
							Light& currentLightBlue = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 2);

							currentLightRed.setBrightnessSmooth(0.f, sampleTime);
							currentLightGreen.setBrightnessSmooth(0.f, sampleTime);
							currentLightBlue.setBrightnessSmooth(0.5f, sampleTime);
						}
					}
					break;

				case EDIT_MODE_FIRST:
				case EDIT_MODE_SECOND:
					processors[0].set_parameter(function, modulatedValues[function]);
					processors[1].set_parameter(function, modulatedValues[channel2Function]);

					if (bDividerTurn) {
						Light& currentLightRed = nixExpander->getLight(Nix::LIGHT_PARAM_1 + function * 3);
						Light& currentLightGreen = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 1);
						Light& currentLightBlue = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 2);

						currentLightRed.setBrightnessSmooth(0.f, sampleTime);
						currentLightGreen.setBrightnessSmooth(0.5f, sampleTime);
						currentLightBlue.setBrightnessSmooth(0.f, sampleTime);
					}
					break;
				default:
					break;
				}
			}
		}

		if (outputBuffer.empty()) {

			while (renderBlock != ioBlock) {
				processChannels(&block[renderBlock], kBlockSize);
				renderBlock = (renderBlock + 1) % kNumBlocks;
			}

			uint32_t gateTriggers = 0;
			gateTriggers |= inputs[INPUT_GATE_1].getVoltage() >= 0.7f ? 1 : 0;
			gateTriggers |= inputs[INPUT_GATE_2].getVoltage() >= 0.7f ? 2 : 0;

			uint32_t buttons = 0;
			buttons |= (params[PARAM_TRIGGER_1].getValue() ? 1 : 0);
			buttons |= (params[PARAM_TRIGGER_2].getValue() ? 2 : 0);

			uint32_t gateInputs = gateTriggers | buttons;

			// Prepare sample rate conversion.
			// Peaks is sampling at 48kHZ.
			outputSrc.setRates(48000, args.sampleRate);
			int inLen = kBlockSize;
			int outLen = outputBuffer.capacity();
			dsp::Frame<2> frame[kBlockSize];

			// Process an entire block of data from the IOBuffer.
			for (size_t blockNum = 0; blockNum < kBlockSize; ++blockNum) {

				Slice slice = NextSlice(1);

				for (size_t channel = 0; channel < kNumChannels; ++channel) {
					gateFlags[channel] = peaks::ExtractGateFlags(gateFlags[channel], gateInputs & (1 << channel));

					frame[blockNum].samples[channel] = slice.block->output[channel][slice.frame_index];
				}

				// A hack to make channel 1 aware of what's going on in channel 2. Used to
				// reset the sequencer.
				slice.block->input[0][slice.frame_index] = gateFlags[0] | (gateFlags[1] << 4) | (buttons & 8 ? peaks::GATE_FLAG_FROM_BUTTON : 0);

				slice.block->input[1][slice.frame_index] = gateFlags[1] | (buttons & 2 ? peaks::GATE_FLAG_FROM_BUTTON : 0);
			}

			outputSrc.process(frame, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}

		// Update outputs.
		if (!outputBuffer.empty()) {
			dsp::Frame<2> frame = outputBuffer.shift();

			// Peaks manual says output spec is 0..8V for envelopes and 10Vpp for audio/CV.
			// TODO: Check the output values against an actual device.
			outputs[OUTPUT_OUT_1].setVoltage(rescale(static_cast<float>(frame.samples[0]), 0.f, 65535.f, -8.f, 8.f));
			outputs[OUTPUT_OUT_2].setVoltage(rescale(static_cast<float>(frame.samples[1]), 0.f, 65535.f, -8.f, 8.f));
		}
	}

	void changeControlMode() {
		uint16_t parameters[kNumKnobs];
		for (size_t knob = 0; knob < kNumKnobs; ++knob) {
			parameters[knob] = adcValue[knob];
		}

		switch (editMode) {
		case EDIT_MODE_TWIN:
			processors[0].CopyParameters(&parameters[0], kNumKnobs);
			processors[1].CopyParameters(&parameters[0], kNumKnobs);
			processors[0].set_control_mode(peaks::CONTROL_MODE_FULL);
			processors[1].set_control_mode(peaks::CONTROL_MODE_FULL);
			break;
		case EDIT_MODE_SPLIT:
			processors[0].CopyParameters(&parameters[0], 2);
			processors[1].CopyParameters(&parameters[2], 2);
			processors[0].set_control_mode(peaks::CONTROL_MODE_HALF);
			processors[1].set_control_mode(peaks::CONTROL_MODE_HALF);
			break;
		default:
			processors[0].set_control_mode(peaks::CONTROL_MODE_FULL);
			processors[1].set_control_mode(peaks::CONTROL_MODE_FULL);
			break;
		}
	}

	void setFunction(uint8_t index, ApicesProcessorFunction f) {
		if (editMode == EDIT_MODE_SPLIT || editMode == EDIT_MODE_TWIN) {
			processorFunction[0] = processorFunction[1] = f;
			processors[0].set_function(processorFunctionTable[f][0]);
			processors[1].set_function(processorFunctionTable[f][1]);
		} else {
			processorFunction[index] = f;
			processors[index].set_function(processorFunctionTable[f][index]);
		}
	}

	void processSwitch(uint16_t switchId) {
		switch (switchId) {
		case SWITCH_TWIN_MODE:
			if (editMode <= EDIT_MODE_SPLIT) {
				editMode = static_cast<EditMode>(EDIT_MODE_SPLIT - editMode);
			}
			changeControlMode();
			saveState();
			break;

		case SWITCH_EXPERT:
			editMode = static_cast<EditMode>((editMode + EDIT_MODE_FIRST) % EDIT_MODE_LAST);
			processorFunction[0] = processorFunction[1];
			processors[0].set_function(processorFunctionTable[processorFunction[0]][0]);
			processors[1].set_function(processorFunctionTable[processorFunction[0]][1]);
			lockPots();
			changeControlMode();
			saveState();
			break;

		case SWITCH_CHANNEL_SELECT:
			if (editMode >= EDIT_MODE_FIRST) {
				editMode = static_cast<EditMode>(EDIT_MODE_SECOND - (editMode & 1));

				switch (editMode) {
				case EDIT_MODE_FIRST:
					params[PARAM_MODE].setValue(processorFunction[0]);
					break;
				case EDIT_MODE_SECOND:
					params[PARAM_MODE].setValue(processorFunction[1]);
					break;
				default:
					break;
				}

				lockPots();
				changeControlMode();
				saveState();
			}
			break;
		}
	}

	void lockPots() {
		std::fill(&adcThreshold[0], &adcThreshold[kNumAdcChannels - 1], kAdcThresholdLocked);
		std::fill(&bSnapped[0], &bSnapped[kNumAdcChannels - 1], false);
	}

	void pollPots() {
		for (uint8_t knob = 0; knob < kNumAdcChannels; ++knob) {
			adcLp[knob] = (static_cast<int32_t>(params[PARAM_KNOB_1 + knob].getValue()) + adcLp[knob] * 7) >> 3;
			int32_t value = adcLp[knob];
			int32_t current_value = adcValue[knob];
			if (value >= current_value + adcThreshold[knob] || value <= current_value - adcThreshold[knob] || !adcThreshold[knob]) {
				onPotChanged(knob, value);
				adcValue[knob] = value;
				adcThreshold[knob] = kAdcThresholdUnlocked;
			}
		}
	}

	void onPotChanged(uint16_t knobId, uint16_t value) {
		switch (editMode) {
		case EDIT_MODE_TWIN:
			processors[0].set_parameter(knobId, value);
			processors[1].set_parameter(knobId, value);
			potValue[knobId] = value >> 8;
			break;

		case EDIT_MODE_SPLIT:
			if (knobId < 2) {
				processors[0].set_parameter(knobId, value);
			} else {
				processors[1].set_parameter(knobId - 2, value);
			}
			potValue[knobId] = value >> 8;
			break;

		case EDIT_MODE_FIRST:
		case EDIT_MODE_SECOND:
			uint8_t index;
			index = knobId + (editMode - EDIT_MODE_FIRST) * kNumKnobs;
			peaks::Processors* processor;
			processor = &processors[editMode - EDIT_MODE_FIRST];

			int16_t delta;
			delta = static_cast<int16_t>(potValue[index]) - static_cast<int16_t>(value >> 8);
			if (delta < 0) {
				delta = -delta;
			}

			if (!bSnapMode || bSnapped[knobId] || delta <= 2) {
				processor->set_parameter(knobId, value);
				potValue[index] = value >> 8;
				bSnapped[knobId] = true;
			}
			break;
		case EDIT_MODE_LAST:
			break;
		}
	}

	void pollSwitches(const ProcessArgs& args) {
		for (uint8_t button = 0; button < kButtonCount; ++button) {
			if (stSwitches[button].process(params[PARAM_EDIT_MODE + button].getValue())) {
				processSwitch(SWITCH_TWIN_MODE + button);
			}
		}
		refreshLeds(args);
	}

	void saveState() {
		settings.editMode = editMode;
		settings.processorFunction[0] = processorFunction[0];
		settings.processorFunction[1] = processorFunction[1];
		std::copy(&potValue[0], &potValue[7], &settings.potValue[0]);
		settings.snap_mode = bSnapMode;
		displayText1 = apicesModeList[settings.processorFunction[0]];
		displayText2 = apicesModeList[settings.processorFunction[1]];
	}

	void refreshLeds(const ProcessArgs& args) {

		// refreshLeds() is only updated every N samples, so make sure setBrightnessSmooth methods account for this
		const float sampleTime = args.sampleTime * kClockUpdateFrequency;

		long long systemTimeMs = getSystemTimeMs();

		uint8_t flash = (systemTimeMs >> 7) & 7;
		int currentLight;
		switch (editMode) {
		case EDIT_MODE_FIRST:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth((flash == 1) ? 1.f : 0.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.75f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
			for (size_t knob = 0; knob < kNumKnobs; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.5f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
			break;
		case EDIT_MODE_SECOND:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth((flash == 1 || flash == 3) ? 1.f : 0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.75f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.75f, sampleTime);
			for (size_t knob = 0; knob < kNumKnobs; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.5f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.5f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
			break;
		case EDIT_MODE_TWIN:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
			for (size_t knob = 0; knob < kNumKnobs; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.5f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.5f, sampleTime);
			}
			break;
		case EDIT_MODE_SPLIT:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
			for (int knob = 0; knob < 2; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.5f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
			for (size_t knob = 2; knob < kNumKnobs; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.5f, sampleTime);
			}
			break;
		default:
			break;
		}

		lights[LIGHT_SPLIT_MODE].setBrightnessSmooth((editMode == EDIT_MODE_SPLIT) ? 0.75f : 0.f, sampleTime);
		lights[LIGHT_EXPERT_MODE].setBrightnessSmooth((editMode & EDIT_MODE_FIRST) ? 0.75f : 0.f, sampleTime);

		ApicesProcessorFunction currentProcessorFunction = getProcessorFunction();
		for (size_t light = 0; light < kNumFunctionLights; ++light) {
			currentLight = LIGHT_FUNCTION_1 + light;
			switch (lightStates[currentProcessorFunction][light]) {
			case LIGHT_ON:
				lights[currentLight].setBrightnessSmooth(1.f, sampleTime);
				break;
			case LIGHT_OFF:
				lights[currentLight].setBrightnessSmooth(0.f, sampleTime);
				break;
			case LIGHT_BLINK:
				lights[currentLight].setBrightnessSmooth(systemTimeMs & 256 ? 0.f : 1.f, sampleTime);
				break;
			default:
				break;
			}
		}

		uint8_t buttonBrightness[kNumChannels];
		for (uint8_t channel = 0; channel < kNumChannels; ++channel) {
			switch (processorFunction[channel]) {
			case FUNCTION_DRUM_GENERATOR:
			case FUNCTION_FM_DRUM_GENERATOR:
				buttonBrightness[channel] = static_cast<int16_t>(abs(brightness[channel]) >> 8);
				buttonBrightness[channel] = buttonBrightness[channel] >= 255 ? 255 : buttonBrightness[channel];
				break;
			case FUNCTION_LFO:
			case FUNCTION_TAP_LFO:
			case FUNCTION_MINI_SEQUENCER:
				int32_t brightnessVal;
				brightnessVal = static_cast<int32_t>(brightness[channel]) * 409 >> 8;
				brightnessVal += 32768;
				brightnessVal >>= 8;
				brightnessVal = clamp(brightnessVal, 0, 255);
				buttonBrightness[channel] = brightnessVal;
				break;
			default:
				buttonBrightness[channel] = brightness[channel] >> 7;
				break;
			}
		}

		bool bIsChannel1Station = processors[0].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION;
		bool bIsChannel2Station = processors[1].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION;
		if (bIsChannel1Station || bIsChannel2Station) {
			if (editMode == EDIT_MODE_SPLIT || editMode == EDIT_MODE_TWIN) {
				uint8_t pattern = processors[0].number_station().digit() ^ processors[1].number_station().digit();
				for (size_t light = 0; light < kNumFunctionLights; ++light) {
					lights[LIGHT_FUNCTION_1 + light].setBrightness((pattern & 1) ? 1.f : 0.f);
					pattern = pattern >> 1;
				}
			}
			// Hacky but animates the lights!
			else if (editMode == EDIT_MODE_FIRST && bIsChannel1Station) {
				int digit = processors[0].number_station().digit();
				for (size_t light = 0; light < kNumFunctionLights; ++light) {
					lights[LIGHT_FUNCTION_1 + light].setBrightness((light & digit) ? 1.f : 0.f);
				}
			}
			// Ibid
			else if (editMode == EDIT_MODE_SECOND && bIsChannel2Station) {
				uint8_t digit = processors[1].number_station().digit();
				for (size_t light = 0; light < kNumFunctionLights; ++light) {
					lights[LIGHT_FUNCTION_1 + light].setBrightness((light & digit) ? 1.f : 0.f);
				}
			}
			if (bIsChannel1Station) {
				buttonBrightness[0] = processors[0].number_station().gate() ? 255 : 0;
			}
			if (bIsChannel2Station) {
				buttonBrightness[1] = processors[1].number_station().gate() ? 255 : 0;
			}
		}

		lights[LIGHT_TRIGGER_1].setBrightnessSmooth(rescale(static_cast<float>(buttonBrightness[0]), 0.f, 255.f, 0.f, 0.75f), sampleTime);
		lights[LIGHT_TRIGGER_2].setBrightnessSmooth(rescale(static_cast<float>(buttonBrightness[1]), 0.f, 255.f, 0.f, 0.75f), sampleTime);
	}

	void onReset() override {
		init();
	}

	void init() {
		std::fill(&potValue[0], &potValue[7], 0);
		std::fill(&brightness[0], &brightness[1], 0);
		std::fill(&adcLp[0], &adcLp[kNumAdcChannels - 1], 0);
		std::fill(&adcValue[0], &adcValue[kNumAdcChannels - 1], 0);
		std::fill(&adcThreshold[0], &adcThreshold[kNumAdcChannels - 1], 0);
		std::fill(&bSnapped[0], &bSnapped[kNumAdcChannels - 1], false);

		editMode = static_cast<EditMode>(settings.editMode);
		processorFunction[0] = static_cast<ApicesProcessorFunction>(settings.processorFunction[0]);
		processorFunction[1] = static_cast<ApicesProcessorFunction>(settings.processorFunction[1]);
		std::copy(&settings.potValue[0], &settings.potValue[7], &potValue[0]);

		if (editMode == EDIT_MODE_FIRST || editMode == EDIT_MODE_SECOND) {
			lockPots();
			for (uint8_t knob = 0; knob < kNumKnobs; ++knob) {
				processors[0].set_parameter(knob, static_cast<uint16_t>(potValue[knob]) << 8);
				processors[1].set_parameter(knob, static_cast<uint16_t>(potValue[knob + kNumKnobs]) << 8);
			}
		}

		bSnapMode = settings.snap_mode;

		changeControlMode();
		setFunction(0, processorFunction[0]);
		setFunction(1, processorFunction[1]);
	}

	void updateOleds() {
		if (editMode == EDIT_MODE_SPLIT) {
			oledText1 = apicesKnobLabelsSplitMode[processorFunction[0]].knob1;
			oledText2 = apicesKnobLabelsSplitMode[processorFunction[0]].knob2;
			oledText3 = apicesKnobLabelsSplitMode[processorFunction[0]].knob3;
			oledText4 = apicesKnobLabelsSplitMode[processorFunction[0]].knob4;
		} else {

			int currentFunction = -1;
			// same for both
			if (editMode == EDIT_MODE_TWIN) {
				currentFunction = processorFunction[0];
			}
			// if expert, pick the active set of labels
			else if (editMode == EDIT_MODE_FIRST || editMode == EDIT_MODE_SECOND) {
				currentFunction = processorFunction[editMode - EDIT_MODE_FIRST];
			} else {
				return;
			}

			std::string channelText = (editMode == EDIT_MODE_TWIN) ? "1&2. " : string::f("%d. ", editMode - EDIT_MODE_FIRST + 1);

			oledText1 = channelText + apicesKnobLabelsTwinMode[currentFunction].knob1;
			oledText2 = channelText + apicesKnobLabelsTwinMode[currentFunction].knob2;
			oledText3 = channelText + apicesKnobLabelsTwinMode[currentFunction].knob3;
			oledText4 = channelText + apicesKnobLabelsTwinMode[currentFunction].knob4;
		}
	}

	void switchExpanderChannel2Lights(Module* nixExpander, bool lightIsOn, const float sampleTime) {
		nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_2).setBrightnessSmooth(lightIsOn ? 0.5f : 0.f, sampleTime);

		for (int light = 0; light < kMaxFunctions; ++light) {
			nixExpander->getLight(Nix::LIGHT_PARAM_CHANNEL_2_1 + light).setBrightnessSmooth(lightIsOn, sampleTime);
		}
	}

	json_t* dataToJson() override {
		saveState();

		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "edit_mode", json_integer(static_cast<int>(settings.editMode)));
		json_object_set_new(rootJ, "fcn_channel_1", json_integer(static_cast<int>(settings.processorFunction[0])));
		json_object_set_new(rootJ, "fcn_channel_2", json_integer(static_cast<int>(settings.processorFunction[1])));

		json_t* potValuesJ = json_array();
		for (int pot : potValue) {
			json_t* pJ = json_integer(pot);
			json_array_append_new(potValuesJ, pJ);
		}
		json_object_set_new(rootJ, "pot_values", potValuesJ);

		json_object_set_new(rootJ, "snap_mode", json_boolean(settings.snap_mode));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* editModeJ = json_object_get(rootJ, "edit_mode");
		if (editModeJ) {
			settings.editMode = static_cast<EditMode>(json_integer_value(editModeJ));
		}

		json_t* fcnChannel1J = json_object_get(rootJ, "fcn_channel_1");
		if (fcnChannel1J) {
			settings.processorFunction[0] = static_cast<ApicesProcessorFunction>(json_integer_value(fcnChannel1J));
		}

		json_t* fcnChannel2J = json_object_get(rootJ, "fcn_channel_2");
		if (fcnChannel2J) {
			settings.processorFunction[1] = static_cast<ApicesProcessorFunction>(json_integer_value(fcnChannel2J));
		}

		json_t* snapModeJ = json_object_get(rootJ, "snap_mode");
		if (snapModeJ) {
			settings.snap_mode = json_boolean_value(snapModeJ);
		}

		json_t* potValuesJ = json_object_get(rootJ, "pot_values");
		size_t potValueId;
		json_t* pJ;
		json_array_foreach(potValuesJ, potValueId, pJ) {
			if (potValueId < sizeof(potValue) / sizeof(potValue)[0]) {
				settings.potValue[potValueId] = json_integer_value(pJ);
			}
		}

		// Update module internal state from settings.
		init();
		saveState();
	}

	inline Slice NextSlice(size_t size) {
		Slice s;
		s.block = &block[ioBlock];
		s.frame_index = ioFrame;
		ioFrame += size;
		if (ioFrame >= kBlockSize) {
			ioFrame -= kBlockSize;
			ioBlock = (ioBlock + 1) % kNumBlocks;
		}
		return s;
	}

	inline ApicesProcessorFunction getProcessorFunction() const {
		return editMode == EDIT_MODE_SECOND ? processorFunction[1] : processorFunction[0];
	}

	inline void setLedBrightness(int channel, int16_t value) {
		brightness[channel] = value;
	}

	inline void processChannels(Block* block, size_t size) {
		for (size_t channel = 0; channel < kNumChannels; ++channel) {
			processors[channel].Process(block->input[channel], output, size);
			setLedBrightness(channel, output[0]);
			for (size_t blockNum = 0; blockNum < size; ++blockNum) {
				// From calibration_data.h, shifting signed to unsigned values.
				int32_t shiftedValue = 32767 + static_cast<int32_t>(output[blockNum]);
				shiftedValue = clamp(shiftedValue, 0, 65535);
				block->output[channel][blockNum] = static_cast<uint16_t>(shiftedValue);
			}
		}
	}
};

struct ApicesWidget : SanguineModuleWidget {
	ApicesWidget(Apices* module) {
		setModule(module);

		moduleName = "apices";
		panelSize = SIZE_22;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* apicesFrambuffer = new FramebufferWidget();
		addChild(apicesFrambuffer);

		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(109.052, 5.573), module, Apices::LIGHT_EXPANDER));

		SanguineMatrixDisplay* displayChannel1 = new SanguineMatrixDisplay(12, module, 52.833, 27.965);
		apicesFrambuffer->addChild(displayChannel1);
		displayChannel1->fallbackString = apicesModeList[0];

		SanguineMatrixDisplay* displayChannel2 = new SanguineMatrixDisplay(12, module, 52.833, 40.557);
		apicesFrambuffer->addChild(displayChannel2);
		displayChannel2->fallbackString = apicesModeList[0];

		if (module) {
			displayChannel1->values.displayText = &module->displayText1;
			displayChannel2->values.displayText = &module->displayText2;
		}

		addParam(createParamCentered<Rogan2SGray>(millimetersToPixelsVec(99.527, 34.261), module, Apices::PARAM_MODE));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(56.011, 79.582),
			module, Apices::PARAM_EDIT_MODE, Apices::LIGHT_SPLIT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<BlueLight>>>(millimetersToPixelsVec(10.375, 50.212),
			module, Apices::PARAM_EXPERT_MODE, Apices::LIGHT_EXPERT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(10.375, 34.272),
			module, Apices::PARAM_CHANNEL_SELECT, Apices::LIGHT_CHANNEL_SELECT));

		addParam(createParamCentered<CKD6>(millimetersToPixelsVec(10.375, 69.669), module, Apices::PARAM_TRIGGER_1));
		addParam(createParamCentered<CKD6>(millimetersToPixelsVec(10.375, 115.9), module, Apices::PARAM_TRIGGER_2));

		addChild(createLightCentered<CKD6Light<RedLight>>(millimetersToPixelsVec(10.375, 69.669), module, Apices::LIGHT_TRIGGER_1));
		addChild(createLightCentered<CKD6Light<BlueLight>>(millimetersToPixelsVec(10.375, 115.9), module, Apices::LIGHT_TRIGGER_2));

		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(91.652, 25.986), module, Apices::LIGHT_FUNCTION_1));
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(107.402, 25.986), module, Apices::LIGHT_FUNCTION_2));
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(91.652, 42.136), module, Apices::LIGHT_FUNCTION_3));
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(107.402, 42.136), module, Apices::LIGHT_FUNCTION_4));

		addChild(createLightCentered<MediumLight<RedLight>>(millimetersToPixelsVec(16.113, 27.965), module, Apices::LIGHT_CHANNEL_1));
		addChild(createLightCentered<MediumLight<RedLight>>(millimetersToPixelsVec(16.113, 40.557), module, Apices::LIGHT_CHANNEL_2));

		addParam(createParamCentered<Sanguine2PSRed>(millimetersToPixelsVec(30.264, 62.728), module, Apices::PARAM_KNOB_1));
		addParam(createParamCentered<Sanguine2PSRed>(millimetersToPixelsVec(81.759, 62.728), module, Apices::PARAM_KNOB_2));
		addParam(createParamCentered<Sanguine2PSBlue>(millimetersToPixelsVec(30.264, 96.558), module, Apices::PARAM_KNOB_3));
		addParam(createParamCentered<Sanguine2PSBlue>(millimetersToPixelsVec(81.759, 96.558), module, Apices::PARAM_KNOB_4));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(41.987, 62.728), module, Apices::LIGHT_KNOBS_MODE + 0 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(69.978, 62.728), module, Apices::LIGHT_KNOBS_MODE + 1 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(41.987, 96.558), module, Apices::LIGHT_KNOBS_MODE + 2 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(millimetersToPixelsVec(69.978, 96.558), module, Apices::LIGHT_KNOBS_MODE + 3 * 3));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(10.375, 84.976), module, Apices::INPUT_GATE_1));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(10.375, 100.593), module, Apices::INPUT_GATE_2));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(101.388, 100.846), module, Apices::OUTPUT_OUT_1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(101.388, 116.989), module, Apices::OUTPUT_OUT_2));

		Sanguine96x32OLEDDisplay* oledDisplay1 = new Sanguine96x32OLEDDisplay(module, 30.264, 74.91);
		apicesFrambuffer->addChild(oledDisplay1);
		oledDisplay1->fallbackString = apicesKnobLabelsTwinMode[0].knob1;

		if (module)
			oledDisplay1->oledText = &module->oledText1;


		Sanguine96x32OLEDDisplay* oledDisplay2 = new Sanguine96x32OLEDDisplay(module, 81.759, 74.91);
		apicesFrambuffer->addChild(oledDisplay2);
		oledDisplay2->fallbackString = apicesKnobLabelsTwinMode[0].knob2;

		if (module)
			oledDisplay2->oledText = &module->oledText2;

		Sanguine96x32OLEDDisplay* oledDisplay3 = new Sanguine96x32OLEDDisplay(module, 30.264, 84.057);
		apicesFrambuffer->addChild(oledDisplay3);
		oledDisplay3->fallbackString = apicesKnobLabelsTwinMode[0].knob3;

		if (module)
			oledDisplay3->oledText = &module->oledText3;

		Sanguine96x32OLEDDisplay* oledDisplay4 = new Sanguine96x32OLEDDisplay(module, 81.759, 84.057);
		apicesFrambuffer->addChild(oledDisplay4);
		oledDisplay4->fallbackString = apicesKnobLabelsTwinMode[0].knob4;

		if (module)
			oledDisplay4->oledText = &module->oledText4;

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 59.118, 117.108);
		addChild(mutantsLogo);

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 46.116, 110.175);
		addChild(bloodLogo);
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		menu->addChild(new MenuSeparator);
		Apices* apices = dynamic_cast<Apices*>(this->module);

		menu->addChild(createBoolPtrMenuItem("Knob pickup (snap)", "", &apices->bSnapMode));
	}

};

Model* modelApices = createModel<Apices, ApicesWidget>("Sanguine-Apices");