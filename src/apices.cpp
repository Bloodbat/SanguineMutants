#include "plugin.hpp"
#include "peaks/processors.h"
#include "sanguinecomponents.hpp"

enum SwitchIndex {
	SWITCH_TWIN_MODE,
	SWITCH_EXPERT,
	SWITCH_CHANNEL_SELECT,
	SWITCH_GATE_TRIG_1,
	SWITCH_GATE_TRIG_2
};

enum EditMode {
	EDIT_MODE_TWIN,
	EDIT_MODE_SPLIT,
	EDIT_MODE_FIRST,
	EDIT_MODE_SECOND,
	EDIT_MODE_LAST
};

enum ProcessorFunction {
	FUNCTION_ENVELOPE,
	FUNCTION_LFO,
	FUNCTION_TAP_LFO,
	FUNCTION_DRUM_GENERATOR,
	FUNCTION_MINI_SEQUENCER,
	FUNCTION_PULSE_SHAPER,
	FUNCTION_PULSE_RANDOMIZER,
	FUNCTION_FM_DRUM_GENERATOR,
	FUNCTION_NUMBER_STATION,
	FUNCTION_BOUNCING_BALL,
	FUNCTION_LAST,
	FUNCTION_FIRST_ALTERNATE_FUNCTION = FUNCTION_MINI_SEQUENCER
};

struct Settings {
	uint8_t editMode;
	uint8_t processorFunction[2];
	uint8_t potValue[8];
	bool snap_mode;
};


static const size_t kNumBlocks = 2;
static const size_t kNumChannels = 2;
static const size_t kBlockSize = 4;
static const int32_t kLongPressDuration = 600;
static const uint8_t kNumAdcChannels = 4;
static const uint16_t kAdcThresholdUnlocked = 1 << (16 - 10);  // 10 bits
static const uint16_t kAdcThresholdLocked = 1 << (16 - 8);  // 8 bits

static const uint8_t kButtonCount = 3;

