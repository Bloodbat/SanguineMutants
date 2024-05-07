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

enum Function {
	FUNCTION_ENVELOPE,
	FUNCTION_LFO,
	FUNCTION_TAP_LFO,
	FUNCTION_DRUM_GENERATOR,
	FUNCTION_MINI_SEQUENCER,
	FUNCTION_PULSE_SHAPER,
	FUNCTION_PULSE_RANDOMIZER,
	FUNCTION_FM_DRUM_GENERATOR,
	FUNCTION_RADIO_STATION,
	FUNCTION_LAST,
	FUNCTION_FIRST_ALTERNATE_FUNCTION = FUNCTION_MINI_SEQUENCER
};

struct Settings {
	uint8_t edit_mode;
	uint8_t function[2];
	uint8_t pot_value[8];
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
		NUM_PARAMS
	};
	enum InputIds {
		GATE_1_INPUT,
		GATE_2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_1_OUTPUT,
		OUT_2_OUTPUT,
		NUM_OUTPUTS
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
		NUM_LIGHTS
	};

	static const peaks::ProcessorFunction function_table_[FUNCTION_LAST][2];

	EditMode edit_mode_ = EDIT_MODE_TWIN;
	Function function_[2] = { FUNCTION_ENVELOPE, FUNCTION_ENVELOPE };
	Settings settings_;

	uint8_t pot_value_[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	bool snap_mode_ = false;
	bool snapped_[4] = { false, false, false, false };

	int32_t adc_lp_[kNumAdcChannels] = { 0, 0, 0, 0 };
	int32_t adc_value_[kNumAdcChannels] = { 0, 0, 0, 0 };
	int32_t adc_threshold_[kNumAdcChannels] = { 0, 0, 0, 0 };

	int function1 = 0;
	int function2 = 0;

	peaks::Processors processors[2] = {};

	int16_t output[kBlockSize] = {};
	int16_t brightness[kNumChannels] = { 0, 0 };

	dsp::SchmittTrigger switches_[kButtonCount];

	// update descriptions/oleds every 16 samples
	static const int cvUpdateFrequency = 16;
	dsp::ClockDivider cvDivider;

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

	Block block_[kNumBlocks] = {};
	size_t io_frame_ = 0;
	size_t io_block_ = 0;
	size_t render_block_ = kNumBlocks / 2;

	std::string oledText1 = "";
	std::string oledText2 = "";
	std::string oledText3 = "";
	std::string oledText4 = "";

	Apices() {

		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(PARAM_MODE, 0.0f, 8.0f, 0.0f, "Mode", "", 0.0f, 1.0f, 1.0f);
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

		//getParamQuantity(PARAM_BUTTON_2)->description = "Long press to enable/disable Easter Egg Modes";

		settings_.edit_mode = EDIT_MODE_TWIN;
		settings_.function[0] = FUNCTION_ENVELOPE;
		settings_.function[1] = FUNCTION_ENVELOPE;
		settings_.snap_mode = false;
		std::fill(&settings_.pot_value[0], &settings_.pot_value[8], 0);

		memset(&processors[0], 0, sizeof(processors[0]));
		memset(&processors[1], 0, sizeof(processors[1]));
		processors[0].Init(0);
		processors[1].Init(1);

		cvDivider.setDivision(cvUpdateFrequency);

		init();
	}

	void onReset() override {
		init();
	}

	void init() {
		std::fill(&pot_value_[0], &pot_value_[8], 0);
		std::fill(&brightness[0], &brightness[1], 0);
		std::fill(&adc_lp_[0], &adc_lp_[kNumAdcChannels], 0);
		std::fill(&adc_value_[0], &adc_value_[kNumAdcChannels], 0);
		std::fill(&adc_threshold_[0], &adc_threshold_[kNumAdcChannels], 0);
		std::fill(&snapped_[0], &snapped_[kNumAdcChannels], false);

		edit_mode_ = static_cast<EditMode>(settings_.edit_mode);
		function_[0] = static_cast<Function>(settings_.function[0]);
		function_[1] = static_cast<Function>(settings_.function[1]);
		std::copy(&settings_.pot_value[0], &settings_.pot_value[8], &pot_value_[0]);

		if (edit_mode_ == EDIT_MODE_FIRST || edit_mode_ == EDIT_MODE_SECOND) {
			lockPots();
			for (uint8_t i = 0; i < 4; ++i) {
				processors[0].set_parameter(
					i,
					static_cast<uint16_t>(pot_value_[i]) << 8);
				processors[1].set_parameter(
					i,
					static_cast<uint16_t>(pot_value_[i + 4]) << 8);
			}
		}

		snap_mode_ = settings_.snap_mode;

		changeControlMode();
		setFunction(0, function_[0]);
		setFunction(1, function_[1]);
	}

	void updateOleds() {
		if (edit_mode_ == EDIT_MODE_SPLIT) {
			switch (function_[0]) {
			case FUNCTION_ENVELOPE: {
				oledText1.assign("1. Attack");
				oledText2 = "1. Decay";
				oledText3 = "2. Attack";
				oledText3 = "2. Decay";
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
				oledText2 = "1. Wave. Var";
				oledText3 = "2. Waveform";
				oledText4 = "2. Wave. Var";
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
				oledText1 = "1. Acc./rgn. prob";
				oledText2 = "1. Delay";
				oledText3 = "2. Acc./rgn. Prob";
				oledText4 = "2. Delay";
				break;
			}
			case FUNCTION_FM_DRUM_GENERATOR: {
				oledText1 = "1. BD prst. Mrph";
				oledText2 = "1. BD prst. Var";
				oledText3 = "2. SD prst. Mrph";
				oledText4 = "2. SD prst. Var";
				break;
			}
			case FUNCTION_RADIO_STATION: {
				oledText1 = "1. Frequency";
				oledText2 = "1. Var. Prob";
				oledText3 = "2. frequency";
				oledText4 = "2. Var. Prob";
				break;
			}
			default: break;
			}
		}
		else {

			int currentFunction = -1;
			// same for both
			if (edit_mode_ == EDIT_MODE_TWIN) {
				currentFunction = function_[0]; 	// == function_[1]
			}
			// if expert, pick the active set of labels
			else if (edit_mode_ == EDIT_MODE_FIRST || edit_mode_ == EDIT_MODE_SECOND) {
				currentFunction = function_[edit_mode_ - EDIT_MODE_FIRST];
			}
			else {
				return;
			}

			std::string channelText = (edit_mode_ == EDIT_MODE_TWIN) ? "1&2. " : string::f("%d. ", edit_mode_ - EDIT_MODE_FIRST + 1);

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
				oledText1 = channelText + "Base freq";
				oledText2 = channelText + "Freq. Mod";
				oledText3 = channelText + "High freq.";
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
				oledText1 = channelText + "In. Trg. Prob";
				oledText2 = channelText + "Tr. Reg. Prob";
				oledText3 = channelText + "Delay time";
				oledText4 = channelText + "Jitter";
				break;
			}
			case FUNCTION_FM_DRUM_GENERATOR: {
				oledText1 = channelText + "Frequency";
				oledText2 = channelText + "FM intensity";
				oledText3 = channelText + "Env decay";
				oledText4 = channelText + "Color";
				break;
			}
			case FUNCTION_RADIO_STATION: {
				oledText1 = channelText + "Frequency";
				oledText2 = channelText + "Var. Prob";
				oledText3 = channelText + "Noise";
				oledText4 = channelText + "Distortion";
				break;
			}
			default: break;
			}
		}

	}

	json_t* dataToJson() override {

		saveState();

		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "edit_mode", json_integer((int)settings_.edit_mode));
		json_object_set_new(rootJ, "fcn_channel_1", json_integer((int)settings_.function[0]));
		json_object_set_new(rootJ, "fcn_channel_2", json_integer((int)settings_.function[1]));

		json_t* potValuesJ = json_array();
		for (int p : pot_value_) {
			json_t* pJ = json_integer(p);
			json_array_append_new(potValuesJ, pJ);
		}
		json_object_set_new(rootJ, "pot_values", potValuesJ);

		json_object_set_new(rootJ, "snap_mode", json_boolean(settings_.snap_mode));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* editModeJ = json_object_get(rootJ, "edit_mode");
		if (editModeJ) {
			settings_.edit_mode = static_cast<EditMode>(json_integer_value(editModeJ));
		}

		json_t* fcnChannel1J = json_object_get(rootJ, "fcn_channel_1");
		if (fcnChannel1J) {
			settings_.function[0] = static_cast<Function>(json_integer_value(fcnChannel1J));
		}

		json_t* fcnChannel2J = json_object_get(rootJ, "fcn_channel_2");
		if (fcnChannel2J) {
			settings_.function[1] = static_cast<Function>(json_integer_value(fcnChannel2J));
		}

		json_t* snapModeJ = json_object_get(rootJ, "snap_mode");
		if (snapModeJ) {
			settings_.snap_mode = json_boolean_value(snapModeJ);
		}

		json_t* potValuesJ = json_object_get(rootJ, "pot_values");
		size_t potValueId;
		json_t* pJ;
		json_array_foreach(potValuesJ, potValueId, pJ) {
			if (potValueId < sizeof(pot_value_) / sizeof(pot_value_)[0]) {
				settings_.pot_value[potValueId] = json_integer_value(pJ);
			}
		}

		// Update module internal state from settings.
		init();
		saveState();
	}

	void process(const ProcessArgs& args) override {
		// only update knobs / lights every 16 samples
		if (cvDivider.process()) {
			pollSwitches();
			pollPots();
			updateOleds();
		}

		Function CurrentFunction = function();
		if (params[PARAM_MODE].getValue() != CurrentFunction) {
			CurrentFunction = static_cast<Function>(params[PARAM_MODE].getValue());
			setFunction(edit_mode_ - EDIT_MODE_FIRST, CurrentFunction);
			saveState();
		}

		if (outputBuffer.empty()) {

			while (render_block_ != io_block_) {
				process(&block_[render_block_], kBlockSize);
				render_block_ = (render_block_ + 1) % kNumBlocks;
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
		s.block = &block_[io_block_];
		s.frame_index = io_frame_;
		io_frame_ += size;
		if (io_frame_ >= kBlockSize) {
			io_frame_ -= kBlockSize;
			io_block_ = (io_block_ + 1) % kNumBlocks;
		}
		return s;
	}

	inline Function function() const {
		return edit_mode_ == EDIT_MODE_SECOND ? function_[1] : function_[0];
	}

	inline void set_led_brightness(int channel, int16_t value) {
		brightness[channel] = value;
	}

	inline void process(Block* block, size_t size) {
		for (size_t i = 0; i < kNumChannels; ++i) {
			processors[i].Process(block->input[i], output, size);
			set_led_brightness(i, output[0]);
			for (size_t j = 0; j < size; ++j) {
				// From calibration_data.h, shifting signed to unsigned values.
				int32_t shifted_value = 32767 + static_cast<int32_t>(output[j]);
				CONSTRAIN(shifted_value, 0, 65535);
				block->output[i][j] = static_cast<uint16_t>(shifted_value);
			}
		}
	}

	void changeControlMode();
	void setFunction(uint8_t index, Function f);
	void onPotChanged(uint16_t id, uint16_t value);
	void onSwitchReleased(uint16_t id);
	void saveState();
	void lockPots();
	void pollSwitches();
	void pollPots();
	void refreshLeds();

	long long getSystemTimeMs();
};

const peaks::ProcessorFunction Apices::function_table_[FUNCTION_LAST][2] = {
	{ peaks::PROCESSOR_FUNCTION_ENVELOPE, peaks::PROCESSOR_FUNCTION_ENVELOPE },
	{ peaks::PROCESSOR_FUNCTION_LFO, peaks::PROCESSOR_FUNCTION_LFO },
	{ peaks::PROCESSOR_FUNCTION_TAP_LFO, peaks::PROCESSOR_FUNCTION_TAP_LFO },
	{ peaks::PROCESSOR_FUNCTION_BASS_DRUM, peaks::PROCESSOR_FUNCTION_SNARE_DRUM },

	{ peaks::PROCESSOR_FUNCTION_MINI_SEQUENCER, peaks::PROCESSOR_FUNCTION_MINI_SEQUENCER },
	{ peaks::PROCESSOR_FUNCTION_PULSE_SHAPER, peaks::PROCESSOR_FUNCTION_PULSE_SHAPER },
	{ peaks::PROCESSOR_FUNCTION_PULSE_RANDOMIZER, peaks::PROCESSOR_FUNCTION_PULSE_RANDOMIZER },
	{ peaks::PROCESSOR_FUNCTION_FM_DRUM, peaks::PROCESSOR_FUNCTION_FM_DRUM },
	{ peaks::PROCESSOR_FUNCTION_NUMBER_STATION, peaks::PROCESSOR_FUNCTION_NUMBER_STATION},
};


void Apices::changeControlMode() {
	uint16_t parameters[4];
	for (int i = 0; i < 4; ++i) {
		parameters[i] = adc_value_[i];
	}

	if (edit_mode_ == EDIT_MODE_SPLIT) {
		processors[0].CopyParameters(&parameters[0], 2);
		processors[1].CopyParameters(&parameters[2], 2);
		processors[0].set_control_mode(peaks::CONTROL_MODE_HALF);
		processors[1].set_control_mode(peaks::CONTROL_MODE_HALF);
	}
	else if (edit_mode_ == EDIT_MODE_TWIN) {
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

void Apices::setFunction(uint8_t index, Function f) {
	if (edit_mode_ == EDIT_MODE_SPLIT || edit_mode_ == EDIT_MODE_TWIN) {
		function_[0] = function_[1] = f;
		processors[0].set_function(function_table_[f][0]);
		processors[1].set_function(function_table_[f][1]);
	}
	else {
		function_[index] = f;
		processors[index].set_function(function_table_[f][index]);
	}
}

void Apices::onSwitchReleased(uint16_t id) {
	switch (id) {
	case SWITCH_TWIN_MODE: {
		if (edit_mode_ <= EDIT_MODE_SPLIT) {
			edit_mode_ = static_cast<EditMode>(EDIT_MODE_SPLIT - edit_mode_);
		}
		changeControlMode();
		saveState();
		break;
	}

	case SWITCH_EXPERT: {
		edit_mode_ = static_cast<EditMode>(
			(edit_mode_ + EDIT_MODE_FIRST) % EDIT_MODE_LAST);
		function_[0] = function_[1];
		processors[0].set_function(function_table_[function_[0]][0]);
		processors[1].set_function(function_table_[function_[0]][1]);
		lockPots();
		changeControlMode();
		saveState();
		break;
	}

	case SWITCH_CHANNEL_SELECT: {
		if (edit_mode_ >= EDIT_MODE_FIRST) {
			edit_mode_ = static_cast<EditMode>(EDIT_MODE_SECOND - (edit_mode_ & 1));

			switch (edit_mode_)
			{
			case EDIT_MODE_FIRST:
				params[PARAM_MODE].setValue(function_[0]);
				break;
			case EDIT_MODE_SECOND:
				params[PARAM_MODE].setValue(function_[1]);
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
		&adc_threshold_[0],
		&adc_threshold_[kNumAdcChannels],
		kAdcThresholdLocked);
	std::fill(&snapped_[0], &snapped_[kNumAdcChannels], false);
}

void Apices::pollPots() {
	for (uint8_t i = 0; i < kNumAdcChannels; ++i) {
		adc_lp_[i] = (int32_t(params[PARAM_KNOB_1 + i].getValue()) + adc_lp_[i] * 7) >> 3;
		int32_t value = adc_lp_[i];
		int32_t current_value = adc_value_[i];
		if (value >= current_value + adc_threshold_[i] ||
			value <= current_value - adc_threshold_[i] ||
			!adc_threshold_[i]) {
			onPotChanged(i, value);
			adc_value_[i] = value;
			adc_threshold_[i] = kAdcThresholdUnlocked;
		}
	}
}

void Apices::onPotChanged(uint16_t id, uint16_t value) {
	switch (edit_mode_) {
	case EDIT_MODE_TWIN:
		processors[0].set_parameter(id, value);
		processors[1].set_parameter(id, value);
		pot_value_[id] = value >> 8;
		break;

	case EDIT_MODE_SPLIT:
		if (id < 2) {
			processors[0].set_parameter(id, value);
		}
		else {
			processors[1].set_parameter(id - 2, value);
		}
		pot_value_[id] = value >> 8;
		break;

	case EDIT_MODE_FIRST:
	case EDIT_MODE_SECOND: {
		uint8_t index = id + (edit_mode_ - EDIT_MODE_FIRST) * 4;
		peaks::Processors* p = &processors[edit_mode_ - EDIT_MODE_FIRST];

		int16_t delta = static_cast<int16_t>(pot_value_[index]) - \
			static_cast<int16_t>(value >> 8);
		if (delta < 0) {
			delta = -delta;
		}

		if (!snap_mode_ || snapped_[id] || delta <= 2) {
			p->set_parameter(id, value);
			pot_value_[index] = value >> 8;
			snapped_[id] = true;
		}
	}
						 break;

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
		if (switches_[i].process(params[PARAM_EDIT_MODE + i].getValue())) {
			onSwitchReleased(SWITCH_TWIN_MODE + i);
		}
	}
	refreshLeds();
}

void Apices::saveState() {
	settings_.edit_mode = edit_mode_;
	settings_.function[0] = function_[0];
	settings_.function[1] = function_[1];
	std::copy(&pot_value_[0], &pot_value_[8], &settings_.pot_value[0]);
	settings_.snap_mode = snap_mode_;
	function1 = settings_.function[0];
	function2 = settings_.function[1];
}

void Apices::refreshLeds() {

	// refreshLeds() is only updated every N samples, so make sure setBrightnessSmooth methods account for this
	const float sampleTime = APP->engine->getSampleTime() * cvUpdateFrequency;

	uint8_t flash = (getSystemTimeMs() >> 7) & 7;
	switch (edit_mode_) {
	case EDIT_MODE_FIRST:
		lights[LIGHT_CHANNEL1].setBrightnessSmooth((flash == 1) ? 1.0f : 0.0f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
		for (int i = 0; i < 4; i++) {
			lights[(LIGHT_KNOBS_MODE + i * 3) + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 1].setBrightnessSmooth(5.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 2].setBrightnessSmooth(0.f, sampleTime);
		}
		break;
	case EDIT_MODE_SECOND:
		lights[LIGHT_CHANNEL1].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth((flash == 1 || flash == 3) ? 1.0f : 0.0f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(1.f, sampleTime);
		for (int i = 0; i < 4; i++) {
			lights[(LIGHT_KNOBS_MODE + i * 3) + 0].setBrightnessSmooth(5.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 1].setBrightnessSmooth(5.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 2].setBrightnessSmooth(0.f, sampleTime);
		}
		break;
	case EDIT_MODE_TWIN: {
		lights[LIGHT_CHANNEL1].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
		for (int i = 0; i < 4; i++) {
			lights[(LIGHT_KNOBS_MODE + i * 3) + 0].setBrightnessSmooth(5.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 2].setBrightnessSmooth(5.f, sampleTime);
		}
		break;
	}
	case EDIT_MODE_SPLIT: {
		lights[LIGHT_CHANNEL1].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL2].setBrightnessSmooth(1.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 0].setBrightnessSmooth(0.f, sampleTime);
		lights[LIGHT_CHANNEL_SELECT + 1].setBrightnessSmooth(0.f, sampleTime);
		for (int i = 0; i < 2; i++) {
			lights[(LIGHT_KNOBS_MODE + i * 3) + 0].setBrightnessSmooth(5.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 2].setBrightnessSmooth(0.f, sampleTime);
		}
		for (int i = 2; i < 4; i++) {
			lights[(LIGHT_KNOBS_MODE + i * 3) + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 1].setBrightnessSmooth(0.f, sampleTime);
			lights[(LIGHT_KNOBS_MODE + i * 3) + 2].setBrightnessSmooth(5.f, sampleTime);
		}
		break;
	}
	}

	lights[LIGHT_SPLIT_MODE].setBrightnessSmooth((edit_mode_ == EDIT_MODE_SPLIT) ? 1.0f : 0.0f, sampleTime);
	lights[LIGHT_EXPERT_MODE].setBrightnessSmooth((edit_mode_ & 2) ? 1.0F : 0.F, sampleTime);

	if ((getSystemTimeMs() & 256) && function() >= FUNCTION_FIRST_ALTERNATE_FUNCTION) {
		for (size_t i = 0; i < 4; ++i) {
			lights[LIGHT_FUNCTION_1 + i].setBrightnessSmooth(0.0f, sampleTime);
		}
	}
	else {
		for (size_t i = 0; i < 4; ++i) {
			lights[LIGHT_FUNCTION_1 + i].setBrightnessSmooth(((function() & 3) == i) ? 1.0f : 0.0f, sampleTime);
		}
	}

	uint8_t b[2];
	for (uint8_t i = 0; i < 2; ++i) {
		switch (function_[i]) {
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
		}
									break;
		default:
			b[i] = brightness[i] >> 7;
			break;
		}
	}

	if (processors[0].function() == peaks::PROCESSOR_FUNCTION_NUMBER_STATION) {
		uint8_t pattern = processors[0].number_station().digit()
			^ processors[1].number_station().digit();
		for (size_t i = 0; i < 4; ++i) {
			lights[LIGHT_FUNCTION_1 + i].value = (pattern & 1) ? 1.0f : 0.0f;
			pattern = pattern >> 1;
		}
		b[0] = processors[0].number_station().gate() ? 255 : 0;
		b[1] = processors[1].number_station().gate() ? 255 : 0;
	}

	const float deltaTime = APP->engine->getSampleTime();
	lights[LIGHT_TRIGGER_1].setSmoothBrightness(rescale(static_cast<float>(b[0]), 0.0f, 255.0f, 0.0f, 1.0f), deltaTime);
	lights[LIGHT_TRIGGER_2].setSmoothBrightness(rescale(static_cast<float>(b[1]), 0.0f, 255.0f, 0.0f, 1.0f), deltaTime);
}

static std::vector<std::string> modeListChan1{
	"1:ENVELOPE",
	"1:LFO",
	"1:TAP LFO",
	"1:DRUM GEN",
	"1:SEQUENCER*",
	"1:PLS. SHAP*",
	"1:PLS. RAND*",
	"1:DRUM FM*",
	"1:RADIO T&",
};

static std::vector<std::string> modeListChan2{
	"2:ENVELOPE",
	"2:LFO",
	"2:TAP LFO",
	"2:DRUM GEN",
	"2:SEQUENCER*",
	"2:PLS. SHAP*",
	"2:PLS. RAND*",
	"2:DRUM FM*",
	"1:RADIO T&",
};

struct ApicesWidget : ModuleWidget {
	ApicesWidget(Apices* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/apices_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SanguineMatrixDisplay* displayChannel1 = new SanguineMatrixDisplay();
		displayChannel1->box.pos = mm2px(Vec(8.937, 22.885));
		displayChannel1->module = module;
		addChild(displayChannel1);

		SanguineMatrixDisplay* displayChannel2 = new SanguineMatrixDisplay();
		displayChannel2->box.pos = mm2px(Vec(8.937, 35.477));
		displayChannel2->module = module;
		addChild(displayChannel2);

		if (module) {
			displayChannel1->itemList = &modeListChan1;
			displayChannel1->selectedItem = &module->function1;
			displayChannel2->itemList = &modeListChan2;
			displayChannel2->selectedItem = &module->function2;
		}

		addParam(createParamCentered<Rogan2SGray>(mm2px(Vec(91.09, 34.261)), module, Apices::PARAM_MODE));

		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<RedLight>>>(mm2px(Vec(51.339, 80.532)),
			module, Apices::PARAM_EDIT_MODE, Apices::LIGHT_SPLIT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<BlueLight>>>(mm2px(Vec(17.472, 54.112)),
			module, Apices::PARAM_EXPERT_MODE, Apices::LIGHT_EXPERT_MODE));
		addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<GreenRedLight>>>(mm2px(Vec(4.933, 54.112)),
			module, Apices::PARAM_CHANNEL_SELECT, Apices::LIGHT_CHANNEL_SELECT));

		addParam(createParamCentered<LEDBezel>(mm2px(Vec(11.595, 70.855)), module, Apices::PARAM_TRIGGER_1));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(11.595, 117.9)), module, Apices::PARAM_TRIGGER_2));

		addChild(createLightCentered<LEDBezelLight<RedLight>>(mm2px(Vec(11.595, 70.855)), module, Apices::LIGHT_TRIGGER_1));
		addChild(createLightCentered<LEDBezelLight<BlueLight>>(mm2px(Vec(11.595, 117.9)), module, Apices::LIGHT_TRIGGER_2));

		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(83.215, 25.986)), module, Apices::LIGHT_FUNCTION_1));
		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(98.965, 25.986)), module, Apices::LIGHT_FUNCTION_2));
		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(83.215, 42.136)), module, Apices::LIGHT_FUNCTION_3));
		addChild(createLightCentered<SmallLight<OrangeLight>>(mm2px(Vec(98.965, 42.136)), module, Apices::LIGHT_FUNCTION_4));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(5.633, 27.965)), module, Apices::LIGHT_CHANNEL1));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(5.633, 40.557)), module, Apices::LIGHT_CHANNEL2));

		addParam(createParamCentered<Rogan2PSRed>(mm2px(Vec(31.734, 63.829)), module, Apices::PARAM_KNOB_1));
		addParam(createParamCentered<Rogan2PSRed>(mm2px(Vec(70.966, 63.829)), module, Apices::PARAM_KNOB_2));
		addParam(createParamCentered<Rogan2PSBlue>(mm2px(Vec(31.734, 97.058)), module, Apices::PARAM_KNOB_3));
		addParam(createParamCentered<Rogan2PSBlue>(mm2px(Vec(70.966, 97.058)), module, Apices::PARAM_KNOB_4));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(43.457, 63.829)), module, Apices::LIGHT_KNOBS_MODE + 0 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(59.186, 63.829)), module, Apices::LIGHT_KNOBS_MODE + 1 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(43.457, 97.058)), module, Apices::LIGHT_KNOBS_MODE + 2 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(59.186, 97.058)), module, Apices::LIGHT_KNOBS_MODE + 3 * 3));

		addInput(createInputCentered<BananutGreen>((mm2px(Vec(11.595, 88.162))), module, Apices::GATE_1_INPUT));
		addInput(createInputCentered<BananutGreen>((mm2px(Vec(11.595, 100.593))), module, Apices::GATE_2_INPUT));

		addOutput(createOutputCentered<BananutRed>((mm2px(Vec(90.05, 100.846))), module, Apices::OUT_1_OUTPUT));
		addOutput(createOutputCentered<BananutRed>((mm2px(Vec(90.05, 116.989))), module, Apices::OUT_2_OUTPUT));

		Sanguine96x32OLEDDisplay* oledDisplay1 = new Sanguine96x32OLEDDisplay();
		oledDisplay1->box.pos = mm2px(Vec(23.585, 73.301));
		oledDisplay1->module = module;
		if (module)
			oledDisplay1->oledText = &module->oledText1;
		addChild(oledDisplay1);

		Sanguine96x32OLEDDisplay* oledDisplay2 = new Sanguine96x32OLEDDisplay();
		oledDisplay2->box.pos = mm2px(Vec(62.817, 73.301));
		oledDisplay2->module = module;
		if (module)
			oledDisplay2->oledText = &module->oledText2;
		addChild(oledDisplay2);

		Sanguine96x32OLEDDisplay* oledDisplay3 = new Sanguine96x32OLEDDisplay();
		oledDisplay3->box.pos = mm2px(Vec(23.585, 81.848));
		oledDisplay3->module = module;
		if (module)
			oledDisplay3->oledText = &module->oledText3;
		addChild(oledDisplay3);

		Sanguine96x32OLEDDisplay* oledDisplay4 = new Sanguine96x32OLEDDisplay();
		oledDisplay4->box.pos = mm2px(Vec(62.817, 81.848));
		oledDisplay4->module = module;
		if (module)
			oledDisplay4->oledText = &module->oledText4;
		addChild(oledDisplay4);

		SanguineShapedLight* mutantsLogo = new SanguineShapedLight();
		mutantsLogo->box.pos = mm2px(Vec(48.469, 114.607));
		mutantsLogo->box.size = Vec(36.06, 14.79);
		mutantsLogo->module = module;
		mutantsLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/mutants_glowy.svg")));
		addChild(mutantsLogo);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight();
		bloodLogo->box.pos = mm2px(Vec(39.678, 106.239));
		bloodLogo->box.size = Vec(11.2, 23.27);
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		addChild(bloodLogo);
	}

	void appendContextMenu(Menu* menu) override {

		menu->addChild(new MenuSeparator);
		Apices* peaks = dynamic_cast<Apices*>(this->module);

		menu->addChild(createBoolPtrMenuItem("Knob pickup (snap)", "", &peaks->snap_mode_));
	}

};

Model* modelApices = createModel<Apices, ApicesWidget>("Sanguine-Apices");