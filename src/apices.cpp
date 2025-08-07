#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"

#include "peaks/processors.h"

#include "apices.hpp"
#ifndef METAMODULE
#include "nix.hpp"
#endif


#pragma GCC diagnostic ignored "-Wclass-memaccess"

/* TODO: MetaModule has no expander support at the moment, if it is ever implemented,
   restore expander code! (helpfully surrounded by ifdefs/ifndefs) */

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
		ENUMS(LIGHT_CHANNEL_SELECT, apicesCommon::kChannelCount),
		LIGHT_SPLIT_MODE,
		LIGHT_EXPERT_MODE,
		ENUMS(LIGHT_KNOBS_MODE, apicesCommon::kKnobCount * 3),
		LIGHT_FUNCTION_1,
		LIGHT_FUNCTION_2,
		LIGHT_FUNCTION_3,
		LIGHT_FUNCTION_4,
#ifndef METAMODULE
		LIGHT_EXPANDER,
#endif
		LIGHTS_COUNT
	};

	apicesCommon::EditModes editMode = apicesCommon::EDIT_MODE_TWIN;
	apicesCommon::EditModes lastEditMode = apicesCommon::EDIT_MODE_LAST;
	apices::ProcessorFunctions processorFunctions[apicesCommon::kChannelCount] = { apices::FUNCTION_ENVELOPE, apices::FUNCTION_ENVELOPE };
	apices::ProcessorFunctions lastProcessorFunctions[apicesCommon::kChannelCount] = { apices::FUNCTION_LAST, apices::FUNCTION_LAST };
	apicesCommon::Settings settings = {};

	uint8_t potValues[apicesCommon::kKnobCount * 2] = {};

	bool bSnapMode = false;
	bool bSnapped[apicesCommon::kKnobCount] = {};

#ifndef METAMODULE
	bool bHasExpander = false;
	bool bWasExpanderConnected = false;
#endif

	// Update descriptions/oleds every 16 samples.
	static const int kLightsFrequency = 16;

	int32_t adcLp[apicesCommon::kAdcChannelCount] = {};
	int32_t adcValue[apicesCommon::kAdcChannelCount] = {};
	int32_t adcThreshold[apicesCommon::kAdcChannelCount] = {};

	peaks::Processors processors[apicesCommon::kChannelCount] = {};

	int16_t output[apicesCommon::kBlockSize] = {};
	int16_t brightness[apicesCommon::kChannelCount] = {};

	dsp::SchmittTrigger stSwitches[apicesCommon::kButtonCount];
	dsp::ClockDivider lightsDivider;

	peaks::GateFlags gateFlags[apicesCommon::kChannelCount] = {};

	dsp::SampleRateConverter<apicesCommon::kChannelCount> srcOutput;
	dsp::DoubleRingBuffer<dsp::Frame<apicesCommon::kChannelCount>, 256> drbOutputBuffer;

	struct Block {
		peaks::GateFlags input[apicesCommon::kChannelCount][apicesCommon::kBlockSize] = {};
		uint16_t output[apicesCommon::kChannelCount][apicesCommon::kBlockSize] = {};
	};

	struct Slice {
		Block* block;
		size_t frame_index;
	};

	Block block[apicesCommon::kBlockCount] = {};
	size_t ioFrame = 0;
	size_t ioBlock = 0;
	size_t renderBlock = apicesCommon::kBlockCount / 2;

#ifndef METAMODULE
	Nix* nixExpander = nullptr;
#endif

	std::string displayText1 = "";
	std::string displayText2 = "";

#ifndef METAMODULE
	std::string oledText1 = "";
	std::string oledText2 = "";
	std::string oledText3 = "";
	std::string oledText4 = "";