struct Apices : Module {
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
		PARAM_COUNT
	};
	enum InputIds {
		GATE_1_INPUT,
		GATE_2_INPUT,
		INPUT_COUNT
	};
	enum OutputIds {
		OUT_1_OUTPUT,
		OUT_2_OUTPUT,
		OUTPUT_COUNT
	};
	enum LightIds {
		LIGHT_TRIGGER_1,
		LIGHT_TRIGGER_2,
		LIGHT_CHANNEL1,
		LIGHT_CHANNEL2,
		ENUMS(LIGHT_CHANNEL_SELECT, 1 * 2),
		LIGHT_SPLIT_MODE,
		LIGHT_EXPERT_MODE,
		ENUMS(LIGHT_KNOBS_MODE, 4 * 3),
		LIGHT_FUNCTION_1,
		LIGHT_FUNCTION_2,
		LIGHT_FUNCTION_3,
		LIGHT_FUNCTION_4,
		LIGHT_COUNT
	};

	static const peaks::ProcessorFunction processorFunctionTable[FUNCTION_LAST][2];

	EditMode editMode = EDIT_MODE_TWIN;
	ProcessorFunction processorFunction[2] = { FUNCTION_ENVELOPE, FUNCTION_ENVELOPE };
	Settings settings;

	uint8_t potValue[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	bool snapMode = false;
	bool snapped[4] = { false, false, false, false };

	int32_t adcLp[kNumAdcChannels] = { 0, 0, 0, 0 };
	int32_t adcValue[kNumAdcChannels] = { 0, 0, 0, 0 };
	int32_t adcThreshold[kNumAdcChannels] = { 0, 0, 0, 0 };

	peaks::Processors processors[2] = {};

	int16_t output[kBlockSize] = {};
	int16_t brightness[kNumChannels] = { 0, 0 };

	dsp::SchmittTrigger switches[kButtonCount];

	// update descriptions/oleds every 16 samples
	static const int kClockUpdateFrequency = 16;
	dsp::ClockDivider clockDivider;

	peaks::GateFlags gate_flags[2] = { 0, 0 };

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

	Apices() {

		config(PARAM_COUNT, INPUT_COUNT, OUTPUT_COUNT, LIGHT_COUNT);

		configParam(PARAM_MODE, 0.0f, 9.0f, 0.0f, "Mode", "", 0.0f, 1.0f, 1.0f);
		paramQuantities[PARAM_MODE]->snapEnabled = true;

		configParam(PARAM_KNOB_1, 0.0f, 65535.0f, 32678.0f, "Knob 1", "", 0.f, 1.f / 65535.f);
		configParam(PARAM_KNOB_2, 0.0f, 65535.0f, 32678.0f, "Knob 2", "", 0.f, 1.f / 65535.f);
		configParam(PARAM_KNOB_3, 0.0f, 65535.0f, 32678.0f, "Knob 3", "", 0.f, 1.f / 65535.f);
		configParam(PARAM_KNOB_4, 0.0f, 65535.0f, 32678.0f, "Knob 4", "", 0.f, 1.f / 65535.f);
		configButton(PARAM_EDIT_MODE, "Toggle split mode");
		configButton(PARAM_CHANNEL_SELECT, "Expert mode channel select");
		configButton(PARAM_EXPERT_MODE, "Toggle expert mode");
		configButton(PARAM_TRIGGER_1, "Trigger 1");
		configButton(PARAM_TRIGGER_2, "Trigger 2");

		settings.editMode = EDIT_MODE_TWIN;
		settings.processorFunction[0] = FUNCTION_ENVELOPE;
		settings.processorFunction[1] = FUNCTION_ENVELOPE;
		settings.snap_mode = false;
		std::fill(&settings.potValue[0], &settings.potValue[8], 0);

		for (int i = 0; i < 2; i++)
		{
			memset(&processors[i], 0, sizeof(processors[i]));
			processors[i].Init(i);
		}

		clockDivider.setDivision(kClockUpdateFrequency);

		init();
	}

	void onReset() override {
		init();
	}

	void init() {
		std::fill(&potValue[0], &potValue[8], 0);
		std::fill(&brightness[0], &brightness[1], 0);
		std::fill(&adcLp[0], &adcLp[kNumAdcChannels], 0);
		std::fill(&adcValue[0], &adcValue[kNumAdcChannels], 0);
		std::fill(&adcThreshold[0], &adcThreshold[kNumAdcChannels], 0);
		std::fill(&snapped[0], &snapped[kNumAdcChannels], false);

		editMode = static_cast<EditMode>(settings.editMode);
		processorFunction[0] = static_cast<ProcessorFunction>(settings.processorFunction[0]);
		processorFunction[1] = static_cast<ProcessorFunction>(settings.processorFunction[1]);
		std::copy(&settings.potValue[0], &settings.potValue[8], &potValue[0]);

		if (editMode == EDIT_MODE_FIRST || editMode == EDIT_MODE_SECOND) {
			lockPots();
			for (uint8_t i = 0; i < 4; ++i) {
				processors[0].set_parameter(
					i,
					static_cast<uint16_t>(potValue[i]) << 8);
				processors[1].set_parameter(
					i,
					static_cast<uint16_t>(potValue[i + 4]) << 8);
			}
		}

		snapMode = settings.snap_mode;

		changeControlMode();
		setFunction(0, processorFunction[0]);
		setFunction(1, processorFunction[1]);
	}

	void updateOleds() {
		if (editMode == EDIT_MODE_SPLIT) {
			switch (processorFunction[0]) {
			case FUNCTION_ENVELOPE: {
				oledText1 = "1. Attack";
				oledText2 = "1. Decay";
				oledText3 = "2. Attack";
				oledText4 = "2. Decay";
				break;
			}
			case FUNCTION_LFO: {
				oledText1 = "1. Frequency";
				oledText2 = "1. Waveform";
				oledText3 = "2. Frequency";
				oledText4 = "2. Waveform";
				break;
			}
			case FUNCTION_TAP_LFO: {
				oledText1 = "1. Waveform";
				oledText2 = "1. Wave. Var.";
				oledText3 = "2. Waveform";
				oledText4 = "2. Wave. Var.";
				break;
			}
			case FUNCTION_DRUM_GENERATOR: {
				oledText1 = "1. BD Tone";
				oledText2 = "1. BD Decay";
				oledText3 = "2. SD Tone";
				oledText4 = "2. SD Snappy";
				break;
			}
			case FUNCTION_MINI_SEQUENCER: {
				oledText1 = "1. Step 1";
				oledText2 = "1. Step 2";
				oledText3 = "2. Step 1";
				oledText4 = "2. Step 2";
				break;
			}
			case FUNCTION_PULSE_SHAPER: {
				oledText1 = "1. Delay";
				oledText2 = "1. Repeats #";
				oledText3 = "2. Delay";
				oledText4 = "2. Repeats #";
				break;
			}
			case FUNCTION_PULSE_RANDOMIZER: {
				oledText1 = "1. Acc/Rgn. Prob";
				oledText2 = "1. Delay";
				oledText3 = "2. Acc/Rgn. Prob";
				oledText4 = "2. Delay";
				break;
			}
			case FUNCTION_FM_DRUM_GENERATOR: {
				oledText1 = "1. BD Morph";
				oledText2 = "1. BD Variation";
				oledText3 = "2. SD Morph";
				oledText4 = "2. SD Variation";
				break;
			}
			case FUNCTION_NUMBER_STATION: {
				oledText1 = "1. Frequency";
				oledText2 = "1. Var. Prob";
				oledText3 = "2. Frequency";
				oledText4 = "2. Var. Prob";
				break;
			}
			case FUNCTION_BOUNCING_BALL: {
				oledText1 = "1. Gravity";
				oledText2 = "1. Bounce";
				oledText3 = "2. Gravity";
				oledText4 = "2. Bounce";
				break;
			}
			default: break;
			}
		}
		else {

			int currentFunction = -1;
			// same for both
			if (editMode == EDIT_MODE_TWIN) {
				currentFunction = processorFunction[0];
			}
			// if expert, pick the active set of labels
			else if (editMode == EDIT_MODE_FIRST || editMode == EDIT_MODE_SECOND) {
				currentFunction = processorFunction[editMode - EDIT_MODE_FIRST];
			}
			else {
				return;
			}

			std::string channelText = (editMode == EDIT_MODE_TWIN) ? "1&2. " : string::f("%d. ", editMode - EDIT_MODE_FIRST + 1);

			switch (currentFunction) {
			case FUNCTION_ENVELOPE: {
				oledText1 = channelText + "Attack";
				oledText2 = channelText + "Decay";
				oledText3 = channelText + "Sustain";
				oledText4 = channelText + "Release";
				break;
			}
			case FUNCTION_LFO: {
				oledText1 = channelText + "Frequency";
				oledText2 = channelText + "Waveform";
				oledText3 = channelText + "Wave. Var";
				oledText4 = channelText + "Phase";
				break;
			}
			case FUNCTION_TAP_LFO: {
				oledText1 = channelText + "Amplitude";
				oledText2 = channelText + "Waveform";
				oledText3 = channelText + "Wave. Var";
				oledText4 = channelText + "Phase";
				break;
			}
			case FUNCTION_DRUM_GENERATOR: {
				oledText1 = channelText + "Base Freq";
				oledText2 = channelText + "Freq. Mod";
				oledText3 = channelText + "High Freq.";
				oledText4 = channelText + "Decay";
				break;
			}
			case FUNCTION_MINI_SEQUENCER: {
				oledText1 = channelText + "Step 1";
				oledText2 = channelText + "Step 2";
				oledText3 = channelText + "Step 3";
				oledText4 = channelText + "Step 4";
				break;
			}
			case FUNCTION_PULSE_SHAPER: {
				oledText1 = channelText + "Pre-delay";
				oledText2 = channelText + "Gate time";
				oledText3 = channelText + "Delay";
				oledText4 = channelText + "Repeats #";
				break;
			}
			case FUNCTION_PULSE_RANDOMIZER: {
				oledText1 = channelText + "Trg. Prob.";
				oledText2 = channelText + "Regen Prob.";
				oledText3 = channelText + "Delay time";
				oledText4 = channelText + "Jitter";
				break;
			}
			case FUNCTION_FM_DRUM_GENERATOR: {
				oledText1 = channelText + "Frequency";
				oledText2 = channelText + "FM Intens";
				oledText3 = channelText + "Env. Decay";
				oledText4 = channelText + "Color";
				break;
			}
			case FUNCTION_NUMBER_STATION: {
				oledText1 = channelText + "Frequency";
				oledText2 = channelText + "Var. Prob.";
				oledText3 = channelText + "Noise";
				oledText4 = channelText + "Distortion";
				break;
			}
			case FUNCTION_BOUNCING_BALL: {
				oledText1 = channelText + "Gravity";
				oledText2 = channelText + "Bounce";
				oledText3 = channelText + "Amplitude";
				oledText4 = channelText + "Velocity";
				break;
			}
			default: break;
			}
		}

	}

	json_t* dataToJson() override {

		saveState();

		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "edit_mode", json_integer((int)settings.editMode));
		json_object_set_new(rootJ, "fcn_channel_1", json_integer((int)settings.processorFunction[0]));
		json_object_set_new(rootJ, "fcn_channel_2", json_integer((int)settings.processorFunction[1]));

		json_t* potValuesJ = json_array();
		for (int p : potValue) {
			json_t* pJ = json_integer(p);
			json_array_append_new(potValuesJ, pJ);
		}
		json_object_set_new(rootJ, "pot_values", potValuesJ);

		json_object_set_new(rootJ, "snap_mode", json_boolean(settings.snap_mode));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* editModeJ = json_object_get(rootJ, "edit_mode");
		if (editModeJ) {
			settings.editMode = static_cast<EditMode>(json_integer_value(editModeJ));
		}

		json_t* fcnChannel1J = json_object_get(rootJ, "fcn_channel_1");
		if (fcnChannel1J) {
			settings.processorFunction[0] = static_cast<ProcessorFunction>(json_integer_value(fcnChannel1J));
		}

		json_t* fcnChannel2J = json_object_get(rootJ, "fcn_channel_2");
		if (fcnChannel2J) {
			settings.processorFunction[1] = static_cast<ProcessorFunction>(json_integer_value(fcnChannel2J));
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

	void process(const ProcessArgs& args) override {
		// only update knobs / lights every 16 samples
		if (clockDivider.process()) {
			pollSwitches();
			pollPots();
			updateOleds();
		}

		ProcessorFunction CurrentFunction = getProcessorFunction();
		if (params[PARAM_MODE].getValue() != CurrentFunction) {
			CurrentFunction = static_cast<ProcessorFunction>(params[PARAM_MODE].getValue());
			setFunction(editMode - EDIT_MODE_FIRST, CurrentFunction);
			saveState();
		}

		if (outputBuffer.empty()) {

			while (renderBlock != ioBlock) {
				processChannels(&block[renderBlock], kBlockSize);
				renderBlock = (renderBlock + 1) % kNumBlocks;
			}

			uint32_t external_gate_inputs = 0;
			external_gate_inputs |= (inputs[GATE_1_INPUT].getVoltage() ? 1 : 0);
			external_gate_inputs |= (inputs[GATE_2_INPUT].getVoltage() ? 2 : 0);

			uint32_t buttons = 0;
			buttons |= (params[PARAM_TRIGGER_1].getValue() ? 1 : 0);
			buttons |= (params[PARAM_TRIGGER_2].getValue() ? 2 : 0);

			uint32_t gate_inputs = external_gate_inputs | buttons;

			// Prepare sample rate conversion.
			// Peaks is sampling at 48kHZ.
			outputSrc.setRates(48000, args.sampleRate);
			int inLen = kBlockSize;
			int outLen = outputBuffer.capacity();
			dsp::Frame<2> f[kBlockSize];

			// Process an entire block of data from the IOBuffer.
			for (size_t k = 0; k < kBlockSize; ++k) {

				Slice slice = NextSlice(1);

				for (size_t i = 0; i < kNumChannels; ++i) {
					gate_flags[i] = peaks::ExtractGateFlags(
						gate_flags[i],
						gate_inputs & (1 << i));

					f[k].samples[i] = slice.block->output[i][slice.frame_index];
				}

				// A hack to make channel 1 aware of what's going on in channel 2. Used to
				// reset the sequencer.
				slice.block->input[0][slice.frame_index] = gate_flags[0] | (gate_flags[1] << 4) | (buttons & 8 ? peaks::GATE_FLAG_FROM_BUTTON : 0);

				slice.block->input[1][slice.frame_index] = gate_flags[1] | (buttons & 2 ? peaks::GATE_FLAG_FROM_BUTTON : 0);
			}

			outputSrc.process(f, &inLen, outputBuffer.endData(), &outLen);
			outputBuffer.endIncr(outLen);
		}

		// Update outputs.
		if (!outputBuffer.empty()) {
			dsp::Frame<2> f = outputBuffer.shift();

			// Peaks manual says output spec is 0..8V for envelopes and 10Vpp for audio/CV.
			// TODO Check the output values against an actual device.
			outputs[OUT_1_OUTPUT].setVoltage(rescale(static_cast<float>(f.samples[0]), 0.0f, 65535.f, -8.0f, 8.0f));
			outputs[OUT_2_OUTPUT].setVoltage(rescale(static_cast<float>(f.samples[1]), 0.0f, 65535.f, -8.0f, 8.0f));
		}
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

	inline ProcessorFunction getProcessorFunction() const {
		return editMode == EDIT_MODE_SECOND ? processorFunction[1] : processorFunction[0];
	}

	inline void setLedBrightness(int channel, int16_t value) {
		brightness[channel] = value;
	}

	inline void processChannels(Block* block, size_t size) {
		for (size_t i = 0; i < kNumChannels; ++i) {
			processors[i].Process(block->input[i], output, size);
			setLedBrightness(i, output[0]);
			for (size_t j = 0; j < size; ++j) {
				// From calibration_data.h, shifting signed to unsigned values.
				int32_t shiftedValue = 32767 + static_cast<int32_t>(output[j]);
				CONSTRAIN(shiftedValue, 0, 65535);
				block->output[i][j] = static_cast<uint16_t>(shiftedValue);
			}
		}
	}

	void changeControlMode();
	void setFunction(uint8_t index, ProcessorFunction f);
	void onPotChanged(uint16_t id, uint16_t value);
	void processSwitch(uint16_t id);
	void saveState();
	void lockPots();
	void pollSwitches();
	void pollPots();
	void refreshLeds();

	long long getSystemTimeMs();
};

const peaks::ProcessorFunction Apices::processorFunctionTable[FUNCTION_LAST][2] = {
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


void Apices::changeControlMode() {
	uint16_t parameters[4];
	for (int i = 0; i < 4; ++i) {
		parameters[i] = adcValue[i];
	}

	if (editMode == EDIT_MODE_SPLIT) {
		processors[0].CopyParameters(&parameters[0], 2);
		processors[1].CopyParameters(&parameters[2], 2);
		processors[0].set_control_mode(peaks::CONTROL_MODE_HALF);
		processors[1].set_control_mode(peaks::CONTROL_MODE_HALF);
	}
	else if (editMode == EDIT_MODE_TWIN) {
		processors[0].CopyParameters(&parameters[0], 4);
		processors[1].CopyParameters(&parameters[0], 4);
		processors[0].set_control_mode(peaks::CONTROL_MODE_FULL);
		processors[1].set_control_mode(peaks::CONTROL_MODE_FULL);
	}
	else {
		processors[0].set_control_mode(peaks::CONTROL_MODE_FULL);
		processors[1].set_control_mode(peaks::CONTROL_MODE_FULL);
	}
}

void Apices::setFunction(uint8_t index, ProcessorFunction f) {
	if (editMode == EDIT_MODE_SPLIT || editMode == EDIT_MODE_TWIN) {
		processorFunction[0] = processorFunction[1] = f;
		processors[0].set_function(processorFunctionTable[f][0]);
		processors[1].set_function(processorFunctionTable[f][1]);
	}
	else {
		processorFunction[index] = f;
		processors[index].set_function(processorFunctionTable[f][index]);
	}
}

void Apices::processSwitch(uint16_t id) {
	switch (id) {
	case SWITCH_TWIN_MODE: {
		if (editMode <= EDIT_MODE_SPLIT) {
			editMode = static_cast<EditMode>(EDIT_MODE_SPLIT - editMode);
		}
		changeControlMode();
		saveState();
		break;
	}

	case SWITCH_EXPERT: {
		editMode = static_cast<EditMode>(
			(editMode + EDIT_MODE_FIRST) % EDIT_MODE_LAST);
		processorFunction[0] = processorFunction[1];
		processors[0].set_function(processorFunctionTable[processorFunction[0]][0]);
		processors[1].set_function(processorFunctionTable[processorFunction[0]][1]);
		lockPots();
		changeControlMode();
		saveState();
		break;
	}

	case SWITCH_CHANNEL_SELECT: {
		if (editMode >= EDIT_MODE_FIRST) {
			editMode = static_cast<EditMode>(EDIT_MODE_SECOND - (editMode & 1));

			switch (editMode)
			{
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
}

void Apices::lockPots() {
	std::fill(
		&adcThreshold[0],
		&adcThreshold[kNumAdcChannels],
		kAdcThresholdLocked);
	std::fill(&snapped[0], &snapped[kNumAdcChannels], false);
}

void Apices::pollPots() {
	for (uint8_t i = 0; i < kNumAdcChannels; ++i) {
		adcLp[i] = (int32_t(params[PARAM_KNOB_1 + i].getValue()) + adcLp[i] * 7) >> 3;
		int32_t value = adcLp[i];
		int32_t current_value = adcValue[i];
		if (value >= current_value + adcThreshold[i] ||
			value <= current_value - adcThreshold[i] ||
			!adcThreshold[i]) {
			onPotChanged(i, value);
			adcValue[i] = value;
			adcThreshold[i] = kAdcThresholdUnlocked;
		}
	}
}

void Apices::onPotChanged(uint16_t id, uint16_t value) {
	switch (editMode) {
	case EDIT_MODE_TWIN:
		processors[0].set_parameter(id, value);
		processors[1].set_parameter(id, value);
		potValue[id] = value >> 8;
		break;

	case EDIT_MODE_SPLIT:
		if (id < 2) {
			processors[0].set_parameter(id, value);
		}
		else {
			processors[1].set_parameter(id - 2, value);
		}
		potValue[id] = value >> 8;
		break;

	case EDIT_MODE_FIRST:
	case EDIT_MODE_SECOND: {
		uint8_t index = id + (editMode - EDIT_MODE_FIRST) * 4;
		peaks::Processors* p = &processors[editMode - EDIT_MODE_FIRST];

		int16_t delta = static_cast<int16_t>(potValue[index]) - \
			static_cast<int16_t>(value >> 8);
		if (delta < 0) {
			delta = -delta;
		}

		if (!snapMode || snapped[id] || delta <= 2) {
			p->set_parameter(id, value);
			potValue[index] = value >> 8;
			snapped[id] = true;
		}
		break;
	}

	case EDIT_MODE_LAST:
		break;
	}
}

long long Apices::getSystemTimeMs() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();
}

void Apices::pollSwitches() {
	for (uint8_t i = 0; i < kButtonCount; ++i) {
		if (switches[i].process(params[PARAM_EDIT_MODE + i].getValue())) {
			processSwitch(SWITCH_TWIN_MODE + i);
		}
	}
	refreshLeds();
}

static const std::vector<std::string> modeList{
	"ENVELOPE",
	"LFO",
	"TAP LFO",
	"DRUM GENERAT",
	"SEQUENCER*",
	"TRG. SHAPE*",
	"TRG. RANDOM*",
	"DIGI DRUMS*",
	"NUMBER STAT&",
	"BOUNCE BALL@"
};

void Apices::saveState() {
	settings.editMode = editMode;
	settings.processorFunction[0] = processorFunction[0];
	settings.processorFunction[1] = processorFunction[1];
	std::copy(&potValue[0], &potValue[8], &settings.potValue[0]);
	settings.snap_mode = snapMode;
	displayText1 = modeList[settings.processorFunction[0]];
	displayText2 = modeList[settings.processorFunction[1]];
}

void Apices::refreshLeds() {

	// refreshLeds() is only updated every N samples, so make sure setBrightnessSmooth methods account for this
	const float sampleTime = APP->engine->getSampleTime() * kClockUpdateFrequency;

	uint8_t flash = (getSystemTimeMs() >> 7) & 7;
	int currentLight;
	switch (editMode) {
	case EDIT_MODE_FIRST:
		lights[LIGHT_CHANNEL1].setBrightnessSmooth((flash == 1) ? 1.0f : 0.0f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
		for (int i = 0; i < 4; i++) {
			currentLight = LIGHT_KNOBS_MODE + i * 3;
			lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.5f, sampleTime);
			lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
		}
		break;
	case EDIT_MODE_SECOND:
		lights[LIGHT_CHANNEL1].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth((flash == 1 || flash == 3) ? 1.0f : 0.0f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(1.f, sampleTime);
		for (int i = 0; i < 4; i++) {
			currentLight = LIGHT_KNOBS_MODE + i * 3;
			lights[currentLight + 0].setBrightnessSmooth(0.5f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.5f, sampleTime);
			lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
		}
		break;
	case EDIT_MODE_TWIN: {
		lights[LIGHT_CHANNEL1].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
		for (int i = 0; i < 4; i++) {
			currentLight = LIGHT_KNOBS_MODE + i * 3;
			lights[currentLight + 0].setBrightnessSmooth(0.5f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 2].setBrightnessSmooth(0.5f, sampleTime);
		}
		break;
	}
	case EDIT_MODE_SPLIT: {
		lights[LIGHT_CHANNEL1].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
		for (int i = 0; i < 2; i++) {
			currentLight = LIGHT_KNOBS_MODE + i * 3;
			lights[currentLight + 0].setBrightnessSmooth(0.5f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
		}
		for (int i = 2; i < 4; i++) {
			currentLight = LIGHT_KNOBS_MODE + i * 3;
			lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 2].setBrightnessSmooth(0.5f, sampleTime);
		}
		break;
	}
	default:
		break;
	}

	lights[LIGHT_SPLIT_MODE].setBrightnessSmooth((editMode == EDIT_MODE_SPLIT) ? 1.0f : 0.0f, sampleTime);
	lights[LIGHT_EXPERT_MODE].setBrightnessSmooth((editMode & 2) ? 1.0F : 0.F, sampleTime);

	if (getProcessorFunction() == FUNCTION_BOUNCING_BALL) {
		lights[LIGHT_FUNCTION_1].setBrightnessSmooth(1.0f, sampleTime);
		lights[LIGHT_FUNCTION_1 + 1].setBrightnessSmooth(0.f, sampleTime);
		if (getSystemTimeMs() & 256) {
			lights[LIGHT_FUNCTION_1 + 2].setBrightnessSmooth(0.f, sampleTime);
			lights[LIGHT_FUNCTION_1 + 3].setBrightnessSmooth(0.f, sampleTime);
		}
		else {
			lights[LIGHT_FUNCTION_1 + 2].setBrightnessSmooth(1.f, sampleTime);
			lights[LIGHT_FUNCTION_1 + 3].setBrightnessSmooth(1.f, sampleTime);
		}
	}
	else {
		if ((getSystemTimeMs() & 256) && getProcessorFunction() >= FUNCTION_FIRST_ALTERNATE_FUNCTION) {
			for (size_t i = 0; i < 4; ++i) {
				lights[LIGHT_FUNCTION_1 + i].setBrightnessSmooth(0.0f, sampleTime);
			}
		}
		else {
			for (size_t i = 0; i < 4; ++i) {
				lights[LIGHT_FUNCTION_1 + i].setBrightnessSmooth(((getProcessorFunction() & 3) == i) ? 1.0f : 0.0f, sampleTime);
			}
		}
	}

	uint8_t b[2];
	for (uint8_t i = 0; i < 2; ++i) {
		switch (processorFunction[i]) {
		case FUNCTION_DRUM_GENERATOR:
		case FUNCTION_FM_DRUM_GENERATOR:
			b[i] = (int16_t)abs(brightness[i]) >> 8;
			b[i] = b[i] >= 255 ? 255 : b[i];
			break;
		case FUNCTION_LFO:
		case FUNCTION_TAP_LFO:
		case FUNCTION_MINI_SEQUENCER: {
			int32_t brightnessVal = int32_t(brightness[i]) * 409 >> 8;
			brightnessVal += 32768;
			brightnessVal >>= 8;
			CONSTRAIN(brightnessVal, 0, 255);
			b[i] = brightnessVal;
			break;
		}
		default:
			b[i] = brightness[i] >> 7;
			break;
		}
	}

	bool channel1IsStation = processors[0].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION;
	bool channel2IsStation = processors[1].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION;
	if (channel1IsStation || channel2IsStation) {
		if (editMode == EDIT_MODE_SPLIT || editMode == EDIT_MODE_TWIN) {
			uint8_t pattern = processors[0].number_station().digit()
				^ processors[1].number_station().digit();
			for (size_t i = 0; i < 4; ++i) {
				lights[LIGHT_FUNCTION_1 + i].value = (pattern & 1) ? 1.0f : 0.0f;
				pattern = pattern >> 1;
			}
		}
		// Hacky but animates the lights!
		else if (editMode == EDIT_MODE_FIRST && channel1IsStation) {
			int digit = processors[0].number_station().digit();
			for (size_t i = 0; i < 4; i++) {
				lights[LIGHT_FUNCTION_1 + i].value = (i & digit) ? 1.0f : 0.0f;
			}
		}
		// Ibid
		else if (editMode == EDIT_MODE_SECOND && channel2IsStation) {
			uint8_t digit = processors[1].number_station().digit();
			for (size_t i = 0; i < 4; i++) {
				lights[LIGHT_FUNCTION_1 + i].value = (i & digit) ? 1.0f : 0.0f;
			}
		}
		if (channel1IsStation) {
			b[0] = processors[0].number_station().gate() ? 255 : 0;
		}
		if (channel2IsStation) {
			b[1] = processors[1].number_station().gate() ? 255 : 0;
		}
	}

	const float deltaTime = APP->engine->getSampleTime();
	lights[LIGHT_TRIGGER_1].setSmoothBrightness(rescale(static_cast<float>(b[0]), 0.0f, 255.0f, 0.0f, 1.0f), deltaTime);
	lights[LIGHT_TRIGGER_2].setSmoothBrightness(rescale(static_cast<float>(b[1]), 0.0f, 255.0f, 0.0f, 1.0f), deltaTime);
}

struct ApicesWidget : ModuleWidget {
	ApicesWidget(Apices* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/apices_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* apicesFrambuffer = new FramebufferWidget();
		addChild(apicesFrambuffer);

		SanguineMatrixDisplay* displayChannel1 = new SanguineMatrixDisplay(12);
		displayChannel1->box.pos = mm2px(Vec(18.616, 22.885));
		displayChannel1->module = module;
		apicesFrambuffer->addChild(displayChannel1);

		SanguineMatrixDisplay* displayChannel2 = new SanguineMatrixDisplay(12);
		displayChannel2->box.pos = mm2px(Vec(18.616, 35.477));
		displayChannel2->module = module;
		apicesFrambuffer->addChild(displayChannel2);

		if (module) {
			displayChannel1->values.displayText = &module->displayText1;
			displayChannel2->values.displayText = &module->displayText2;
		}

		addParam(createParamCentered<Rogan2SGray>(mm2px(Vec(99.527, 34.261)), module, Apices::PARAM_MODE));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(mm2px(Vec(56.011, 79.582)),
			module, Apices::PARAM_EDIT_MODE, Apices::LIGHT_SPLIT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<BlueLight>>>(mm2px(Vec(10.375, 50.212)),
			module, Apices::PARAM_EXPERT_MODE, Apices::LIGHT_EXPERT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(mm2px(Vec(10.375, 34.272)),
			module, Apices::PARAM_CHANNEL_SELECT, Apices::LIGHT_CHANNEL_SELECT));

		addParam(createParamCentered<LEDBezel>(mm2px(Vec(10.375, 67.669)), module, Apices::PARAM_TRIGGER_1));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(10.375, 117.9)), module, Apices::PARAM_TRIGGER_2));

		addChild(createLightCentered<LEDBezelLight<RedLight>>(mm2px(Vec(10.375, 67.669)), module, Apices::LIGHT_TRIGGER_1));
		addChild(createLightCentered<LEDBezelLight<BlueLight>>(mm2px(Vec(10.375, 117.9)), module, Apices::LIGHT_TRIGGER_2));

		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(91.652, 25.986)), module, Apices::LIGHT_FUNCTION_1));
		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(107.402, 25.986)), module, Apices::LIGHT_FUNCTION_2));
		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(91.652, 42.136)), module, Apices::LIGHT_FUNCTION_3));
		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(107.402, 42.136)), module, Apices::LIGHT_FUNCTION_4));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(16.113, 27.965)), module, Apices::LIGHT_CHANNEL1));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(16.113, 40.557)), module, Apices::LIGHT_CHANNEL2));

		addParam(createParamCentered<Rogan2PSRed>(mm2px(Vec(30.264, 62.728)), module, Apices::PARAM_KNOB_1));
		addParam(createParamCentered<Rogan2PSRed>(mm2px(Vec(81.759, 62.728)), module, Apices::PARAM_KNOB_2));
		addParam(createParamCentered<Rogan2PSBlue>(mm2px(Vec(30.264, 96.558)), module, Apices::PARAM_KNOB_3));
		addParam(createParamCentered<Rogan2PSBlue>(mm2px(Vec(81.759, 96.558)), module, Apices::PARAM_KNOB_4));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(41.987, 62.728)), module, Apices::LIGHT_KNOBS_MODE + 0 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(69.978, 62.728)), module, Apices::LIGHT_KNOBS_MODE + 1 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(41.987, 96.558)), module, Apices::LIGHT_KNOBS_MODE + 2 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(69.978, 96.558)), module, Apices::LIGHT_KNOBS_MODE + 3 * 3));

		addInput(createInputCentered<BananutGreen>((mm2px(Vec(10.375, 84.976))), module, Apices::GATE_1_INPUT));
		addInput(createInputCentered<BananutGreen>((mm2px(Vec(10.375, 100.593))), module, Apices::GATE_2_INPUT));

		addOutput(createOutputCentered<BananutRed>((mm2px(Vec(101.388, 100.846))), module, Apices::OUT_1_OUTPUT));
		addOutput(createOutputCentered<BananutRed>((mm2px(Vec(101.388, 116.989))), module, Apices::OUT_2_OUTPUT));	

		Sanguine96x32OLEDDisplay* oledDisplay1 = new Sanguine96x32OLEDDisplay();
		oledDisplay1->box.pos = mm2px(Vec(22.115, 72.201));
		oledDisplay1->module = module;
		if (module)
			oledDisplay1->oledText = &module->oledText1;
		apicesFrambuffer->addChild(oledDisplay1);

		Sanguine96x32OLEDDisplay* oledDisplay2 = new Sanguine96x32OLEDDisplay();
		oledDisplay2->box.pos = mm2px(Vec(73.609, 72.201));
		oledDisplay2->module = module;
		if (module)
			oledDisplay2->oledText = &module->oledText2;
		apicesFrambuffer->addChild(oledDisplay2);

		Sanguine96x32OLEDDisplay* oledDisplay3 = new Sanguine96x32OLEDDisplay();
		oledDisplay3->box.pos = mm2px(Vec(22.115, 81.348));
		oledDisplay3->module = module;
		if (module)
			oledDisplay3->oledText = &module->oledText3;
		apicesFrambuffer->addChild(oledDisplay3);

		Sanguine96x32OLEDDisplay* oledDisplay4 = new Sanguine96x32OLEDDisplay();
		oledDisplay4->box.pos = mm2px(Vec(73.609, 81.348));
		oledDisplay4->module = module;
		if (module)
			oledDisplay4->oledText = &module->oledText4;
		apicesFrambuffer->addChild(oledDisplay4);

		SanguineShapedLight* mutantsLogo = new SanguineShapedLight();
		mutantsLogo->box.pos = mm2px(Vec(53.01, 114.607));
		mutantsLogo->box.size = Vec(36.06, 14.79);
		mutantsLogo->module = module;
		mutantsLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/mutants_glowy.svg")));
		apicesFrambuffer->addChild(mutantsLogo);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight();
		bloodLogo->box.pos = mm2px(Vec(44.219, 106.239));
		bloodLogo->box.size = Vec(11.2, 23.27);
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		apicesFrambuffer->addChild(bloodLogo);
	}

	void appendContextMenu(Menu* menu) override {

		menu->addChild(new MenuSeparator);
		Apices* peaks = dynamic_cast<Apices*>(this->module);

		menu->addChild(createBoolPtrMenuItem("Knob pickup (snap)", "", &peaks->snapMode));
	}

};

Model* modelApices = createModel<Apices, ApicesWidget>("Sanguine-Apices");