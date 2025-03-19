#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "vimina.hpp"

struct Vimina : SanguineModule {
	enum ParamIds {
		PARAM_FACTOR_1,
		PARAM_FACTOR_2,
		PARAM_MODE_1,
		PARAM_MODE_2,
		PARAM_RESET_1,
		PARAM_RESET_2,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_RESET,
		INPUT_CLOCK,
		INPUT_CV1,
		INPUT_CV2,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_OUT_1A,
		OUTPUT_OUT_2A,
		OUTPUT_OUT_1B,
		OUTPUT_OUT_2B,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHTS_STATE, 2 * 2),
		ENUMS(LIGHTS_MODE, 2 * 2),
		LIGHTS_COUNT
	};

	enum SectionFunctions {
		SECTION_FUNCTION_FACTORER,
		SECTION_FUNCTION_SWING
	};

	enum ChannelStates {
		CHANNEL_REST,
		CHANNEL_THRU,
		CHANNEL_GENERATED
	};

	static const int kTimingErrorCorrectionAmount = 12;

	static const int kLightsFrequency = 16;

	static const int kMaxModuleSections = 2;

	static const int kTimerCounterMax = 2147483647;

	// Channel ids
	static const int kResetChannel = 0;
	static const int kClockChannel = 1;

	static const int kTriggerExtendCount = 20;

	// Factorer constants
	/*
	The number 15 represents the set:
	 -8, -7, -6, -5, -4, -3, -2, 1, 2, 3, 4, 5, 6, 7, 8
	where negative numbers are multiplier factors; positive numbers are divider
	factors, and 1 is bypass.
	*/
	static const int kFactorCount = 15;
	/*
	The index of 1 in the set above.
	This is the factor setting where factorer is neither dividing nor multiplying.
	*/
	static const int kFactorerBypassIndex = 7;
	static const int kFactorerBypassValue = 1;

	// LED constants
	static const int kLedThruGateDuration = 256;
	static const int kLedGeneratedGateDuration = 128;
	static const int kPulseTrackerBufferSize = 2;

	int channelCount = 0;
	int ledsChannel = 0;
	int triggerCount[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	int32_t channelFactor[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	int32_t channelSwing[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	uint32_t pulseTrackerRecordedCount[PORT_MAX_CHANNELS];

	uint32_t ledGateDuration[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	ChannelStates ledState[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	ChannelStates channelState[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	uint8_t triggerExtendCount[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	int32_t divisionCounter[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	int32_t swingCounter[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	// Swing constants
	const float kSwingFactorMin = 50.f;
	const float kSwingFactorMax = 70.f; // Maximum swing amount can be set up to 99.

	// Scaling constant
	const float kMaxParamValue = 1.f;

	float channelVoltage[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	float pulseTrackerBuffer[kPulseTrackerBufferSize][PORT_MAX_CHANNELS] = {};

	bool gateInputState[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	bool multiplyDebouncing[kMaxModuleSections][PORT_MAX_CHANNELS];

	SectionFunctions channelFunction[kMaxModuleSections] = {
		SECTION_FUNCTION_SWING,
		SECTION_FUNCTION_FACTORER
	};

	dsp::BooleanTrigger btReset[kMaxModuleSections];
	dsp::ClockDivider lightsDivider;
	dsp::Timer tmrModuleClock[PORT_MAX_CHANNELS]; // Replaces the ATMega88pa's TCNT1

	Vimina() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		for (int section = 0; section < kMaxModuleSections; ++section) {
			configParam(PARAM_FACTOR_1 + section, 0.f, kMaxParamValue, 0.5f, string::f("Channel %d factor", section + 1));
			configSwitch(PARAM_RESET_1 + section, 0.f, 1.f, 0.f, string::f("Channel %d reset", section + 1));

			configInput(INPUT_CV1 + section, string::f("Channel %d factor", section + 1));
			configOutput(OUTPUT_OUT_1A + section, string::f("Channel %d A", section + 1));
			configOutput(OUTPUT_OUT_1B + section, string::f("Channel %d B", section + 1));
		}
		configInput(INPUT_RESET, "Reset");
		configInput(INPUT_CLOCK, "Clock");

		configSwitch(PARAM_MODE_1, 0.f, 1.f, 1.f, "Channel 1 mode", vimina::modeLabels);
		configSwitch(PARAM_MODE_2, 0.f, 1.f, 0.f, "Channel 2 mode", vimina::modeLabels);

		configBypass(INPUT_CLOCK, OUTPUT_OUT_1A);
		configBypass(INPUT_CLOCK, OUTPUT_OUT_1B);
		configBypass(INPUT_CLOCK, OUTPUT_OUT_2A);
		configBypass(INPUT_CLOCK, OUTPUT_OUT_2B);

		init();

		lightsDivider.setDivision(kLightsFrequency);
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(inputs[INPUT_CLOCK].getChannels(), 1);

		bool bClockConnected = inputs[INPUT_CLOCK].isConnected();

		bool bResetConnected = inputs[INPUT_RESET].isConnected();

		bool resetButtons[kMaxModuleSections] = {};

		for (int section = 0; section < kMaxModuleSections; ++section) {
			resetButtons[section] = btReset[section].process(params[PARAM_RESET_1 + section].getValue());
		}

		outputs[OUTPUT_OUT_1A].setChannels(channelCount);
		outputs[OUTPUT_OUT_1B].setChannels(channelCount);
		outputs[OUTPUT_OUT_2A].setChannels(channelCount);
		outputs[OUTPUT_OUT_2B].setChannels(channelCount);

		for (int channel = 0; channel < channelCount; ++channel) {
			tmrModuleClock[channel].process(args.sampleRate * args.sampleTime);

			bool bIsTrigger = false;

			if (bClockConnected) {
				if (isRisingEdge(kClockChannel, inputs[INPUT_CLOCK].getVoltage(channel) >= 2.f, channel)) {
					/* Pulse tracker is always recording. this should help smooth transitions
					   between functions even though divide doesn't use it. */
					   // Shift
					pulseTrackerBuffer[kPulseTrackerBufferSize - 2][channel] = pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel];
					pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel] = tmrModuleClock[channel].time;
					if (pulseTrackerRecordedCount[channel] < kPulseTrackerBufferSize) {
						pulseTrackerRecordedCount[channel] += 1;
					}
					bIsTrigger = true;
				}
			}

			bool bIsReset = false;
			if (bResetConnected) {
				bIsReset = isRisingEdge(kResetChannel, inputs[INPUT_RESET].getVoltage(channel) >= 2.f, channel);
			}

			for (uint8_t section = 0; section < kMaxModuleSections; ++section) {
				channelFunction[section] = SectionFunctions(params[PARAM_MODE_1 + section].getValue());

				if (resetButtons[section]) {
					handleReset(section, channel);
				}

				if (bClockConnected) {
					setupChannel(section, channel);

					handleTriggers(section, bIsTrigger, bIsReset, channel);

					transformClock(section, channel);

					setOutputVoltages(section, channel);
				}
				channelState[section][channel] = CHANNEL_REST; // Clean up.
			}

			if (tmrModuleClock[channel].time >= kTimerCounterMax) {
				tmrModuleClock[channel].reset();
			}
		}
		if (lightsDivider.process()) {
			const float sampleTime = kLightsFrequency * args.sampleTime;

			if (ledsChannel >= channelCount) {
				ledsChannel = channelCount - 1;
			}

			for (int section = 0; section < kMaxModuleSections; ++section) {
				int currentLight = LIGHTS_MODE + section * 2;
				lights[currentLight + 0].setBrightnessSmooth(channelFunction[section] == SECTION_FUNCTION_FACTORER ?
					kSanguineButtonLightValue : 0.f, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(!channelFunction[section] == SECTION_FUNCTION_FACTORER ?
					kSanguineButtonLightValue : 0.f, sampleTime);
			}
		}

		updateChannelLeds(0, args.sampleTime, ledsChannel);
		updateChannelLeds(1, args.sampleTime, ledsChannel);
	}

	uint32_t getPulseTrackerElapsed(const int channel) {
		return (tmrModuleClock[channel].time >= pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel]) ?
			tmrModuleClock[channel].time - pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel] :
			tmrModuleClock[channel].time + (kTimerCounterMax - pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel]);
	}

	uint32_t getPulseTrackerPeriod(const int channel) {
		return (pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel] >= pulseTrackerBuffer[kPulseTrackerBufferSize - 2][channel]) ?
			pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel] - pulseTrackerBuffer[kPulseTrackerBufferSize - 2][channel] :
			pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel] + (kTimerCounterMax - pulseTrackerBuffer[kPulseTrackerBufferSize - 2][channel]);
	}

	void handleReset(const uint8_t section, const int channel) {
		switch (channelFunction[section]) {
		case SECTION_FUNCTION_FACTORER:
			resetDivision(section, channel);
			break;
		case SECTION_FUNCTION_SWING:
			resetSwing(section, channel);
			break;
		}
	}

	void handleTriggers(const uint8_t section, const bool isTrigger, const bool isReset, const int channel) {
		if (isTrigger) {
			switch (channelFunction[section]) {
			case SECTION_FUNCTION_FACTORER:
				if (isDivideEnabled(section, channel)) {
					if (divisionCounter[section][channel] <= 0) {
						channelState[section][channel] = CHANNEL_GENERATED; // divide converts thru to exec on every division
					}
					// Deal with counter.
					if (divisionCounter[section][channel] >= channelFactor[section][channel] - 1) {
						resetDivision(section, channel);
					} else {
						++divisionCounter[section][channel];
					}
				} else {
					// Mult always acknowledges thru.
					channelState[section][channel] = CHANNEL_THRU;
					triggerCount[section][channel] = 0;
				}
				break;
			case SECTION_FUNCTION_SWING:
				switch (swingCounter[section][channel]) {
				case 0: // thru beat
					channelState[section][channel] = CHANNEL_THRU;
					swingCounter[section][channel] = 1;
					break;
				case 1:
					// Skipped thru beat.
					// Unless lowest setting, no swing - should do thru.
					if (channelSwing[section][channel] <= kSwingFactorMin) {
						channelState[section][channel] = CHANNEL_GENERATED;
						resetSwing(section, channel);
					} else {
						// Rest.
						channelState[section][channel] = CHANNEL_REST;
						swingCounter[section][channel] = 2;
					}
					break;
				default:
					resetSwing(section, channel); // Something is wrong if we're here, so reset.
					break;
				}
				break;
			}
		}
		if (isReset) {
			handleReset(section, channel);
		}
	}

	void init() {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			pulseTrackerBuffer[kPulseTrackerBufferSize - 2][channel] = 0;
			pulseTrackerBuffer[kPulseTrackerBufferSize - 1][channel] = 0;
			pulseTrackerRecordedCount[channel] = 0;

			for (uint8_t i = 0; i < kMaxModuleSections; ++i) {
				triggerExtendCount[i][channel] = 0;
			}
			tmrModuleClock[channel].reset();
		}
	}

	bool isDivideEnabled(const uint8_t section, const int channel) {
		return channelFactor[section][channel] > kFactorerBypassValue;
	}

	bool isMultiplyEnabled(const uint8_t section, const int channel) {
		return channelFactor[section][channel] < kFactorerBypassValue;
	}

	bool isMultiplyStrikeTurn(const uint8_t section, const uint32_t elapsed, const int channel) {
		float interval = getPulseTrackerPeriod(channel) / -channelFactor[section][channel];
		if (fmod(elapsed, interval) <= kTimingErrorCorrectionAmount) {
			if (!multiplyDebouncing[section][channel]) {
				return true;
			}
		} else {
			multiplyDebouncing[section][channel] = false;
		}
		return false;
	}

	bool isPulseTrackerPeriod(const int channel) {
		return pulseTrackerRecordedCount[channel] >= kPulseTrackerBufferSize;
	}

	bool isRisingEdge(const uint8_t section, const bool voltageAboveThreshold, const int channel) {
		bool lastGateState = gateInputState[section][channel];
		gateInputState[section][channel] = voltageAboveThreshold;
		return gateInputState[section][channel] && !lastGateState;
	}

	bool isSwingStrikeTurn(const uint8_t section, const uint32_t elapsed, const int channel) {
		if (swingCounter[section][channel] >= 2 && channelSwing[section][channel] > kSwingFactorMin) {
			uint32_t period = getPulseTrackerPeriod(channel);
			uint32_t interval = ((10 * (period * 2)) / (1000 / channelSwing[section][channel])) - period;
			return (elapsed >= interval && elapsed <= interval + kTimingErrorCorrectionAmount);
		} else {
			// thru
			return false;
		}
	}

	void resetDivision(const uint8_t section, const int channel) {
		divisionCounter[section][channel] = 0;
	}

	void resetSwing(const uint8_t section, const int channel) {
		swingCounter[section][channel] = 0;
	}

	void setOutputVoltages(const uint8_t section, const int channel) {
		if (channelState[section][channel] > CHANNEL_REST) {
			outputs[OUTPUT_OUT_1A + section].setVoltage(10.f, channel);
			outputs[OUTPUT_OUT_1B + section].setVoltage(10.f, channel);
			triggerExtendCount[section][channel] = kTriggerExtendCount;

			switch (channelState[section][channel])
			{
			case CHANNEL_GENERATED:
				ledGateDuration[section][channel] = kLedGeneratedGateDuration;
				ledState[section][channel] = CHANNEL_GENERATED;
				break;
			case CHANNEL_THRU:
				ledGateDuration[section][channel] = kLedThruGateDuration;
				ledState[section][channel] = CHANNEL_THRU;
				break;
			default:
				break;
			}
		} else {
			if (triggerExtendCount[section][channel] == 0) {
				outputs[OUTPUT_OUT_1A + section].setVoltage(0.f, channel);
				outputs[OUTPUT_OUT_1B + section].setVoltage(0.f, channel);
			} else {
				--triggerExtendCount[section][channel];
			}
		}
	}

	void setupChannel(const uint8_t section, const int channel) {
		channelVoltage[section][channel] = clamp(params[PARAM_FACTOR_1 + section].getValue() +
			(inputs[INPUT_CV1 + section].getVoltage(channel) / 10.f), 0.f, 1.f);

		switch (channelFunction[section])
		{
		case SECTION_FUNCTION_FACTORER:
			int16_t factorIndex;
			factorIndex = std::round(channelVoltage[section][channel] /
				(kMaxParamValue / (kFactorCount - 1.f)) - kFactorerBypassIndex);
			// Offset result so that there are no -1 or 0 factors, but values are still evenly spaced.
			if (factorIndex == 0) {
				channelFactor[section][channel] = kFactorerBypassValue;
			} else if (factorIndex < 0) {
				channelFactor[section][channel] = --factorIndex; // abs
			} else {
				channelFactor[section][channel] = ++factorIndex;
			}
			break;
		case SECTION_FUNCTION_SWING:
			channelSwing[section][channel] = channelVoltage[section][channel] /
				(kMaxParamValue / (kSwingFactorMax - kSwingFactorMin)) + kSwingFactorMin;
			break;
		}
	}

	void transformClock(const uint8_t section, const int channel) {
		switch (channelFunction[section]) {
		case SECTION_FUNCTION_FACTORER:
			if (isMultiplyEnabled(section, channel) && isPulseTrackerPeriod(channel) &&
				isMultiplyStrikeTurn(section, getPulseTrackerElapsed(channel), channel) &&
				triggerCount[section][channel] >= channelFactor[section][channel]) {
				channelState[section][channel] = CHANNEL_GENERATED;
				multiplyDebouncing[section][channel] = true;
				--triggerCount[section][channel];
			}
			break;
		case SECTION_FUNCTION_SWING:
			if (isSwingStrikeTurn(section, getPulseTrackerElapsed(channel), channel)) {
				channelState[section][channel] = CHANNEL_GENERATED;
				resetSwing(section, channel);
			}
			break;
		}
	}

	void updateChannelLeds(const uint8_t section, const float sampleTime, const int channel) {
		if (ledGateDuration[section][channel]) {
			--ledGateDuration[section][channel];
			if (!ledGateDuration[section][channel]) {
				ledState[section][channel] = CHANNEL_REST;
			}
		}

		int currentLight = LIGHTS_STATE + section * 2;
		switch (ledState[section][channel]) {
		case CHANNEL_REST:
			lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
			break;
		case CHANNEL_THRU:
			lights[currentLight + 0].setBrightnessSmooth(1.f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
			break;
		case CHANNEL_GENERATED:
			lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(1.f, sampleTime);
			break;
		}
	}

	void onReset() override {
		params[PARAM_MODE_1].setValue(1);
		params[PARAM_MODE_2].setValue(0);
		params[PARAM_FACTOR_1].setValue(0.5f);
		params[PARAM_FACTOR_2].setValue(0.5f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* ledsChannelJ = json_integer(ledsChannel);
		json_object_set_new(rootJ, "ledsChannel", ledsChannelJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* ledsChannelJ = json_object_get(rootJ, "ledsChannel");
		if (ledsChannelJ) {
			ledsChannel = json_integer_value(ledsChannelJ);
		}
	}
};

struct ViminaWidget : SanguineModuleWidget {
	explicit ViminaWidget(Vimina* module) {
		setModule(module);

		moduleName = "vimina";
		panelSize = SIZE_6;
		backplateColor = PLATE_RED;

		makePanel();

		addScrews(SCREW_TOP_LEFT | SCREW_BOTTOM_LEFT);

		// Channel 1
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(4.622, 16.723), module,
			Vimina::PARAM_MODE_1, Vimina::LIGHTS_MODE + 0 * 2));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(25.863, 16.723),
			module, Vimina::PARAM_RESET_1));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(15.24, 27.904), module, Vimina::PARAM_FACTOR_1));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.012, 44.303), module, Vimina::INPUT_RESET));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(24.481, 44.303), module, Vimina::INPUT_CV1));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(6.012, 59.959), module, Vimina::OUTPUT_OUT_1A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 59.959), module, Vimina::LIGHTS_STATE + 0 * 2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(24.481, 59.959), module, Vimina::OUTPUT_OUT_1B));

		// Channel 2
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(4.622, 74.653), module,
			Vimina::PARAM_MODE_2, Vimina::LIGHTS_MODE + 1 * 2));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(25.863, 74.653),
			module, Vimina::PARAM_RESET_2));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(15.24, 85.833), module, Vimina::PARAM_FACTOR_2));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(6.012, 102.232), module, Vimina::INPUT_CLOCK));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(24.481, 102.232), module, Vimina::INPUT_CV2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(6.012, 117.888), module, Vimina::OUTPUT_OUT_2A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 117.888), module, Vimina::LIGHTS_STATE + 1 * 2));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(24.481, 117.888), module, Vimina::OUTPUT_OUT_2B));
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Vimina* module = dynamic_cast<Vimina*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> availableChannels;
		for (int i = 0; i < module->channelCount; ++i) {
			availableChannels.push_back(channelNumbers[i]);
		}
		menu->addChild(createIndexSubmenuItem("LEDs channel", availableChannels,
			[=]() {return module->ledsChannel; },
			[=](int i) {module->ledsChannel = i; }
		));
	}
};

Model* modelVimina = createModel<Vimina, ViminaWidget>("Sanguine-Vimina");