#endif

	const peaks::ProcessorFunction processorFunctionTable[apices::FUNCTION_LAST][apicesCommon::kChannelCount] = {
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

		configParam(PARAM_KNOB_1, 0.f, 65535.f, 32678.f, "Knob 1", "%", 0.f, (1.f / 65535.f) * 100);
		configParam(PARAM_KNOB_2, 0.f, 65535.f, 32678.f, "Knob 2", "%", 0.f, (1.f / 65535.f) * 100);
		configParam(PARAM_KNOB_3, 0.f, 65535.f, 32678.f, "Knob 3", "%", 0.f, (1.f / 65535.f) * 100);
		configParam(PARAM_KNOB_4, 0.f, 65535.f, 32678.f, "Knob 4", "%", 0.f, (1.f / 65535.f) * 100);
		configButton(PARAM_EDIT_MODE, "Toggle split mode");
		configButton(PARAM_CHANNEL_SELECT, "Expert mode channel select");
		configButton(PARAM_EXPERT_MODE, "Toggle expert mode");
		configButton(PARAM_TRIGGER_1, "Trigger 1");
		configButton(PARAM_TRIGGER_2, "Trigger 2");

		settings.editMode = apicesCommon::EDIT_MODE_TWIN;
		settings.processorFunctions[0] = apices::FUNCTION_ENVELOPE;
		settings.processorFunctions[1] = apices::FUNCTION_ENVELOPE;
		settings.snapMode = false;

		for (size_t channel = 0; channel < apicesCommon::kChannelCount; ++channel)
		{
			memset(&processors[channel], 0, sizeof(peaks::Processors));
			processors[channel].Init(channel);
		}

		lightsDivider.setDivision(kLightsFrequency);

		init();
	}

	void process(const ProcessArgs& args) override {
#ifndef METAMODULE
		nixExpander = dynamic_cast<Nix*>(getRightExpander().module);
		bHasExpander = (nixExpander && nixExpander->getModel() == modelNix && !nixExpander->isBypassed());
#endif

		float sampleTime = 0.f;

		bool bIsLightsTurn = lightsDivider.process();

		// Update knobs / lights / displays every 16 samples only.
		if (bIsLightsTurn) {
			// For refreshLeds(): it is only updated every n samples, for proper light smoothing.
			sampleTime = args.sampleTime * kLightsFrequency;

			pollSwitches(args, sampleTime);
			pollPots();
			updateOleds();

#ifndef METAMODULE
			lights[LIGHT_EXPANDER].setBrightnessSmooth(bHasExpander ? kSanguineButtonLightValue : 0.f, sampleTime);
#endif
		}

		apices::ProcessorFunctions currentFunction = getProcessorFunction();
		if (params[PARAM_MODE].getValue() != currentFunction) {
			currentFunction = static_cast<apices::ProcessorFunctions>(params[PARAM_MODE].getValue());
			setFunction(editMode - apicesCommon::EDIT_MODE_FIRST, currentFunction);
			saveState();
		}

#ifndef METAMODULE
		if (bHasExpander) {
			bWasExpanderConnected = true;

			// Channel 1 strip.
			simd::float_4 expanderCVValues1;

			expanderCVValues1[0] = nixExpander->getInput(Nix::INPUT_PARAM_CV_1).getVoltage();
			expanderCVValues1[1] = nixExpander->getInput(Nix::INPUT_PARAM_CV_2).getVoltage();
			expanderCVValues1[2] = nixExpander->getInput(Nix::INPUT_PARAM_CV_3).getVoltage();
			expanderCVValues1[3] = nixExpander->getInput(Nix::INPUT_PARAM_CV_4).getVoltage();

			expanderCVValues1 /= 5.f;
			expanderCVValues1 = clamp(expanderCVValues1, -1.f, 1.f);
			expanderCVValues1 *= 255.f;

			expanderCVValues1[0] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_1).getValue();
			expanderCVValues1[1] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_2).getValue();
			expanderCVValues1[2] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_3).getValue();
			expanderCVValues1[3] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_4).getValue();

			simd::int32_4 expanderModulatedValues1 = expanderCVValues1;

			expanderModulatedValues1[0] += potValues[0];
			expanderModulatedValues1[1] += potValues[1];
			expanderModulatedValues1[2] += potValues[2];
			expanderModulatedValues1[3] += potValues[3];

			expanderModulatedValues1 = expanderModulatedValues1 << 8;

			expanderModulatedValues1 = clamp(expanderModulatedValues1, 0, 65535);

			// Channel 2 strip.
			simd::int32_4 expanderModulatedValues2 = {};

			if (editMode > apicesCommon::EDIT_MODE_SPLIT) {
				simd::float_4 expanderCVValues2 = {};

				expanderCVValues2[0] = nixExpander->getInput(Nix::INPUT_PARAM_CV_CHANNEL_2_1).getVoltage();
				expanderCVValues2[1] = nixExpander->getInput(Nix::INPUT_PARAM_CV_CHANNEL_2_2).getVoltage();
				expanderCVValues2[2] = nixExpander->getInput(Nix::INPUT_PARAM_CV_CHANNEL_2_3).getVoltage();
				expanderCVValues2[3] = nixExpander->getInput(Nix::INPUT_PARAM_CV_CHANNEL_2_4).getVoltage();

				expanderCVValues2 /= 5.f;
				expanderCVValues2 = clamp(expanderCVValues2, -1.f, 1.f);
				expanderCVValues2 *= 255.f;

				expanderCVValues2[0] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_CHANNEL_2_1).getValue();
				expanderCVValues2[1] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_CHANNEL_2_2).getValue();
				expanderCVValues2[2] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_CHANNEL_2_3).getValue();
				expanderCVValues2[3] *= nixExpander->getParam(Nix::PARAM_PARAM_CV_CHANNEL_2_4).getValue();

				expanderModulatedValues2 = expanderCVValues2;

				expanderModulatedValues2[0] += potValues[4];
				expanderModulatedValues2[1] += potValues[5];
				expanderModulatedValues2[2] += potValues[6];
				expanderModulatedValues2[3] += potValues[7];

				expanderModulatedValues2 = expanderModulatedValues2 << 8;

				expanderModulatedValues2 = clamp(expanderModulatedValues2, 0, 65535);
			}

			for (size_t knob = 0; knob < apicesCommon::kKnobCount; ++knob) {
				int channel1Input = Nix::INPUT_PARAM_CV_1 + knob;
				if (nixExpander->getInput(channel1Input).isConnected() ||
					nixExpander->getChannel1PortChanged(knob)) {
					switch (editMode) {
					case apicesCommon::EDIT_MODE_TWIN:
						processors[0].set_parameter(knob, expanderModulatedValues1[knob]);
						processors[1].set_parameter(knob, expanderModulatedValues1[knob]);
						break;

					case apicesCommon::EDIT_MODE_SPLIT:
						if (knob < 2) {
							processors[0].set_parameter(knob, expanderModulatedValues1[knob]);
						} else {
							processors[1].set_parameter(knob - 2, expanderModulatedValues1[knob]);
						}
						break;

					case apicesCommon::EDIT_MODE_FIRST:
					case apicesCommon::EDIT_MODE_SECOND:
						processors[0].set_parameter(knob, expanderModulatedValues1[knob]);
						break;
					default:
						break;
					}

					nixExpander->setChannel1PortChanged(knob, false);
				}

				if (editMode > apicesCommon::EDIT_MODE_SPLIT) {
					int channel2Input = Nix::INPUT_PARAM_CV_CHANNEL_2_1 + knob;

					if (nixExpander->getInput(channel2Input).isConnected() ||
						nixExpander->getChannel2PortChanged(knob)) {
						processors[1].set_parameter(knob, expanderModulatedValues2[knob]);

						nixExpander->setChannel2PortChanged(knob, false);
					}
				}

				if (bIsLightsTurn) {
					int currentLightRed = Nix::LIGHT_PARAM_1 + knob * 3;
					int currentLightGreen = (Nix::LIGHT_PARAM_1 + knob * 3) + 1;
					int currentLightBlue = (Nix::LIGHT_PARAM_1 + knob * 3) + 2;

					switch (editMode) {
					case apicesCommon::EDIT_MODE_TWIN:
						nixExpander->getLight(currentLightRed).setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						nixExpander->getLight(currentLightGreen).setBrightnessSmooth(0.f, sampleTime);
						nixExpander->getLight(currentLightBlue).setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						break;

					case apicesCommon::EDIT_MODE_SPLIT:
						if (knob < 2) {
							nixExpander->getLight(currentLightRed).setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
							nixExpander->getLight(currentLightGreen).setBrightnessSmooth(0.f, sampleTime);
							nixExpander->getLight(currentLightBlue).setBrightnessSmooth(0.f, sampleTime);
						} else {
							nixExpander->getLight(currentLightRed).setBrightnessSmooth(0.f, sampleTime);
							nixExpander->getLight(currentLightGreen).setBrightnessSmooth(0.f, sampleTime);
							nixExpander->getLight(currentLightBlue).setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						}
						break;

					case apicesCommon::EDIT_MODE_FIRST:
					case apicesCommon::EDIT_MODE_SECOND:
						nixExpander->getLight(currentLightRed).setBrightnessSmooth(0.f, sampleTime);
						nixExpander->getLight(currentLightGreen).setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						nixExpander->getLight(currentLightBlue).setBrightnessSmooth(0.f, sampleTime);
						break;
					default:
						break;
					}

					Light& channel1LightRed = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1);
					Light& channel1LightGreen = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1 + 1);
					Light& channel1LightBlue = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1 + 2);

					switch (editMode) {
					case apicesCommon::EDIT_MODE_FIRST:
					case apicesCommon::EDIT_MODE_SECOND:
						channel1LightRed.setBrightnessSmooth(0.f, sampleTime);
						channel1LightGreen.setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						channel1LightBlue.setBrightnessSmooth(0.f, sampleTime);
						switchExpanderChannel2Lights(true, sampleTime);
						break;
					case apicesCommon::EDIT_MODE_TWIN:
						channel1LightRed.setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						channel1LightGreen.setBrightnessSmooth(0.f, sampleTime);
						channel1LightBlue.setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						switchExpanderChannel2Lights(false, sampleTime);
						break;
					case apicesCommon::EDIT_MODE_SPLIT:
						channel1LightRed.setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
						channel1LightGreen.setBrightnessSmooth(0.f, sampleTime);
						channel1LightBlue.setBrightnessSmooth(0.f, sampleTime);
						switchExpanderChannel2Lights(false, sampleTime);
						break;
					default:
						break;
					}
				}
			}
		}
