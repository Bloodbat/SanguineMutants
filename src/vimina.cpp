#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

static const std::vector<std::string> viminaModeLabels = {
	"Clock multiplier/divider",
	"Clock swing"
};

struct Vimina : SanguineModule {
	enum ParamIds {
		PARAM_FACTOR1,
		PARAM_FACTOR2,
		PARAM_MODE1,
		PARAM_MODE2,
		PARAM_RESET1,
		PARAM_RESET2,
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
		OUTPUT_OUT1A,
		OUTPUT_OUT2A,
		OUTPUT_OUT1B,
		OUTPUT_OUT2B,
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

	static const int kTimerCounterMax = 65535;

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

	int16_t channelFactor[kMaxModuleSections] = {};
	int16_t channelSwing[kMaxModuleSections] = {};

	uint16_t pulseTrackerRecordedCount = 0;

	uint16_t ledGateDuration[kMaxModuleSections] = {};
	ChannelStates ledState[kMaxModuleSections] = {};

	ChannelStates channelState[kMaxModuleSections] = {};
	uint8_t triggerExtendCount[kMaxModuleSections] = {};

	int8_t divisionCounter[kMaxModuleSections] = {};
	int8_t swingCounter[kMaxModuleSections] = {};

	// Swing constants
	const float kSwingFactorMin = 50.f;
	const float kSwingFactorMax = 70.f; // Maximum swing amount can be set up to 99.

	// Scaling constant
	const float kMaxParamValue = 1.f;

	const float kClockSpeed = 8000.f;

	float channelVoltage[kMaxModuleSections] = {};
	float pulseTrackerBuffer[kPulseTrackerBufferSize] = {};


	bool gateInputState[kMaxModuleSections] = {};
	bool multiplyDebouncing[kMaxModuleSections];

	SectionFunctions channelFunction[kMaxModuleSections] = {
		SECTION_FUNCTION_SWING,
		SECTION_FUNCTION_FACTORER
	};

	dsp::BooleanTrigger btReset[kMaxModuleSections];
	dsp::ClockDivider lightsDivider;
	dsp::Timer moduleClock; // Replaces the ATMega88pa's TCNT1

	Vimina() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		for (int i = 0; i < 2; i++) {
			configParam(PARAM_FACTOR1 + i, 0.f, kMaxParamValue, 0.5f, string::f("Channel %d factor", i + 1));
			configSwitch(PARAM_RESET1 + i, 0.f, 1.f, 0.f, string::f("Channel %d reset", i + 1));

			configInput(INPUT_CV1 + i, string::f("Channel %d factor", i + 1));
			configOutput(OUTPUT_OUT1A + i, string::f("Channel %d A", i + 1));
			configOutput(OUTPUT_OUT1B + i, string::f("Channel %d B", i + 1));
		}
		configInput(INPUT_RESET, "Reset");
		configInput(INPUT_CLOCK, "Clock");

		configSwitch(PARAM_MODE1, 0.f, 1.f, 1.f, "Channel 1 mode", viminaModeLabels);
		configSwitch(PARAM_MODE2, 0.f, 1.f, 0.f, "Channel 2 mode", viminaModeLabels);

		configBypass(INPUT_CLOCK, OUTPUT_OUT1A);
		configBypass(INPUT_CLOCK, OUTPUT_OUT1B);
		configBypass(INPUT_CLOCK, OUTPUT_OUT2A);
		configBypass(INPUT_CLOCK, OUTPUT_OUT2B);

		init();

		lightsDivider.setDivision(kLightsFrequency);
	}

