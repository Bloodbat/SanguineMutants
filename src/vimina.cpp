#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "sanguinejson.hpp"
#include "vimina.hpp"

using namespace sanguineCommonCode;

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

	// Channel ids
	static const int kResetChannel = 0;
	static const int kClockChannel = 1;

	static const int kTriggerExtendCount = 64;

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
	int triggerCounts[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	uint32_t pulseTrackerRecordedCounts[PORT_MAX_CHANNELS];

	uint32_t ledGateDurations[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	uint32_t swingCounters[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	uint32_t pulseTrackerBuffers[kPulseTrackerBufferSize][PORT_MAX_CHANNELS] = {};

	uint32_t tmrModuleClock[PORT_MAX_CHANNELS]; // Replaces the ATMega88pa's TCNT1

	int32_t divisionCounters[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	int32_t channelFactors[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	int32_t channelSwings[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	ChannelStates channelStates[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	ChannelStates ledStates[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	uint8_t triggerExtendCounts[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	// Swing constants
	static constexpr float kSwingFactorMin = 50.f;
	static constexpr float kSwingFactorMax = 70.f; // Maximum swing amount can be set up to 99.

	// Scaling constant
	static constexpr float kMaxParamValue = 1.f;

	static constexpr float kSwingConversionFactor = kMaxParamValue / (kSwingFactorMax - kSwingFactorMin);
	static constexpr float kFactorerConversionFactor = kMaxParamValue / (kFactorCount - 1.f);

	static constexpr float kEdgeVoltageThreshold = 2.f;

	float channelVoltage[kMaxModuleSections][PORT_MAX_CHANNELS] = {};

	float lastSectionFactors[kMaxModuleSections] = { 0.5f, 0.5f };

	bool inputGateState[kMaxModuleSections][PORT_MAX_CHANNELS] = {};
	bool isMultiplyDebouncing[kMaxModuleSections][PORT_MAX_CHANNELS];

	SectionFunctions channelFunction[kMaxModuleSections] = {
		SECTION_FUNCTION_SWING,
		SECTION_FUNCTION_FACTORER
	};

	dsp::BooleanTrigger btReset[kMaxModuleSections];
	dsp::ClockDivider lightsDivider;

	std::string sectionTooltips[kMaxModuleSections] = {
		vimina::clockPrefix + "Swing 60.00%",
		vimina::clockPrefix + vimina::factorLabels[0]
	};

	Vimina() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);
		for (int section = 0; section < kMaxModuleSections; ++section) {
			const int sectionNumber = section + 1;
			configParam(PARAM_FACTOR_1 + section, 0.f, kMaxParamValue, 0.5f, string::f("Channel %d factor", sectionNumber));
			configSwitch(PARAM_RESET_1 + section, 0.f, 1.f, 0.f, string::f("Channel %d reset", sectionNumber));

			configInput(INPUT_CV1 + section, string::f("Channel %d factor", sectionNumber));
			configOutput(OUTPUT_OUT_1A + section, string::f("Channel %d A", sectionNumber));
			configOutput(OUTPUT_OUT_1B + section, string::f("Channel %d B", sectionNumber));
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

		bool bIsClockConnected = inputs[INPUT_CLOCK].isConnected();

		bool bIsResetConnected = inputs[INPUT_RESET].isConnected();

		bool isResetRequest[kMaxModuleSections] = {};

		for (int section = 0; section < kMaxModuleSections; ++section) {
			isResetRequest[section] = btReset[section].process(params[PARAM_RESET_1 + section].getValue());
		}

		outputs[OUTPUT_OUT_1A].setChannels(channelCount);
		outputs[OUTPUT_OUT_1B].setChannels(channelCount);
		outputs[OUTPUT_OUT_2A].setChannels(channelCount);
		outputs[OUTPUT_OUT_2B].setChannels(channelCount);

		for (int channel = 0; channel < channelCount; ++channel) {
			++tmrModuleClock[channel];
			bool bIsTrigger = false;

			if (bIsClockConnected) {
				if (isRisingEdge(kClockChannel, INPUT_CLOCK, kEdgeVoltageThreshold, channel)) {
					/*
					   Pulse tracker is always recording. this should help smooth transitions
					   between functions even though divide doesn't use it.
					*/
					// Shift
					pulseTrackerBuffers[kPulseTrackerBufferSize - 2][channel] =
						pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel];
					pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel] =
						tmrModuleClock[channel];
					if (pulseTrackerRecordedCounts[channel] < kPulseTrackerBufferSize) {
						++pulseTrackerRecordedCounts[channel];
					}
					bIsTrigger = true;
				}
			}

			bool bIsReset = false;
			if (bIsResetConnected) {
				bIsReset = isRisingEdge(kResetChannel, INPUT_RESET, kEdgeVoltageThreshold, channel);
			}

			for (uint8_t section = 0; section < kMaxModuleSections; ++section) {
				channelFunction[section] = SectionFunctions(params[PARAM_MODE_1 + section].getValue());

				if (isResetRequest[section]) {
					handleReset(section, channel);
				}

				if (bIsClockConnected) {
					setupChannel(section, channel);

					handleTriggers(section, bIsTrigger, bIsReset, channel);

					transformClock(section, channel);

					setOutputVoltages(section, channel);
				}
				channelStates[section][channel] = CHANNEL_REST; // Clean up.
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

				float sectionFactor = params[PARAM_FACTOR_1 + section].getValue();

				if (lastSectionFactors[section] != sectionFactor) {
					float swingFactor = 0.f;
					int factorIndex = 0;

					switch (channelFunction[section]) {
					case SECTION_FUNCTION_SWING:
						swingFactor = params[PARAM_FACTOR_1 + section].getValue() / kSwingConversionFactor + kSwingFactorMin;

						sectionTooltips[section] = vimina::clockPrefix + string::f("Swing %.2f%%", swingFactor);
						break;

					case SECTION_FUNCTION_FACTORER:
						factorIndex = static_cast<int16_t>(std::round(params[PARAM_FACTOR_1 + section].getValue() /
							kFactorerConversionFactor - kFactorerBypassIndex));

						if (factorIndex < 0) {
							factorIndex = -factorIndex; // abs
						} else if (factorIndex > 0) {
							factorIndex += kFactorerBypassIndex;
						}
						sectionTooltips[section] = vimina::clockPrefix + vimina::factorLabels[factorIndex];
						break;
					}
					lastSectionFactors[section] = sectionFactor;
				}
				getParamQuantity(PARAM_FACTOR_1 + section)->description = sectionTooltips[section];
			}
		}

		updateChannelLeds(0, args.sampleTime, ledsChannel);
		updateChannelLeds(1, args.sampleTime, ledsChannel);
	}

	uint32_t getPulseTrackerElapsed(const int channel) {
		return (tmrModuleClock[channel] >= pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel]) ?
			tmrModuleClock[channel] - pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel] :
			tmrModuleClock[channel] + (UINT32_MAX - pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel]);
	}

	uint32_t getPulseTrackerPeriod(const int channel) {
		return (pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel] >= pulseTrackerBuffers[kPulseTrackerBufferSize - 2][channel]) ?
			pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel] - pulseTrackerBuffers[kPulseTrackerBufferSize - 2][channel] :
			pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel] + (UINT32_MAX - pulseTrackerBuffers[kPulseTrackerBufferSize - 2][channel]);
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
					if (divisionCounters[section][channel] <= 0) {
						channelStates[section][channel] = CHANNEL_GENERATED; // Divide converts thru to exec on every division.
					}
					// Deal with counter.
					if (divisionCounters[section][channel] >= channelFactors[section][channel] - 1) {
						resetDivision(section, channel);
					} else {
						++divisionCounters[section][channel];
					}
				} else {
					// Mult always acknowledges thru.
					channelStates[section][channel] = CHANNEL_THRU;
					triggerCounts[section][channel] = 0;
				}
				break;
			case SECTION_FUNCTION_SWING:
				switch (swingCounters[section][channel]) {
				case 0: // Thru beat.
					channelStates[section][channel] = CHANNEL_THRU;
					swingCounters[section][channel] = 1;
					break;
				case 1:
					// Skipped thru beat.
					// Unless lowest setting, no swing - should do thru.
					if (channelSwings[section][channel] <= kSwingFactorMin) {
						channelStates[section][channel] = CHANNEL_GENERATED;
						resetSwing(section, channel);
					} else {
						// Rest.
						channelStates[section][channel] = CHANNEL_REST;
						swingCounters[section][channel] = 2;
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
			pulseTrackerBuffers[kPulseTrackerBufferSize - 2][channel] = 0;
			pulseTrackerBuffers[kPulseTrackerBufferSize - 1][channel] = 0;
			pulseTrackerRecordedCounts[channel] = 0;

			for (uint8_t section = 0; section < kMaxModuleSections; ++section) {
				triggerExtendCounts[section][channel] = 0;
				isMultiplyDebouncing[section][channel] = false;
			}
			tmrModuleClock[channel] = 0;
		}
	}

	bool isDivideEnabled(const uint8_t section, const int channel) {
		return channelFactors[section][channel] > kFactorerBypassValue;
	}

	bool isMultiplyEnabled(const uint8_t section, const int channel) {
		return channelFactors[section][channel] < kFactorerBypassValue;
	}

	bool isMultiplyStrikeTurn(const uint8_t section, const uint32_t elapsed, const int channel) {
		float interval = getPulseTrackerPeriod(channel) / -channelFactors[section][channel];
		if (fmod(elapsed, interval) <= kTimingErrorCorrectionAmount) {
			if (!isMultiplyDebouncing[section][channel]) {
				return true;
			}
		} else {
			isMultiplyDebouncing[section][channel] = false;
		}
		return false;
	}

	bool isPulseTrackerPeriod(const int channel) {
		return pulseTrackerRecordedCounts[channel] >= kPulseTrackerBufferSize;
	}

	bool isRisingEdge(const uint8_t section, const int port, const float threshold, const int channel) {
		bool bLastGateState = inputGateState[section][channel];
		inputGateState[section][channel] = inputs[port].getVoltage(channel) >= threshold;
		return inputGateState[section][channel] & !(bLastGateState);
	}

	bool isSwingStrikeTurn(const uint8_t section, const uint32_t elapsed, const int channel) {
		if (swingCounters[section][channel] >= 2 && channelSwings[section][channel] > kSwingFactorMin) {
			uint32_t period = getPulseTrackerPeriod(channel);
			uint32_t interval = ((10 * (period * 2)) / (1000 / channelSwings[section][channel])) - period;
			return (elapsed >= interval && elapsed <= interval + kTimingErrorCorrectionAmount);
		} else {
			// Thru.
			return false;
		}
	}

	void resetDivision(const uint8_t section, const int channel) {
		divisionCounters[section][channel] = 0;
	}

	void resetSwing(const uint8_t section, const int channel) {
		swingCounters[section][channel] = 0;
	}

	void setOutputVoltages(const uint8_t section, const int channel) {
		if (channelStates[section][channel] > CHANNEL_REST) {
			outputs[OUTPUT_OUT_1A + section].setVoltage(10.f, channel);
			outputs[OUTPUT_OUT_1B + section].setVoltage(10.f, channel);
			triggerExtendCounts[section][channel] = kTriggerExtendCount;

			switch (channelStates[section][channel])
			{
			case CHANNEL_GENERATED:
				ledGateDurations[section][channel] = kLedGeneratedGateDuration;
				ledStates[section][channel] = CHANNEL_GENERATED;
				break;
			case CHANNEL_THRU:
				ledGateDurations[section][channel] = kLedThruGateDuration;
				ledStates[section][channel] = CHANNEL_THRU;
				break;
			default:
				break;
			}
		} else {
			if (triggerExtendCounts[section][channel] == 0) {
				outputs[OUTPUT_OUT_1A + section].setVoltage(0.f, channel);
				outputs[OUTPUT_OUT_1B + section].setVoltage(0.f, channel);
			} else {
				--triggerExtendCounts[section][channel];
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
				kFactorerConversionFactor - kFactorerBypassIndex);
			// Offset result so that there are no -1 or 0 factors, but values are still evenly spaced.
			if (factorIndex == 0) {
				channelFactors[section][channel] = kFactorerBypassValue;
			} else if (factorIndex < 0) {
				channelFactors[section][channel] = --factorIndex; // abs
			} else {
				channelFactors[section][channel] = ++factorIndex;
			}
			break;
		case SECTION_FUNCTION_SWING:
			channelSwings[section][channel] = channelVoltage[section][channel] /
				kSwingConversionFactor + kSwingFactorMin;
			break;
		}
	}

	void transformClock(const uint8_t section, const int channel) {
		switch (channelFunction[section]) {
		case SECTION_FUNCTION_FACTORER:
			if (isMultiplyEnabled(section, channel) && isPulseTrackerPeriod(channel) &&
				isMultiplyStrikeTurn(section, getPulseTrackerElapsed(channel), channel) &&
				triggerCounts[section][channel] >= channelFactors[section][channel]) {
				channelStates[section][channel] = CHANNEL_GENERATED;
				isMultiplyDebouncing[section][channel] = true;
				--triggerCounts[section][channel];
			}
			break;
		case SECTION_FUNCTION_SWING:
			if (isSwingStrikeTurn(section, getPulseTrackerElapsed(channel), channel)) {
				channelStates[section][channel] = CHANNEL_GENERATED;
				resetSwing(section, channel);
			}
			break;
		}
	}

	void updateChannelLeds(const uint8_t section, const float sampleTime, const int channel) {
		if (ledGateDurations[section][channel]) {
			--ledGateDurations[section][channel];
			if (!ledGateDurations[section][channel]) {
				ledStates[section][channel] = CHANNEL_REST;
			}
		}

		int currentLight = LIGHTS_STATE + section * 2;
		switch (ledStates[section][channel]) {
		case CHANNEL_REST:
			lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
			lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
			break;
		case CHANNEL_THRU:
			lights[currentLight + 0].setBrightnessSmooth(1.f, sampleTime);
			lights[currentLight + 1].setBrightness(0.f);
			break;
		case CHANNEL_GENERATED:
			lights[currentLight + 0].setBrightness(0.f);
			lights[currentLight + 1].setBrightnessSmooth(1.f, sampleTime);
			break;
		}
	}

	void onReset(const ResetEvent& e) override {
		params[PARAM_MODE_1].setValue(1);
		params[PARAM_MODE_2].setValue(0);
		params[PARAM_FACTOR_1].setValue(0.5f);
		params[PARAM_FACTOR_2].setValue(0.5f);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "ledsChannel", ledsChannel);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "ledsChannel", intValue)) {
			ledsChannel = intValue;
		}
	}
};

struct ViminaWidget : SanguineModuleWidget {
	explicit ViminaWidget(Vimina* module) {
		setModule(module);

		moduleName = "vimina";
		panelSize = sanguineThemes::SIZE_6;
		backplateColor = sanguineThemes::PLATE_RED;

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