#endif

		if (drbOutputBuffer.empty()) {
			while (renderBlock != ioBlock) {
				processChannels(&block[renderBlock], apicesCommon::kBlockSize);
				renderBlock = (renderBlock + 1) % apicesCommon::kBlockCount;
			}

			uint32_t gateTriggers = 0;
			gateTriggers |= inputs[INPUT_GATE_1].getVoltage() >= 0.7f ? 1 : 0;
			gateTriggers |= inputs[INPUT_GATE_2].getVoltage() >= 0.7f ? 2 : 0;

			uint32_t buttons = 0;
			buttons |= (params[PARAM_TRIGGER_1].getValue() ? 1 : 0);
			buttons |= (params[PARAM_TRIGGER_2].getValue() ? 2 : 0);

			uint32_t gateInputs = gateTriggers | buttons;

			/* Prepare sample rate conversion.
			   Peaks is sampling at 48kHZ. */
			srcOutput.setRates(apicesCommon::kSampleRate, args.sampleRate);
			int inLen = apicesCommon::kBlockSize;
			int outLen = drbOutputBuffer.capacity();
			dsp::Frame<apicesCommon::kChannelCount> frame[apicesCommon::kBlockSize];

			// Process an entire block of data from the IOBuffer.
			for (size_t blockNum = 0; blockNum < apicesCommon::kBlockSize; ++blockNum) {

				Slice slice = NextSlice(1);

				for (size_t channel = 0; channel < apicesCommon::kChannelCount; ++channel) {
					gateFlags[channel] = peaks::ExtractGateFlags(gateFlags[channel], gateInputs & (1 << channel));

					frame[blockNum].samples[channel] = slice.block->output[channel][slice.frame_index];
				}

				/* A hack to make channel 1 aware of what's going on in channel 2. Used to
				   reset the sequencer. */
				slice.block->input[0][slice.frame_index] = gateFlags[0] | (gateFlags[1] << 4) | (buttons & 8 ? peaks::GATE_FLAG_FROM_BUTTON : 0);

				slice.block->input[1][slice.frame_index] = gateFlags[1] | (buttons & 2 ? peaks::GATE_FLAG_FROM_BUTTON : 0);
			}

			srcOutput.process(frame, &inLen, drbOutputBuffer.endData(), &outLen);
			drbOutputBuffer.endIncr(outLen);
		}

		// Update outputs.
		if (!drbOutputBuffer.empty()) {
			dsp::Frame<apicesCommon::kChannelCount> frame = drbOutputBuffer.shift();

			// Peaks manual says output spec is 0..8V for envelopes and 10Vpp for audio/CV.
			outputs[OUTPUT_OUT_1].setVoltage(rescale(static_cast<float>(frame.samples[0]), 0.f, 65535.f, -8.f, 8.f));
			outputs[OUTPUT_OUT_2].setVoltage(rescale(static_cast<float>(frame.samples[1]), 0.f, 65535.f, -8.f, 8.f));
		}
	}

	void changeControlMode() {
		uint16_t parameters[apicesCommon::kKnobCount];
		for (size_t knob = 0; knob < apicesCommon::kKnobCount; ++knob) {
			parameters[knob] = adcValue[knob];
		}

		switch (editMode) {
		case apicesCommon::EDIT_MODE_TWIN:
			processors[0].CopyParameters(&parameters[0], apicesCommon::kKnobCount);
			processors[1].CopyParameters(&parameters[0], apicesCommon::kKnobCount);
			processors[0].set_control_mode(peaks::CONTROL_MODE_FULL);
			processors[1].set_control_mode(peaks::CONTROL_MODE_FULL);
			break;
		case apicesCommon::EDIT_MODE_SPLIT:
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

	void setFunction(uint8_t index, apices::ProcessorFunctions f) {
		if (editMode < apicesCommon::EDIT_MODE_FIRST) {
			processorFunctions[0] = processorFunctions[1] = f;
			processors[0].set_function(processorFunctionTable[f][0]);
			processors[1].set_function(processorFunctionTable[f][1]);
		} else {
			processorFunctions[index] = f;
			processors[index].set_function(processorFunctionTable[f][index]);
		}
	}

	void processSwitch(uint16_t switchId) {
		switch (switchId) {
		case apicesCommon::SWITCH_TWIN_MODE:
			if (editMode <= apicesCommon::EDIT_MODE_SPLIT) {
				editMode = static_cast<apicesCommon::EditModes>(apicesCommon::EDIT_MODE_SPLIT - editMode);
			}
			changeControlMode();
			saveState();
			break;

		case apicesCommon::SWITCH_EXPERT:
			editMode = static_cast<apicesCommon::EditModes>((editMode + apicesCommon::EDIT_MODE_FIRST) % apicesCommon::EDIT_MODE_LAST);
			processorFunctions[0] = processorFunctions[1];
			processors[0].set_function(processorFunctionTable[processorFunctions[0]][0]);
			processors[1].set_function(processorFunctionTable[processorFunctions[0]][1]);
			lockPots();
			changeControlMode();
			saveState();
			break;

		case apicesCommon::SWITCH_CHANNEL_SELECT:
			if (editMode >= apicesCommon::EDIT_MODE_FIRST) {
				editMode = static_cast<apicesCommon::EditModes>(apicesCommon::EDIT_MODE_SECOND - (editMode & 1));

				switch (editMode) {
				case apicesCommon::EDIT_MODE_FIRST:
					params[PARAM_MODE].setValue(processorFunctions[0]);
					break;
				case apicesCommon::EDIT_MODE_SECOND:
					params[PARAM_MODE].setValue(processorFunctions[1]);
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
		std::fill(&adcThreshold[0],
			&adcThreshold[apicesCommon::kAdcChannelCount - 1], apicesCommon::kAdcThresholdLocked);
		std::fill(&bSnapped[0], &bSnapped[apicesCommon::kAdcChannelCount - 1], false);
	}

	void pollPots() {
		for (uint8_t knob = 0; knob < apicesCommon::kAdcChannelCount; ++knob) {
			adcLp[knob] = (static_cast<int32_t>(params[PARAM_KNOB_1 + knob].getValue()) + adcLp[knob] * 7) >> 3;
			int32_t value = adcLp[knob];
			int32_t current_value = adcValue[knob];
			if (value >= current_value + adcThreshold[knob] || value <= current_value - adcThreshold[knob] || !adcThreshold[knob]) {
				onPotChanged(knob, value);
				adcValue[knob] = value;
				adcThreshold[knob] = apicesCommon::kAdcThresholdUnlocked;
			}
		}
	}

	void onPotChanged(uint16_t knobId, uint16_t value) {
		switch (editMode) {
		case apicesCommon::EDIT_MODE_TWIN:
			processors[0].set_parameter(knobId, value);
			processors[1].set_parameter(knobId, value);
			potValues[knobId] = value >> 8;
			break;

		case apicesCommon::EDIT_MODE_SPLIT:
			if (knobId < 2) {
				processors[0].set_parameter(knobId, value);
			} else {
				processors[1].set_parameter(knobId - 2, value);
			}
			potValues[knobId] = value >> 8;
			break;

		case apicesCommon::EDIT_MODE_FIRST:
		case apicesCommon::EDIT_MODE_SECOND:
			uint8_t index;
			index = knobId + (editMode - apicesCommon::EDIT_MODE_FIRST) * apicesCommon::kKnobCount;
			peaks::Processors* processor;
			processor = &processors[editMode - apicesCommon::EDIT_MODE_FIRST];

			int16_t delta;
			delta = static_cast<int16_t>(potValues[index]) - static_cast<int16_t>(value >> 8);
			if (delta < 0) {
				delta = -delta;
			}

			if (!bSnapMode || bSnapped[knobId] || delta <= 2) {
				processor->set_parameter(knobId, value);
				potValues[index] = value >> 8;
				bSnapped[knobId] = true;
			}
			break;
		case apicesCommon::EDIT_MODE_LAST:
			break;
		}
	}

	void pollSwitches(const ProcessArgs& args, const float& sampleTime) {
		for (uint8_t button = 0; button < apicesCommon::kButtonCount; ++button) {
			if (stSwitches[button].process(params[PARAM_EDIT_MODE + button].getValue())) {
				processSwitch(apicesCommon::SWITCH_TWIN_MODE + button);
			}
		}

		refreshLeds(args, sampleTime);
	}

	void saveState() {
		settings.editMode = editMode;
		settings.processorFunctions[0] = processorFunctions[0];
		settings.processorFunctions[1] = processorFunctions[1];
		std::copy(&potValues[0], &potValues[7], &settings.potValues[0]);
		settings.snapMode = bSnapMode;
		displayText1 = apices::modeLabels[settings.processorFunctions[0]];
		displayText2 = apices::modeLabels[settings.processorFunctions[1]];
	}

	void refreshLeds(const ProcessArgs& args, const float& sampleTime) {
		long long systemTimeMs = getSystemTimeMs();

		uint8_t flash = (systemTimeMs >> 7) & 7;
		int currentLight;
		switch (editMode) {
		case apicesCommon::EDIT_MODE_FIRST:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth((flash == 1) ? 1.f : 0.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
			for (size_t knob = 0; knob < apicesCommon::kKnobCount; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
			break;
		case apicesCommon::EDIT_MODE_SECOND:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth((flash == 1 || flash == 3) ? 1.f : 0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
			for (size_t knob = 0; knob < apicesCommon::kKnobCount; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
			break;
		case apicesCommon::EDIT_MODE_TWIN:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
			for (size_t knob = 0; knob < apicesCommon::kKnobCount; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
			}
			break;
		case apicesCommon::EDIT_MODE_SPLIT:
			lights[LIGHT_CHANNEL_1].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_2].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
			for (int knob = 0; knob < 2; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
			}
			for (size_t knob = 2; knob < apicesCommon::kKnobCount; ++knob) {
				currentLight = LIGHT_KNOBS_MODE + knob * 3;
				lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(kSanguineButtonLightValue, sampleTime);
			}
			break;
		default:
			break;
		}

		lights[LIGHT_SPLIT_MODE].setBrightnessSmooth((editMode == apicesCommon::EDIT_MODE_SPLIT) ?
			kSanguineButtonLightValue : 0.f, sampleTime);
		lights[LIGHT_EXPERT_MODE].setBrightnessSmooth((editMode & apicesCommon::EDIT_MODE_FIRST) ?
			kSanguineButtonLightValue : 0.f, sampleTime);

		apices::ProcessorFunctions currentProcessorFunction = getProcessorFunction();
		for (size_t light = 0; light < apicesCommon::kFunctionLightCount; ++light) {
			currentLight = LIGHT_FUNCTION_1 + light;
			switch (apices::lightStates[currentProcessorFunction][light]) {
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

		uint8_t buttonBrightness[apicesCommon::kChannelCount];
		for (uint8_t channel = 0; channel < apicesCommon::kChannelCount; ++channel) {
			switch (processorFunctions[channel]) {
			case apices::FUNCTION_DRUM_GENERATOR:
			case apices::FUNCTION_FM_DRUM_GENERATOR:
				buttonBrightness[channel] = static_cast<int16_t>(abs(brightness[channel]) >> 8);
				buttonBrightness[channel] = buttonBrightness[channel] >= 255 ? 255 : buttonBrightness[channel];
				break;
			case apices::FUNCTION_LFO:
			case apices::FUNCTION_TAP_LFO:
			case apices::FUNCTION_MINI_SEQUENCER:
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
			if (editMode < apicesCommon::EDIT_MODE_FIRST) {
				uint8_t pattern = processors[0].number_station().digit() ^ processors[1].number_station().digit();
				for (size_t light = 0; light < apicesCommon::kFunctionLightCount; ++light) {
					lights[LIGHT_FUNCTION_1 + light].setBrightness((pattern & 1) ? 1.f : 0.f);
					pattern = pattern >> 1;
				}
			}
			// Hacky but animates the lights!
			else if (editMode == apicesCommon::EDIT_MODE_FIRST && bIsChannel1Station) {
				int digit = processors[0].number_station().digit();
				for (size_t light = 0; light < apicesCommon::kFunctionLightCount; ++light) {
					lights[LIGHT_FUNCTION_1 + light].setBrightness((light & digit) ? 1.f : 0.f);
				}
			}
			// Ibid.
			else if (editMode == apicesCommon::EDIT_MODE_SECOND && bIsChannel2Station) {
				uint8_t digit = processors[1].number_station().digit();
				for (size_t light = 0; light < apicesCommon::kFunctionLightCount; ++light) {
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

		lights[LIGHT_TRIGGER_1].setBrightnessSmooth(rescale(static_cast<float>(buttonBrightness[0]),
			0.f, 255.f, 0.f, kSanguineButtonLightValue), sampleTime);
		lights[LIGHT_TRIGGER_2].setBrightnessSmooth(rescale(static_cast<float>(buttonBrightness[1]),
			0.f, 255.f, 0.f, kSanguineButtonLightValue), sampleTime);
	}

	void onReset() override {
		init();
	}

	void init() {
		std::fill(&potValues[0], &potValues[7], 0);
		std::fill(&brightness[0], &brightness[1], 0);
		std::fill(&adcLp[0], &adcLp[apicesCommon::kAdcChannelCount - 1], 0);
		std::fill(&adcValue[0], &adcValue[apicesCommon::kAdcChannelCount - 1], 0);
		std::fill(&adcThreshold[0], &adcThreshold[apicesCommon::kAdcChannelCount - 1], 0);
		std::fill(&bSnapped[0], &bSnapped[apicesCommon::kAdcChannelCount - 1], false);

		editMode = static_cast<apicesCommon::EditModes>(settings.editMode);
		processorFunctions[0] = static_cast<apices::ProcessorFunctions>(settings.processorFunctions[0]);
		processorFunctions[1] = static_cast<apices::ProcessorFunctions>(settings.processorFunctions[1]);
		std::copy(&settings.potValues[0], &settings.potValues[7], &potValues[0]);

		if (editMode >= apicesCommon::EDIT_MODE_FIRST) {
			lockPots();
			for (uint8_t knob = 0; knob < apicesCommon::kKnobCount; ++knob) {
				processors[0].set_parameter(knob, static_cast<uint16_t>(potValues[knob]) << 8);
				processors[1].set_parameter(knob,
					static_cast<uint16_t>(potValues[knob + apicesCommon::kKnobCount]) << 8);
			}
		}

		bSnapMode = settings.snapMode;

		changeControlMode();
		setFunction(0, processorFunctions[0]);
		setFunction(1, processorFunctions[1]);
	}

	void updateOleds() {
		if (lastEditMode != editMode || lastProcessorFunctions[0] != processorFunctions[0] ||
			lastProcessorFunctions[1] != processorFunctions[1]) {
			if (editMode == apicesCommon::EDIT_MODE_SPLIT) {
				paramQuantities[PARAM_KNOB_1]->name = apices::knobLabelsSplitMode[processorFunctions[0]].knob1;
				paramQuantities[PARAM_KNOB_2]->name = apices::knobLabelsSplitMode[processorFunctions[0]].knob2;
				paramQuantities[PARAM_KNOB_3]->name = apices::knobLabelsSplitMode[processorFunctions[0]].knob3;
				paramQuantities[PARAM_KNOB_4]->name = apices::knobLabelsSplitMode[processorFunctions[0]].knob4;

#ifndef METAMODULE
				oledText1 = apices::displayLabelsSplitMode[processorFunctions[0]].knob1;
				oledText2 = apices::displayLabelsSplitMode[processorFunctions[0]].knob2;
				oledText3 = apices::displayLabelsSplitMode[processorFunctions[0]].knob3;
				oledText4 = apices::displayLabelsSplitMode[processorFunctions[0]].knob4;
#endif

				if (bHasExpander) {
					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_1]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob1 + apicesCommon::kSuffixCV;
					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_2]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob2 + apicesCommon::kSuffixCV;
					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_3]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob3 + apicesCommon::kSuffixCV;
					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_4]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob4 + apicesCommon::kSuffixCV;

					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_1]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob1;
					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_2]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob2;
					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_3]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob3;
					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_4]->name =
						apices::knobLabelsSplitMode[processorFunctions[0]].knob4;

					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_1]->name = apicesCommon::kInactivedLabel;
					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_2]->name = apicesCommon::kInactivedLabel;
					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_3]->name = apicesCommon::kInactivedLabel;
					nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_4]->name = apicesCommon::kInactivedLabel;

					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_1]->name = apicesCommon::kInactivedLabel;
					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_2]->name = apicesCommon::kInactivedLabel;
					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_3]->name = apicesCommon::kInactivedLabel;
					nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_4]->name = apicesCommon::kInactivedLabel;
				}
			} else {
				int currentFunction = -1;
				std::string channelTextDisplay;
				std::string channelTextKnob;
				// Same for both.
				if (editMode == apicesCommon::EDIT_MODE_TWIN) {
					currentFunction = processorFunctions[0];
					channelTextDisplay = "1&2. ";
					channelTextKnob = apicesCommon::kPrefixChannelTwin;

					if (bHasExpander) {
						std::string toolTipText;

						toolTipText = channelTextKnob + apices::knobLabelsTwinMode[currentFunction].knob1;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_1]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_1]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channelTextKnob + apices::knobLabelsTwinMode[currentFunction].knob2;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_2]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_2]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channelTextKnob + apices::knobLabelsTwinMode[currentFunction].knob3;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_3]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_3]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channelTextKnob + apices::knobLabelsTwinMode[currentFunction].knob4;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_4]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_4]->name = toolTipText + apicesCommon::kSuffixCV;

						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_1]->name = apicesCommon::kInactivedLabel;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_2]->name = apicesCommon::kInactivedLabel;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_3]->name = apicesCommon::kInactivedLabel;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_4]->name = apicesCommon::kInactivedLabel;

						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_1]->name = apicesCommon::kInactivedLabel;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_2]->name = apicesCommon::kInactivedLabel;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_3]->name = apicesCommon::kInactivedLabel;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_4]->name = apicesCommon::kInactivedLabel;
					}
				}
				// If expert, pick the active set of labels.
				else if (editMode >= apicesCommon::EDIT_MODE_FIRST) {
					int functionOffset = editMode - apicesCommon::EDIT_MODE_FIRST;
					int calculatedChannel = functionOffset + 1;

					currentFunction = processorFunctions[functionOffset];
					channelTextDisplay = string::f("%d. ", calculatedChannel);
					channelTextKnob = string::f(apicesCommon::kPrefixChannelExpert, calculatedChannel);

					if (bHasExpander) {
						int processor1Function = processorFunctions[0];
						int processor2Function = processorFunctions[1];

						const std::string channel1TextKnob = string::f(apicesCommon::kPrefixChannelExpert, 1);
						const std::string channel2TextKnob = string::f(apicesCommon::kPrefixChannelExpert, 2);

						std::string toolTipText;

						toolTipText = channel1TextKnob + apices::knobLabelsTwinMode[processor1Function].knob1;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_1]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_1]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channel1TextKnob + apices::knobLabelsTwinMode[processor1Function].knob2;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_2]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_2]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channel1TextKnob + apices::knobLabelsTwinMode[processor1Function].knob3;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_3]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_3]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channel1TextKnob + apices::knobLabelsTwinMode[processor1Function].knob4;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_4]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_4]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channel2TextKnob + apices::knobLabelsTwinMode[processor2Function].knob1;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_1]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_1]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channel2TextKnob + apices::knobLabelsTwinMode[processor2Function].knob2;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_2]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_2]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channel2TextKnob + apices::knobLabelsTwinMode[processor2Function].knob3;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_3]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_3]->name = toolTipText + apicesCommon::kSuffixCV;

						toolTipText = channel2TextKnob + apices::knobLabelsTwinMode[processor2Function].knob4;
						nixExpander->inputInfos[Nix::INPUT_PARAM_CV_CHANNEL_2_4]->name = toolTipText;
						nixExpander->paramQuantities[Nix::PARAM_PARAM_CV_CHANNEL_2_4]->name = toolTipText + apicesCommon::kSuffixCV;
					}
				}

				paramQuantities[PARAM_KNOB_1]->name = channelTextKnob +
					apices::knobLabelsTwinMode[currentFunction].knob1;
				paramQuantities[PARAM_KNOB_2]->name = channelTextKnob +
					apices::knobLabelsTwinMode[currentFunction].knob2;
				paramQuantities[PARAM_KNOB_3]->name = channelTextKnob +
					apices::knobLabelsTwinMode[currentFunction].knob3;
				paramQuantities[PARAM_KNOB_4]->name = channelTextKnob +
					apices::knobLabelsTwinMode[currentFunction].knob4;