	void process(const ProcessArgs& args) override {
		// TODO POLYPHONY!: Channel count should be set by clock!

		moduleClock.process(kClockSpeed * args.sampleTime);

		bool bClockConnected = inputs[INPUT_CLOCK].isConnected();

		bool bResetConnected = inputs[INPUT_RESET].isConnected();

		bool bIsLightsTurn = lightsDivider.process();

		bool bIsTrigger = false;

		if (bClockConnected) {
			if (checkRisingEdge(kClockChannel, inputs[INPUT_CLOCK].getVoltage() >= 2.f)) {
				// Pulse tracker is always recording. this should help smooth transitions
				// between functions even though divide doesn't use it.
				// Shift
				pulseTrackerBuffer[kPulseTrackerBufferSize - 2] = pulseTrackerBuffer[kPulseTrackerBufferSize - 1];
				pulseTrackerBuffer[kPulseTrackerBufferSize - 1] = moduleClock.time;
				if (pulseTrackerRecordedCount < kPulseTrackerBufferSize) {
					pulseTrackerRecordedCount += 1;
				}
				bIsTrigger = true;
			}
		}

		bool bIsReset = false;
		if (bResetConnected) {
			bIsReset = checkRisingEdge(kResetChannel, inputs[INPUT_RESET].getVoltage() >= 2.f);
		}

		for (uint8_t section = 0; section < kMaxModuleSections; section++) {
			channelFunction[section] = SectionFunctions(params[PARAM_MODE1 + section].getValue());

			if (btReset[section].process(params[PARAM_RESET1 + section].getValue())) {
				handleReset(section);
			}

			if (bClockConnected) {
				setupChannel(section);

				handleTriggers(section, bIsTrigger, bIsReset);

				transformClock(section);

				setOutputVoltages(section);
			}
			channelState[section] = CHANNEL_REST; // Clean up.

			if (bIsLightsTurn) {
				const float sampleTime = kLightsFrequency * args.sampleTime;

				updateChannelLeds(section, sampleTime);

				int currentLight = LIGHTS_MODE + section * 2;
				bool bIsGreenLight = channelFunction[section] == SECTION_FUNCTION_FACTORER;
				lights[currentLight + 0].setBrightnessSmooth(bIsGreenLight, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(!bIsGreenLight, sampleTime);
			}
		}
		if (moduleClock.time >= kTimerCounterMax) {
			moduleClock.reset();
		}
	}

	bool checkRisingEdge(const uint8_t channel, const bool voltageAboveThreshold) {
		bool lastGateState = gateInputState[channel];
		gateInputState[channel] = voltageAboveThreshold;
		return gateInputState[channel] && !lastGateState;
	}

	uint16_t getPulseTrackerElapsed() {
		return (moduleClock.time >= pulseTrackerBuffer[kPulseTrackerBufferSize - 1]) ?
			moduleClock.time - pulseTrackerBuffer[kPulseTrackerBufferSize - 1] :
			moduleClock.time + (kTimerCounterMax - pulseTrackerBuffer[kPulseTrackerBufferSize - 1]);
	}

	uint16_t getPulseTrackerPeriod() {
		return (pulseTrackerBuffer[kPulseTrackerBufferSize - 1] >= pulseTrackerBuffer[kPulseTrackerBufferSize - 2]) ?
			pulseTrackerBuffer[kPulseTrackerBufferSize - 1] - pulseTrackerBuffer[kPulseTrackerBufferSize - 2] :
			pulseTrackerBuffer[kPulseTrackerBufferSize - 1] + (kTimerCounterMax - pulseTrackerBuffer[kPulseTrackerBufferSize - 2]);
	}

	void handleReset(const uint8_t channel) {
		switch (channelFunction[channel]) {
		case SECTION_FUNCTION_FACTORER:
			resetDivision(channel);
			break;
		case SECTION_FUNCTION_SWING:
			resetSwing(channel);
			break;
		}
	}

	void setupChannel(const uint8_t section) {
		channelVoltage[section] = clamp(params[PARAM_FACTOR1 + section].getValue() +
			(inputs[INPUT_CV1 + section].getVoltage() / 10.f), 0.f, 1.f);

		switch (channelFunction[section])
		{
		case SECTION_FUNCTION_FACTORER:
			int16_t factorIndex;
			factorIndex = (channelVoltage[section] / (kMaxParamValue / (kFactorCount - 1))) - kFactorerBypassIndex;
			// Offset result so that there are no -1 or 0 factors, but values are still evenly spaced.
			if (factorIndex == 0) {
				channelFactor[section] = kFactorerBypassValue;
			}
			else if (factorIndex < 0) {
				channelFactor[section] = --factorIndex; // abs
			}
			else {
				channelFactor[section] = ++factorIndex;
			}
			break;
		case SECTION_FUNCTION_SWING:
			channelSwing[section] = channelVoltage[section] / (kMaxParamValue / (kSwingFactorMax - kSwingFactorMin)) + kSwingFactorMin;
			break;
		}
	}

	void handleTriggers(const uint8_t section, const bool isTrigger, const bool isReset) {
		if (isTrigger) {
			switch (channelFunction[section]) {
			case SECTION_FUNCTION_FACTORER:
				if (channelFactor[section] > kFactorerBypassValue) {
					if (divisionCounter[section] <= 0) {
						channelState[section] = CHANNEL_GENERATED; // divide converts thru to exec on every division
					}
					// Deal with counter.
					if (divisionCounter[section] >= channelFactor[section] - 1) {
						resetDivision(section);
					}
					else {
						++divisionCounter[section];
					}
				}
				else {
					// Mult always acknowledges thru.
					channelState[section] = CHANNEL_THRU;
				}
				break;
			case SECTION_FUNCTION_SWING:
				switch (swingCounter[section]) {
				case 0: // thru beat
					channelState[section] = CHANNEL_THRU;
					swingCounter[section] = 1;
					break;
				case 1:
					// Skipped thru beat.
					// Unless lowest setting, no swing - should do thru.
					if (channelSwing[section] <= kSwingFactorMin) {
						channelState[section] = CHANNEL_GENERATED;
						resetSwing(section);
					}
					else {
						// Rest.
						channelState[section] = CHANNEL_REST;
						swingCounter[section] = 2;
					}
					break;
				default:
					resetSwing(section); // Something is wrong if we're here, so reset.
					break;
				}
				break;
			}
		}
		if (isReset) {
			handleReset(section);
		}
	}

	void transformClock(const uint8_t section) {
		switch (channelFunction[section]) {
		case SECTION_FUNCTION_FACTORER:
			if (channelFactor[section] < kFactorerBypassValue && pulseTrackerRecordedCount >= kPulseTrackerBufferSize &&
				checkMultiplyStrikeTurn(section, getPulseTrackerElapsed())) {
				channelState[section] = CHANNEL_GENERATED;
				multiplyDebouncing[section] = true;
			}
			break;
		case SECTION_FUNCTION_SWING:
			if (checkSwingStrikeTurn(section, getPulseTrackerElapsed())) {
				channelState[section] = CHANNEL_GENERATED;
				resetSwing(section); // reset
			}
			break;
		}
	}

	void setOutputVoltages(const uint8_t section) {
		if (channelState[section] > CHANNEL_REST) {
			outputs[OUTPUT_OUT1A + section].setVoltage(10.f);
			outputs[OUTPUT_OUT1B + section].setVoltage(10.f);
			triggerExtendCount[section] = kTriggerExtendCount;
			if (channelState[section] < CHANNEL_GENERATED) {
				ledGateDuration[section] = kLedThruGateDuration;
				ledState[section] = CHANNEL_THRU;
			}
			else {
				ledGateDuration[section] = kLedGeneratedGateDuration;
				ledState[section] = CHANNEL_GENERATED;
			}
		}
		else {
			if (triggerExtendCount[section] <= 0) {
				outputs[OUTPUT_OUT1A + section].setVoltage(0.f);
				outputs[OUTPUT_OUT1B + section].setVoltage(0.f);
			}
			else {
				triggerExtendCount[section] -= 1;
			}
		}
	}

	void updateChannelLeds(const uint8_t channel, const float sampleTime) {
		if (ledGateDuration[channel]) {
			--ledGateDuration[channel];
			if (!ledGateDuration[channel]) {
				ledState[channel] = CHANNEL_REST;
			}
		}

		int currentLight = LIGHTS_STATE + channel * 2;
		switch (ledState[channel]) {
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

	void init() {
		pulseTrackerBuffer[kPulseTrackerBufferSize - 2] = 0;
		pulseTrackerBuffer[kPulseTrackerBufferSize - 1] = 0;
		pulseTrackerRecordedCount = 0;

		for (uint8_t i = 0; i < kMaxModuleSections; ++i) {
			triggerExtendCount[i] = 0;
		}
		moduleClock.reset();
	}

	bool checkMultiplyStrikeTurn(const uint8_t section, const uint16_t elapsed) {
		float interval = getPulseTrackerPeriod() / -channelFactor[section];
		if (fmod(elapsed, interval) <= kTimingErrorCorrectionAmount) {
			if (!multiplyDebouncing[section]) {
				return true;
			}
		}
		else {
			multiplyDebouncing[section] = false;
		}
		return false;
	}

	void resetDivision(const uint8_t channel) {
		divisionCounter[channel] = 0;
	}

	void resetSwing(const uint8_t channel) {
		swingCounter[channel] = 0;
	}

	bool checkSwingStrikeTurn(uint8_t section, uint16_t elapsed) {
		if (swingCounter[section] >= 2 && channelSwing[section] > kSwingFactorMin) {
			uint16_t period = getPulseTrackerPeriod();
			uint16_t interval = ((10 * (period * 2)) / (1000 / channelSwing[section])) - period;
			return (elapsed >= interval && elapsed <= interval + kTimingErrorCorrectionAmount);
		}
		else {
			// thru
			return false;
		}
	}

	void onReset() override {
		params[PARAM_MODE1].setValue(1);
		params[PARAM_MODE2].setValue(0);
		params[PARAM_FACTOR1].setValue(0.5f);
		params[PARAM_FACTOR2].setValue(0.5f);
	}
};

struct ViminaWidget : SanguineModuleWidget {
	ViminaWidget(Vimina* module) {
		setModule(module);

		moduleName = "vimina";
		panelSize = SIZE_6;
		backplateColor = PLATE_RED;

		makePanel();

		addScrews(SCREW_TOP_LEFT | SCREW_BOTTOM_LEFT);

		// Channel 1
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(4.622, 16.723), module,
			Vimina::PARAM_MODE1, Vimina::LIGHTS_MODE + 0 * 2));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(25.863, 16.723),
			module, Vimina::PARAM_RESET1));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(15.24, 29.079), module, Vimina::PARAM_FACTOR1));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(6.012, 44.303), module, Vimina::INPUT_RESET));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(24.481, 44.303), module, Vimina::INPUT_CV1));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(6.012, 59.959), module, Vimina::OUTPUT_OUT1A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 59.959), module, Vimina::LIGHTS_STATE + 0 * 2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(24.481, 59.959), module, Vimina::OUTPUT_OUT1B));

		// Channel 2
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(4.622, 74.653), module,
			Vimina::PARAM_MODE2, Vimina::LIGHTS_MODE + 1 * 2));

		addParam(createParamCentered<VCVButton>(millimetersToPixelsVec(25.863, 74.653),
			module, Vimina::PARAM_RESET2));

		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(15.24, 87.008), module, Vimina::PARAM_FACTOR2));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(6.012, 102.232), module, Vimina::INPUT_CLOCK));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(24.481, 102.232), module, Vimina::INPUT_CV2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(6.012, 117.888), module, Vimina::OUTPUT_OUT2A));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(15.24, 117.888), module, Vimina::LIGHTS_STATE + 1 * 2));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(24.481, 117.888), module, Vimina::OUTPUT_OUT2B));
	}
};

Model* modelVimina = createModel<Vimina, ViminaWidget>("Sanguine-Vimina");