#ifndef METAMODULE
				oledText1 = channelTextDisplay + apices::displayLabelsTwinMode[currentFunction].knob1;
				oledText2 = channelTextDisplay + apices::displayLabelsTwinMode[currentFunction].knob2;
				oledText3 = channelTextDisplay + apices::displayLabelsTwinMode[currentFunction].knob3;
				oledText4 = channelTextDisplay + apices::displayLabelsTwinMode[currentFunction].knob4;
#endif
			}
			lastEditMode = editMode;
			lastProcessorFunctions[0] = processorFunctions[0];
			lastProcessorFunctions[1] = processorFunctions[1];
		}
	}

#ifndef METAMODULE
	void setExpanderChannel1Lights(bool lightIsOn) {
		Light& channel1LightRed = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1);
		Light& channel1LightGreen = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1 + 1);
		Light& channel1LightBlue = nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_1 + 2);

		switch (editMode) {
		case apicesCommon::EDIT_MODE_FIRST:
		case apicesCommon::EDIT_MODE_SECOND:
			channel1LightRed.setBrightness(0.f);
			channel1LightGreen.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
			channel1LightBlue.setBrightness(0.f);
			setExpanderChannel2Lights(lightIsOn & true);
			break;
		case apicesCommon::EDIT_MODE_TWIN:
			channel1LightRed.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
			channel1LightGreen.setBrightness(0.f);
			channel1LightBlue.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
			setExpanderChannel2Lights(false);
			break;
		case apicesCommon::EDIT_MODE_SPLIT:
			channel1LightRed.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
			channel1LightGreen.setBrightness(0.f);
			channel1LightBlue.setBrightness(0.f);
			setExpanderChannel2Lights(false);
			break;
		default:
			break;
		}

		for (size_t function = 0; function < apicesCommon::kKnobCount; ++function) {
			Light& currentLightRed = nixExpander->getLight(Nix::LIGHT_PARAM_1 + function * 3);
			Light& currentLightGreen = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 1);
			Light& currentLightBlue = nixExpander->getLight((Nix::LIGHT_PARAM_1 + function * 3) + 2);

			switch (editMode) {
			case apicesCommon::EDIT_MODE_TWIN:
				currentLightRed.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
				currentLightGreen.setBrightness(0.f);
				currentLightBlue.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
				break;

			case apicesCommon::EDIT_MODE_SPLIT:
				if (function < 2) {
					currentLightRed.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
					currentLightGreen.setBrightness(0.f);
					currentLightBlue.setBrightness(0.f);
				} else {
					currentLightRed.setBrightness(0.f);
					currentLightGreen.setBrightness(0.f);
					currentLightBlue.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
				}
				break;

			case apicesCommon::EDIT_MODE_FIRST:
			case apicesCommon::EDIT_MODE_SECOND:
				currentLightRed.setBrightness(0.f);
				currentLightGreen.setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);
				currentLightBlue.setBrightness(0.f);
				break;
			default:
				break;
			}
		}
	}

	void setExpanderChannel2Lights(bool lightIsOn) {
		nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_2).setBrightness(lightIsOn ? kSanguineButtonLightValue : 0.f);

		for (size_t light = 0; light < apicesCommon::kKnobCount; ++light) {
			nixExpander->getLight(Nix::LIGHT_PARAM_CHANNEL_2_1 + light).setBrightness(lightIsOn);
		}
	}

	void switchExpanderChannel2Lights(bool lightIsOn, const float& sampleTime) {
		nixExpander->getLight(Nix::LIGHT_SPLIT_CHANNEL_2).setBrightnessSmooth(lightIsOn ? kSanguineButtonLightValue : 0.f, sampleTime);

		for (size_t light = 0; light < apicesCommon::kKnobCount; ++light) {
			nixExpander->getLight(Nix::LIGHT_PARAM_CHANNEL_2_1 + light).setBrightnessSmooth(lightIsOn, sampleTime);
		}
	}
#endif

	json_t* dataToJson() override {
		saveState();

		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "edit_mode", static_cast<int>(settings.editMode));
		setJsonInt(rootJ, "fcn_channel_1", static_cast<int>(settings.processorFunctions[0]));
		setJsonInt(rootJ, "fcn_channel_2", static_cast<int>(settings.processorFunctions[1]));
		setJsonBoolean(rootJ, "snap_mode", settings.snapMode);

		json_t* potValuesJ = json_array();
		for (int pot : potValues) {
			json_t* pJ = json_integer(pot);
			json_array_append_new(potValuesJ, pJ);
		}
		json_object_set_new(rootJ, "pot_values", potValuesJ);

		if (settings.processorFunctions[0] == apices::FUNCTION_TAP_LFO) {
			setJsonInt(rootJ, "tapLFO0", processors[0].getPhaseIncrement());
		}

		if (settings.processorFunctions[1] == apices::FUNCTION_TAP_LFO) {
			setJsonInt(rootJ, "tapLFO1", processors[1].getPhaseIncrement());
		}
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue;

		if (getJsonInt(rootJ, "edit_mode", intValue)) {
			settings.editMode = static_cast<apicesCommon::EditModes>(intValue);
		}

		if (getJsonInt(rootJ, "fcn_channel_1", intValue)) {
			settings.processorFunctions[0] = static_cast<apices::ProcessorFunctions>(intValue);
		}

		if (getJsonInt(rootJ, "fcn_channel_2", intValue)) {
			settings.processorFunctions[1] = static_cast<apices::ProcessorFunctions>(intValue);
		}

		getJsonBoolean(rootJ, "snap_mode", settings.snapMode);

		json_t* potValuesJ = json_object_get(rootJ, "pot_values");
		size_t potValueId;
		json_t* pJ;
		json_array_foreach(potValuesJ, potValueId, pJ) {
			if (potValueId < sizeof(potValues) / sizeof(potValues)[0]) {
				settings.potValues[potValueId] = json_integer_value(pJ);
			}
		}

		// Update module internal state from settings.
		init();
		saveState();

		if (settings.processorFunctions[0] == apices::FUNCTION_TAP_LFO) {
			if (getJsonInt(rootJ, "tapLFO0", intValue)) {
				processors[0].setPhaseIncrement(intValue);
			}
		}

		if (settings.processorFunctions[1] == apices::FUNCTION_TAP_LFO) {
			if (getJsonInt(rootJ, "tapLFO1", intValue)) {
				processors[1].setPhaseIncrement(intValue);
			}
		}
	}

	inline Slice NextSlice(size_t size) {
		Slice s;
		s.block = &block[ioBlock];
		s.frame_index = ioFrame;
		ioFrame += size;
		if (ioFrame >= apicesCommon::kBlockSize) {
			ioFrame -= apicesCommon::kBlockSize;
			ioBlock = (ioBlock + 1) % apicesCommon::kBlockCount;
		}
		return s;
	}

	inline apices::ProcessorFunctions getProcessorFunction() const {
		return editMode == apicesCommon::EDIT_MODE_SECOND ? processorFunctions[1] : processorFunctions[0];
	}

	inline void setLedBrightness(int channel, int16_t value) {
		brightness[channel] = value;
	}

	inline void processChannels(Block* block, size_t size) {
		for (size_t channel = 0; channel < apicesCommon::kChannelCount; ++channel) {
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

#ifndef METAMODULE
	void onBypass(const BypassEvent& e) override {
		if (bHasExpander) {
			nixExpander->getLight(Nix::LIGHT_MASTER_MODULE).setBrightness(0.f);
			setExpanderChannel1Lights(false);
		}
		Module::onBypass(e);
	}

	void onUnBypass(const UnBypassEvent& e) override {
		if (bHasExpander) {
			nixExpander->getLight(Nix::LIGHT_MASTER_MODULE).setBrightness(kSanguineButtonLightValue);
			setExpanderChannel1Lights(true);
		}
		Module::onUnBypass(e);
	}

	void onExpanderChange(const ExpanderChangeEvent& e) override {
		if (bWasExpanderConnected) {
			for (size_t knob = 0; knob < apicesCommon::kKnobCount; ++knob) {
				switch (editMode) {
				case apicesCommon::EDIT_MODE_TWIN:
					processors[0].set_parameter(knob, potValues[knob] << 8);
					processors[1].set_parameter(knob, potValues[knob] << 8);
					break;

				case apicesCommon::EDIT_MODE_SPLIT:
					if (knob < 2) {
						processors[0].set_parameter(knob, potValues[knob] << 8);
					} else {
						processors[1].set_parameter(knob, potValues[knob - 2] << 8);
					}
					break;

				case apicesCommon::EDIT_MODE_FIRST:
				case apicesCommon::EDIT_MODE_SECOND:
					processors[0].set_parameter(knob, potValues[knob] << 8);
					processors[1].set_parameter(knob, potValues[knob + apicesCommon::kChannel2Offset] << 8);
					break;
				default:
					break;
				}
			}
			bWasExpanderConnected = false;
		}
	}
#endif
};

struct ApicesWidget : SanguineModuleWidget {
	explicit ApicesWidget(Apices* module) {
		setModule(module);

		moduleName = "apices";
		panelSize = sanguineThemes::SIZE_22;
		backplateColor = sanguineThemes::PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* apicesFramebuffer = new FramebufferWidget();
		addChild(apicesFramebuffer);

#ifndef METAMODULE
		addChild(createLightCentered<SmallLight<OrangeLight>>(millimetersToPixelsVec(109.052, 5.573), module, Apices::LIGHT_EXPANDER));
#endif

		SanguineMatrixDisplay* displayChannel1 = new SanguineMatrixDisplay(12, module, 52.833, 27.965);
		apicesFramebuffer->addChild(displayChannel1);
		displayChannel1->fallbackString = apices::modeLabels[0];

		SanguineMatrixDisplay* displayChannel2 = new SanguineMatrixDisplay(12, module, 52.833, 40.557);
		apicesFramebuffer->addChild(displayChannel2);
		displayChannel2->fallbackString = apices::modeLabels[0];

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

#ifndef METAMODULE
		Sanguine96x32OLEDDisplay* oledDisplay1 = new Sanguine96x32OLEDDisplay(module, 30.264, 74.91);
		apicesFramebuffer->addChild(oledDisplay1);
		oledDisplay1->fallbackString = apices::displayLabelsTwinMode[0].knob1;

		Sanguine96x32OLEDDisplay* oledDisplay2 = new Sanguine96x32OLEDDisplay(module, 81.759, 74.91);
		apicesFramebuffer->addChild(oledDisplay2);
		oledDisplay2->fallbackString = apices::displayLabelsTwinMode[0].knob2;

		Sanguine96x32OLEDDisplay* oledDisplay3 = new Sanguine96x32OLEDDisplay(module, 30.264, 84.057);
		apicesFramebuffer->addChild(oledDisplay3);
		oledDisplay3->fallbackString = apices::displayLabelsTwinMode[0].knob3;

		Sanguine96x32OLEDDisplay* oledDisplay4 = new Sanguine96x32OLEDDisplay(module, 81.759, 84.057);
		apicesFramebuffer->addChild(oledDisplay4);
		oledDisplay4->fallbackString = apices::displayLabelsTwinMode[0].knob4;

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 59.118, 117.108);
		addChild(mutantsLogo);

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 46.116, 110.175);
		addChild(bloodLogo);
#endif

		if (module) {
#ifdef METAMODULE
			module->displayText1 = apices::modeLabels[0];
			module->displayText2 = apices::modeLabels[0];
#endif
			displayChannel1->values.displayText = &module->displayText1;
			displayChannel2->values.displayText = &module->displayText2;
#ifndef METAMODULE
			oledDisplay1->oledText = &module->oledText1;
			oledDisplay2->oledText = &module->oledText2;
			oledDisplay3->oledText = &module->oledText3;
			oledDisplay4->oledText = &module->oledText4;
#endif
		}
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		menu->addChild(new MenuSeparator);
		Apices* apices = dynamic_cast<Apices*>(this->module);

		menu->addChild(createBoolPtrMenuItem("Knob pickup (snap)", "", &apices->bSnapMode));

#ifndef METAMODULE
		menu->addChild(new MenuSeparator());
		if (apices->bHasExpander) {
			menu->addChild(createMenuLabel("Nix expander already connected"));
		} else {
			menu->addChild(createMenuItem("Add Nix expander", "", [=]() {
				apices->addExpander(modelNix, this);
				}));
		}
#endif
	}
};

Model* modelApices = createModel<Apices, ApicesWidget>("Sanguine-Apices");