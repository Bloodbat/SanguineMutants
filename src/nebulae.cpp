#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"
#include "sanguinechannels.hpp"
#include "array"

#include "clouds/dsp/granular_processor.h"

#include "nebulae.hpp"

using simd::float_4;

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Nebulae : SanguineModule {
	enum ParamIds {
		PARAM_MODE,
		PARAM_FREEZE,
		PARAM_POSITION,
		PARAM_DENSITY,
		PARAM_SIZE,
		PARAM_TEXTURE,
		PARAM_PITCH,
		PARAM_IN_GAIN,
		PARAM_BLEND,
		PARAM_SPREAD,
		PARAM_FEEDBACK,
		PARAM_REVERB,
		PARAM_HI_FI,
		PARAM_STEREO,
		PARAM_LEDS_MODE,
		PARAM_OUT_GAIN,
		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_FREEZE,
		INPUT_POSITION,
		INPUT_DENSITY,
		INPUT_SIZE,
		INPUT_TEXTURE,
		INPUT_PITCH,
		INPUT_TRIGGER,
		INPUT_BLEND,
		INPUT_SPREAD,
		INPUT_FEEDBACK,
		INPUT_REVERB,
		INPUT_LEFT,
		INPUT_RIGHT,
		INPUT_MODE,
		INPUT_IN_GAIN,
		INPUT_OUT_GAIN,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_LEFT,
		OUTPUT_RIGHT,
		OUTPUTS_COUNT
	};

	enum LightIds {
		LIGHT_FREEZE,
		LIGHT_BLEND,
		LIGHT_SPREAD,
		LIGHT_FEEDBACK,
		LIGHT_REVERB,
		ENUMS(LIGHT_POSITION_CV, 2),
		ENUMS(LIGHT_DENSITY_CV, 2),
		ENUMS(LIGHT_SIZE_CV, 2),
		ENUMS(LIGHT_TEXTURE_CV, 2),
		LIGHT_HI_FI,
		LIGHT_STEREO,
		ENUMS(LIGHT_CHANNEL_1, 3),
		ENUMS(LIGHT_CHANNEL_2, 3),
		ENUMS(LIGHT_CHANNEL_3, 3),
		ENUMS(LIGHT_CHANNEL_4, 3),
		ENUMS(LIGHT_CHANNEL_5, 3),
		ENUMS(LIGHT_CHANNEL_6, 3),
		ENUMS(LIGHT_CHANNEL_7, 3),
		ENUMS(LIGHT_CHANNEL_8, 3),
		ENUMS(LIGHT_CHANNEL_9, 3),
		ENUMS(LIGHT_CHANNEL_10, 3),
		ENUMS(LIGHT_CHANNEL_11, 3),
		ENUMS(LIGHT_CHANNEL_12, 3),
		ENUMS(LIGHT_CHANNEL_13, 3),
		ENUMS(LIGHT_CHANNEL_14, 3),
		ENUMS(LIGHT_CHANNEL_15, 3),
		ENUMS(LIGHT_CHANNEL_16, 3),
		LIGHTS_COUNT
	};

	cloudyCommon::LedModes ledMode = cloudyCommon::LEDS_INPUT;

	cloudyCommon::LedModes lastLedMode = cloudyCommon::LEDS_INPUT;

	std::string textMode = nebulae::modeListDisplay[0];
#ifndef METAMODULE
	std::string textFreeze = nebulae::modeDisplays[0].labelFreeze;
	std::string textPosition = nebulae::modeDisplays[0].labelPosition;
	std::string textDensity = nebulae::modeDisplays[0].labelDensity;
	std::string textSize = nebulae::modeDisplays[0].labelSize;
	std::string textTexture = nebulae::modeDisplays[0].labelTexture;
	std::string textPitch = nebulae::modeDisplays[0].labelPitch;
	std::string textTrigger = nebulae::modeDisplays[0].labelTrigger;
#endif

	dsp::SampleRateConverter<PORT_MAX_CHANNELS * 2> srcInputs;
	dsp::SampleRateConverter<PORT_MAX_CHANNELS * 2> srcOutputs;
	dsp::DoubleRingBuffer<dsp::Frame<PORT_MAX_CHANNELS * 2>, 256> drbInputBuffers;
	dsp::DoubleRingBuffer<dsp::Frame<PORT_MAX_CHANNELS * 2>, 256> drbOutputBuffers;
	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider lightsDivider;
	dsp::BooleanTrigger btLedsMode;

	clouds::PlaybackMode knobPlaybackMode = clouds::PLAYBACK_MODE_GRANULAR;
	clouds::PlaybackMode lastPlaybackMode = clouds::PLAYBACK_MODE_LAST;

	std::array<clouds::PlaybackMode, PORT_MAX_CHANNELS> channelModes;

	int channelCount;
	int displayChannel = 0;

	static const int kLightsFrequency = 64;
	int jitteredLightsFrequency;

	int stereoChannels = 2;

	uint8_t* buffersLarge[PORT_MAX_CHANNELS];
	uint8_t* buffersSmall[PORT_MAX_CHANNELS];

	bool lastFrozen[PORT_MAX_CHANNELS];
	bool bDisplaySwitched = false;
	bool bTriggersAreGates = true;
	bool lastTriggered[PORT_MAX_CHANNELS];

	bool bHaveModeCable = false;
	bool bRightInputConnected = false;
	bool bLeftOutputConnected = false;
	bool bRightOutputConnected = false;

	bool bWantLoFi = false;
	bool bFrozen = false;

	clouds::GranularProcessor* cloudsProcessors[PORT_MAX_CHANNELS];
	clouds::Parameters* cloudsParameters[PORT_MAX_CHANNELS];

	float_4 knobValues;
	float_4 sliderValues;
	float_4 voltages1[PORT_MAX_CHANNELS];

	float paramPitch = 0.f;

	float inputGain = 0.5f;
	float outputGain = 1.f;

	dsp::Frame<PORT_MAX_CHANNELS * 2> inputFrames;
	dsp::Frame<PORT_MAX_CHANNELS * 2> outputFrames = {};

	Nebulae() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configInput(INPUT_FREEZE, "Freeze");
		configSwitch(PARAM_FREEZE, 0.f, 1.f, 0.f, "Freeze", cloudyCommon::freezeButtonLabels);

		configButton(PARAM_LEDS_MODE, "LED display value: Input");

		configSwitch(PARAM_MODE, 0.f, 3.f, 0.f, "Mode", nebulae::modeListMenu);

		configParam(PARAM_POSITION, 0.f, 1.f, 0.5f, "Grain position", "%", 0.f, 100.f);
		configInput(INPUT_POSITION, "Grain position CV");

		configParam(PARAM_DENSITY, 0.f, 1.f, 0.5f, "Grain density", "%", 0.f, 100.f);
		configInput(INPUT_DENSITY, "Grain density CV");

		configParam(PARAM_SIZE, 0.f, 1.f, 0.5f, "Grain size", "%", 0.f, 100.f);
		configInput(INPUT_SIZE, "Grain size CV");

		configParam(PARAM_TEXTURE, 0.f, 1.f, 0.5f, "Grain texture", "%", 0.f, 100.f);
		configInput(INPUT_TEXTURE, "Grain texture CV");

		configParam(PARAM_PITCH, -2.f, 2.f, 0.f, "Grain pitch");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");

		configParam(PARAM_BLEND, 0.f, 1.f, 0.5f, "Dry ↔ wet", "%", 0.f, 100.f);
		configInput(INPUT_BLEND, "Dry / wet CV");

		configInput(INPUT_TRIGGER, "Trigger");

		configInput(INPUT_SPREAD, "Spread CV");
		configParam(PARAM_SPREAD, 0.f, 1.f, 0.5f, "Spread", "%", 0.f, 100.f);

		configInput(INPUT_FEEDBACK, "Feedback CV");
		configParam(PARAM_FEEDBACK, 0.f, 1.f, 0.5f, "Feedback", "%", 0.f, 100.f);

		configInput(INPUT_REVERB, "Reverb CV");
		configParam(PARAM_REVERB, 0.f, 1.f, 0.5f, "Reverb", "%", 0.f, 100.f);

		configInput(INPUT_IN_GAIN, "Input gain CV");
		configInput(INPUT_OUT_GAIN, "Output gain CV");

		configParam(PARAM_IN_GAIN, 0.f, 1.f, 0.5f, "Input gain", "%", 0.f, 100.f);

		configParam(PARAM_OUT_GAIN, 0.f, 2.f, 1.f, "Output gain", "%", 0.f, 100.f);

		configInput(INPUT_IN_GAIN, "Output gain CV");

		configInput(INPUT_LEFT, "Left");
		configInput(INPUT_RIGHT, "Right");

		configSwitch(PARAM_HI_FI, 0.f, 1.f, 1.f, "Buffer resolution", cloudyCommon::bufferQualityLabels);
		configSwitch(PARAM_STEREO, 0.f, 1.f, 1.f, "Buffer channels", cloudyCommon::bufferChannelsLabels);

		configOutput(OUTPUT_LEFT, "Left");
		configOutput(OUTPUT_RIGHT, "Right");

		configInput(INPUT_MODE, "Mode");

		configBypass(INPUT_LEFT, OUTPUT_LEFT);
		configBypass(INPUT_RIGHT, OUTPUT_RIGHT);

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			lastTriggered[channel] = false;
			lastFrozen[channel] = false;

			buffersLarge[channel] = new uint8_t[cloudyCommon::kBigBufferLength]();
			buffersSmall[channel] = new uint8_t[cloudyCommon::kSmallBufferLength]();
			cloudsProcessors[channel] = new clouds::GranularProcessor();
			memset(cloudsProcessors[channel], 0, sizeof(clouds::GranularProcessor));
			cloudsProcessors[channel]->Init(buffersLarge[channel], cloudyCommon::kBigBufferLength,
				buffersSmall[channel], cloudyCommon::kSmallBufferLength);
		}

		channelModes.fill(clouds::PLAYBACK_MODE_GRANULAR);
	}

	~Nebulae() {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			delete cloudsProcessors[channel];
			delete[] buffersSmall[channel];
			delete[] buffersLarge[channel];
		}
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(std::max(inputs[INPUT_LEFT].getChannels(), inputs[INPUT_RIGHT].getChannels()), 1);

		if (displayChannel >= channelCount) {
			displayChannel = channelCount - 1;
		}

		int currentChannel;
		// Get input.
		if (!drbInputBuffers.full()) {
			for (int channel = 0; channel < channelCount; ++channel) {
				currentChannel = channel << 1;
				float finalInputGain = clamp(inputGain + (inputs[INPUT_IN_GAIN].getVoltage(channel) / 5.f), 0.f, 1.f);
				inputFrames.samples[currentChannel] = inputs[INPUT_LEFT].getVoltage(channel) * finalInputGain / 5.f;
				inputFrames.samples[currentChannel + 1] = bRightInputConnected ? inputs[INPUT_RIGHT].getVoltage(channel)
					* finalInputGain / 5.f : inputFrames.samples[currentChannel];
			}
			drbInputBuffers.push(inputFrames);
		}

		// Render frames.
		if (drbOutputBuffers.empty()) {
			// Convert input buffer.
			dsp::Frame<PORT_MAX_CHANNELS * 2> convertedFrames[clouds::kMaxBlockSize];
			int inputLength = drbInputBuffers.size();
			int outputLength = clouds::kMaxBlockSize;
			srcInputs.setChannels(channelCount << 1);
			srcInputs.process(drbInputBuffers.startData(), &inputLength, convertedFrames, &outputLength);
			drbInputBuffers.startIncr(inputLength);

			dsp::Frame<PORT_MAX_CHANNELS * 2> renderedFrames[clouds::kMaxBlockSize];

			for (int channel = 0; channel < channelCount; ++channel) {
				currentChannel = channel << 1;

				clouds::ShortFrame input[clouds::kMaxBlockSize] = {};

				/*
				   We might not fill all of the input buffer if there is a deficiency, but this cannot be avoided due to imprecisions
				   between the input and output SRC.
				*/
				for (int block = 0; block < outputLength; ++block) {
					input[block].l = clamp(convertedFrames[block].samples[currentChannel] * 32767.0, -32768, 32767);
					input[block].r = clamp(convertedFrames[block].samples[currentChannel + 1] * 32767.0, -32768, 32767);
				}

				cloudsParameters[channel] = cloudsProcessors[channel]->mutable_parameters();

				voltages1[channel][0] = inputs[INPUT_POSITION].getVoltage(channel);
				voltages1[channel][1] = inputs[INPUT_DENSITY].getVoltage(channel);
				voltages1[channel][2] = inputs[INPUT_SIZE].getVoltage(channel);
				voltages1[channel][3] = inputs[INPUT_TEXTURE].getVoltage(channel);

				// Set up Clouds processor.
				cloudsProcessors[channel]->set_playback_mode(channelModes[channel]);

				cloudsProcessors[channel]->set_num_channels(stereoChannels);
				cloudsProcessors[channel]->set_low_fidelity(bWantLoFi);
				cloudsProcessors[channel]->Prepare();

				float_4 scaledVoltages;

				scaledVoltages[0] = inputs[INPUT_BLEND].getVoltage(channel);
				scaledVoltages[1] = inputs[INPUT_SPREAD].getVoltage(channel);
				scaledVoltages[2] = inputs[INPUT_FEEDBACK].getVoltage(channel);
				scaledVoltages[3] = inputs[INPUT_REVERB].getVoltage(channel);

				scaledVoltages /= 5.f;

				scaledVoltages += knobValues;

				scaledVoltages = clamp(scaledVoltages, 0.f, 1.f);

				cloudsParameters[channel]->dry_wet = scaledVoltages[0];
				cloudsParameters[channel]->stereo_spread = scaledVoltages[1];
				cloudsParameters[channel]->feedback = scaledVoltages[2];
				cloudsParameters[channel]->reverb = scaledVoltages[3];

				scaledVoltages = voltages1[channel];
				scaledVoltages /= 5.f;

				scaledVoltages += sliderValues;

				scaledVoltages = clamp(scaledVoltages, 0.f, 1.f);

				cloudsParameters[channel]->position = scaledVoltages[0];
				cloudsParameters[channel]->density = scaledVoltages[1];
				cloudsParameters[channel]->size = scaledVoltages[2];
				cloudsParameters[channel]->texture = scaledVoltages[3];

				// Trigger.
				bool bIsGate = inputs[INPUT_TRIGGER].getVoltage(channel) >= 1.f;

				cloudsParameters[channel]->trigger = (bTriggersAreGates & bIsGate) |
					((!bTriggersAreGates) & (bIsGate & (!lastTriggered[channel])));
				cloudsParameters[channel]->gate = bIsGate;

				lastTriggered[channel] = bIsGate;

				cloudsParameters[channel]->freeze = ((inputs[INPUT_FREEZE].getVoltage(channel) >= 1.f) | bFrozen);

				cloudsParameters[channel]->pitch = clamp((paramPitch + inputs[INPUT_PITCH].getVoltage(channel)) * 12.f, -48.f, 48.f);

				if (bFrozen && !lastFrozen[channel]) {
					lastFrozen[channel] = true;
					if (!bDisplaySwitched && displayChannel == channel) {
						ledMode = cloudyCommon::LEDS_OUTPUT;
						lastLedMode = cloudyCommon::LEDS_OUTPUT;
					}
				} else if (!bFrozen && lastFrozen[channel]) {
					lastFrozen[channel] = false;
					if (!bDisplaySwitched && displayChannel == channel) {
						ledMode = cloudyCommon::LEDS_INPUT;
						lastLedMode = cloudyCommon::LEDS_INPUT;
					} else {
						bDisplaySwitched = false;
					}
				}

				clouds::ShortFrame output[clouds::kMaxBlockSize];
				cloudsProcessors[channel]->Process(input, output, clouds::kMaxBlockSize);

				// Convert output buffer.
				for (size_t block = 0; block < clouds::kMaxBlockSize; ++block) {
					renderedFrames[block].samples[currentChannel] = output[block].l / 32768.f;
					renderedFrames[block].samples[currentChannel + 1] = output[block].r / 32768.f;
				}
			}

			int inCount = clouds::kMaxBlockSize;
			int outCount = drbOutputBuffers.capacity();
			srcOutputs.setChannels(channelCount << 1);
			srcOutputs.process(renderedFrames, &inCount, drbOutputBuffers.endData(), &outCount);
			drbOutputBuffers.endIncr(outCount);
		}

		// Set output.
		if (!drbOutputBuffers.empty()) {
			outputFrames = drbOutputBuffers.shift();
			for (int channel = 0; channel < channelCount; ++channel) {
				currentChannel = channel << 1;
				float finalOutputGain = clamp(outputGain + (inputs[INPUT_OUT_GAIN].getVoltage(channel) / 5.f), 0.f, 2.f);
				if (bLeftOutputConnected) {
					outputFrames.samples[currentChannel] *= finalOutputGain;
					outputs[OUTPUT_LEFT].setVoltage(5.f * outputFrames.samples[currentChannel], channel);
				}
				int currentSample = currentChannel + 1;
				if (bRightOutputConnected) {
					outputFrames.samples[currentSample] *= finalOutputGain;
					outputs[OUTPUT_RIGHT].setVoltage(5.f * outputFrames.samples[currentSample], channel);
				}
			}
		}

		outputs[OUTPUT_LEFT].setChannels(channelCount);
		outputs[OUTPUT_RIGHT].setChannels(channelCount);

		// Lights.
		if (lightsDivider.process()) { // Expensive, so call this infrequently!
			const float sampleTime = args.sampleTime * jitteredLightsFrequency;

			stereoChannels = static_cast<int>(params[PARAM_STEREO].getValue()) + 1;
			bWantLoFi = params[PARAM_HI_FI].getValue() < 1.f;

			bFrozen = params[PARAM_FREEZE].getValue() >= 1.f;

			knobValues[0] = params[PARAM_BLEND].getValue();
			knobValues[1] = params[PARAM_SPREAD].getValue();
			knobValues[2] = params[PARAM_FEEDBACK].getValue();
			knobValues[3] = params[PARAM_REVERB].getValue();

			sliderValues[0] = params[PARAM_POSITION].getValue();
			sliderValues[1] = params[PARAM_DENSITY].getValue();
			sliderValues[2] = params[PARAM_SIZE].getValue();
			sliderValues[3] = params[PARAM_TEXTURE].getValue();

			paramPitch = params[PARAM_PITCH].getValue();

			inputGain = params[PARAM_IN_GAIN].getValue();
			outputGain = params[PARAM_OUT_GAIN].getValue();

			if (btLedsMode.process(params[PARAM_LEDS_MODE].getValue())) {
				ledMode = cloudyCommon::LedModes((ledMode + 1) % cloudyCommon::LED_MODES_LAST);
				lastLedMode = ledMode;

				paramQuantities[PARAM_LEDS_MODE]->name =
					string::f(cloudyCommon::kLedButtonTooltip, cloudyCommon::vuButtonLabels[ledMode]);

				bDisplaySwitched = lastFrozen[displayChannel];
			}

			int vuChannel = displayChannel << 1;
			dsp::Frame<2> lightFrame = {};
			switch (ledMode) {
			case cloudyCommon::LEDS_OUTPUT:
				lightFrame.samples[0] = outputFrames.samples[vuChannel];
				lightFrame.samples[1] = outputFrames.samples[vuChannel + 1];
				break;
			default:
				lightFrame.samples[0] = inputFrames.samples[vuChannel];
				lightFrame.samples[1] = inputFrames.samples[vuChannel + 1];
				break;
			}

			vuMeter.process(sampleTime, fmaxf(fabsf(lightFrame.samples[0]), fabsf(lightFrame.samples[1])));

			float lightBlend = vuMeter.getBrightness(-24.f, -18.f);
			float lightSpread = vuMeter.getBrightness(-18.f, -12.f);
			float lightBrightness = vuMeter.getBrightness(-12.f, -6.f);
			float lightReverb = vuMeter.getBrightness(-6.f, 0.f);

			lights[LIGHT_BLEND].setBrightness(lightBlend);
			lights[LIGHT_SPREAD].setBrightness(lightSpread);
			lights[LIGHT_FEEDBACK].setBrightness(lightBrightness);
			lights[LIGHT_REVERB].setBrightness(lightReverb);

			lights[LIGHT_FREEZE].setBrightnessSmooth(cloudsParameters[displayChannel]->freeze *
				kSanguineButtonLightValue, sampleTime);

			knobPlaybackMode = clouds::PlaybackMode(params[PARAM_MODE].getValue());
			clouds::PlaybackMode channelPlaybackMode = knobPlaybackMode;

			channelModes.fill(knobPlaybackMode);

			if (bHaveModeCable) {
				float_4 modeVoltages;
				for (int channel = 0; channel < channelCount; channel += 4) {
					modeVoltages = inputs[INPUT_MODE].getVoltageSimd<float_4>(channel);
					modeVoltages = simd::round(modeVoltages);
					modeVoltages = simd::clamp(modeVoltages, 0.f, 3.f);
					channelModes[channel] = static_cast<clouds::PlaybackMode>(modeVoltages[0]);
					channelModes[channel + 1] = static_cast<clouds::PlaybackMode>(modeVoltages[1]);
					channelModes[channel + 2] = static_cast<clouds::PlaybackMode>(modeVoltages[2]);
					channelModes[channel + 3] = static_cast<clouds::PlaybackMode>(modeVoltages[3]);
				}

				channelPlaybackMode = channelModes[displayChannel];
			}

			int currentLight;
			clouds::PlaybackMode currentChannelMode;
			bool bIsChannelActive;
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				currentLight = LIGHT_CHANNEL_1 + channel * 3;
				currentChannelMode = cloudsProcessors[channel]->playback_mode();
				bIsChannelActive = channel < channelCount;

				lights[currentLight].setBrightnessSmooth(bIsChannelActive &
					((currentChannelMode == clouds::PLAYBACK_MODE_STRETCH) |
						(currentChannelMode == clouds::PLAYBACK_MODE_LOOPING_DELAY)), sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(bIsChannelActive &
					((currentChannelMode == clouds::PLAYBACK_MODE_GRANULAR) |
						(currentChannelMode == clouds::PLAYBACK_MODE_STRETCH)), sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(bIsChannelActive &
					(currentChannelMode == clouds::PLAYBACK_MODE_SPECTRAL), sampleTime);
			}

			if (channelPlaybackMode != lastPlaybackMode) {
				textMode = nebulae::modeListDisplay[channelPlaybackMode];

#ifndef METAMODULE
				textFreeze = nebulae::modeDisplays[channelPlaybackMode].labelFreeze;
				textPosition = nebulae::modeDisplays[channelPlaybackMode].labelPosition;
				textDensity = nebulae::modeDisplays[channelPlaybackMode].labelDensity;
				textSize = nebulae::modeDisplays[channelPlaybackMode].labelSize;
				textTexture = nebulae::modeDisplays[channelPlaybackMode].labelTexture;
				textPitch = nebulae::modeDisplays[channelPlaybackMode].labelPitch;
				textTrigger = nebulae::modeDisplays[channelPlaybackMode].labelTrigger;
#endif

				paramQuantities[PARAM_FREEZE]->name = nebulae::modeTooltips[channelPlaybackMode].labelFreeze;
				inputInfos[INPUT_FREEZE]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelFreeze;

				paramQuantities[PARAM_POSITION]->name = nebulae::modeTooltips[channelPlaybackMode].labelPosition;
				inputInfos[INPUT_POSITION]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelPosition;

				paramQuantities[PARAM_DENSITY]->name = nebulae::modeTooltips[channelPlaybackMode].labelDensity;
				inputInfos[INPUT_DENSITY]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelDensity;

				paramQuantities[PARAM_SIZE]->name = nebulae::modeTooltips[channelPlaybackMode].labelSize;
				inputInfos[INPUT_SIZE]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelSize;

				paramQuantities[PARAM_TEXTURE]->name = nebulae::modeTooltips[channelPlaybackMode].labelTexture;
				inputInfos[INPUT_TEXTURE]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelTexture;

				paramQuantities[PARAM_PITCH]->name = nebulae::modeTooltips[channelPlaybackMode].labelPitch;
				inputInfos[INPUT_PITCH]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelPitch;

				inputInfos[INPUT_TRIGGER]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelTrigger;

				inputInfos[INPUT_BLEND]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelBlend;

				inputInfos[INPUT_SPREAD]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelSpread;

				inputInfos[INPUT_FEEDBACK]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelFeedback;

				inputInfos[INPUT_REVERB]->name = nebulae::modeInputTooltips[channelPlaybackMode].labelReverb;

				lastPlaybackMode = channelPlaybackMode;
			}

			voltages1[displayChannel] = simd::rescale(voltages1[displayChannel], 0.f, 5.f, 0.f, 1.f);

			lights[LIGHT_POSITION_CV].setBrightness(voltages1[displayChannel][0]);
			lights[LIGHT_POSITION_CV + 1].setBrightness(-voltages1[displayChannel][0]);

			lights[LIGHT_DENSITY_CV].setBrightness(voltages1[displayChannel][1]);
			lights[LIGHT_DENSITY_CV + 1].setBrightness(-voltages1[displayChannel][1]);

			lights[LIGHT_SIZE_CV].setBrightness(voltages1[displayChannel][2]);
			lights[LIGHT_SIZE_CV + 1].setBrightness(-voltages1[displayChannel][2]);

			lights[LIGHT_TEXTURE_CV].setBrightness(voltages1[displayChannel][3]);
			lights[LIGHT_TEXTURE_CV + 1].setBrightness(-voltages1[displayChannel][3]);

			lights[LIGHT_HI_FI].setBrightnessSmooth(params[PARAM_HI_FI].getValue() *
				kSanguineButtonLightValue, sampleTime);
			lights[LIGHT_STEREO].setBrightnessSmooth(params[PARAM_STEREO].getValue() *
				kSanguineButtonLightValue, sampleTime);
		} // lightsDivider
	}

	void onPortChange(const PortChangeEvent& e) override {
		switch (e.type) {
		case Port::INPUT:
			switch (e.portId) {
			case INPUT_MODE:
				bHaveModeCable = e.connecting;
				break;

			case INPUT_RIGHT:
				bRightInputConnected = e.connecting;
				break;

			default:
				break;
			}
			break;

		case Port::OUTPUT:
			switch (e.portId) {
			case OUTPUT_LEFT:
				bLeftOutputConnected = e.connecting;
				break;

			case OUTPUT_RIGHT:
				bRightOutputConnected = e.connecting;
				break;
			}
			break;
		}
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		srcInputs.setRates(static_cast<int>(e.sampleRate), cloudyCommon::kHardwareRate);
		srcOutputs.setRates(cloudyCommon::kHardwareRate, static_cast<int>(e.sampleRate));
	}

	void onAdd(const AddEvent& e) override {
		jitteredLightsFrequency = kLightsFrequency + (getId() % kLightsFrequency);
		lightsDivider.setDivision(jitteredLightsFrequency);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonInt(rootJ, "displayChannel", displayChannel);
		setJsonInt(rootJ, "LEDsMode", ledMode);
		setJsonBoolean(rootJ, "triggersAreGates", bTriggersAreGates);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		if (getJsonInt(rootJ, "displayChannel", intValue)) {
			displayChannel = intValue;
		}

		// TODO: fix tooltip!
		if (getJsonInt(rootJ, "LEDsMode", intValue)) {
			ledMode = static_cast<cloudyCommon::LedModes>(intValue);
		}

		getJsonBoolean(rootJ, "triggersAreGates", bTriggersAreGates);
	}

	int getModeParam() {
		return params[PARAM_MODE].getValue();
	}
	void setModeParam(int mode) {
		params[PARAM_MODE].setValue(mode);
	}
};

struct NebulaeWidget : SanguineModuleWidget {
	explicit NebulaeWidget(Nebulae* module) {
		setModule(module);

		moduleName = "nebulae";
		panelSize = sanguineThemes::SIZE_27;
		backplateColor = sanguineThemes::PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* nebulaeFramebuffer = new FramebufferWidget();
		addChild(nebulaeFramebuffer);

		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(68.374, 13.96), module, Nebulae::LIGHT_BLEND));
		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(79.578, 13.96), module, Nebulae::LIGHT_SPREAD));
		addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(90.781, 13.96), module, Nebulae::LIGHT_FEEDBACK));
		addChild(createLightCentered<MediumLight<RedLight>>(millimetersToPixelsVec(101.985, 13.96), module, Nebulae::LIGHT_REVERB));

		addParam(createParamCentered<TL1105>(millimetersToPixelsVec(111.383, 13.96), module, Nebulae::PARAM_LEDS_MODE));

		CKD6* freezeButton = createParamCentered<CKD6>(millimetersToPixelsVec(12.229, 18.268), module, Nebulae::PARAM_FREEZE);
		freezeButton->latch = true;
		freezeButton->momentary = false;
		addParam(freezeButton);
		addChild(createLightCentered<CKD6Light<BlueLight>>(millimetersToPixelsVec(12.229, 18.268), module, Nebulae::LIGHT_FREEZE));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(44.176, 26.927), module, Nebulae::INPUT_MODE));

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 85.18, 26.927);
		nebulaeFramebuffer->addChild(displayModel);
		displayModel->fallbackString = nebulae::modeListDisplay[0];

		addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(128.505, 26.927), module, Nebulae::PARAM_MODE));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(12.229, 35.987), module, Nebulae::INPUT_FREEZE));

		const float xDelta = 2.241f;

		float currentX = 68.374;

		for (int light = 0; light < PORT_MAX_CHANNELS; ++light) {
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 35.318), module,
				Nebulae::LIGHT_CHANNEL_1 + light * 3));
			currentX += xDelta;
		}

		addParam(createParamCentered<Sanguine1PGreen>(millimetersToPixelsVec(86.118, 44.869), module, Nebulae::PARAM_BLEND));

		addParam(createParamCentered<Sanguine1PRed>(millimetersToPixelsVec(105.638, 44.869), module, Nebulae::PARAM_PITCH));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(11.763, 57.169),
			module, Nebulae::PARAM_POSITION, Nebulae::LIGHT_POSITION_CV));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(29.722, 57.169),
			module, Nebulae::PARAM_DENSITY, Nebulae::LIGHT_DENSITY_CV));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(47.682, 57.169),
			module, Nebulae::PARAM_SIZE, Nebulae::LIGHT_SIZE_CV));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(65.644, 57.169),
			module, Nebulae::PARAM_TEXTURE, Nebulae::LIGHT_TEXTURE_CV));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(86.118, 63.587), module, Nebulae::INPUT_BLEND));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(105.638, 63.587), module, Nebulae::INPUT_PITCH));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(125.214, 63.587), module, Nebulae::INPUT_TRIGGER));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(86.118, 77.713), module, Nebulae::INPUT_SPREAD));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(105.638, 77.713), module, Nebulae::INPUT_FEEDBACK));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(125.214, 77.713), module, Nebulae::INPUT_REVERB));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(11.763, 83.772), module, Nebulae::INPUT_POSITION));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(29.722, 83.772), module, Nebulae::INPUT_DENSITY));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(47.682, 83.772), module, Nebulae::INPUT_SIZE));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(65.644, 83.772), module, Nebulae::INPUT_TEXTURE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(18.631, 96.427),
			module, Nebulae::PARAM_HI_FI, Nebulae::LIGHT_HI_FI));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(43.823, 96.427),
			module, Nebulae::PARAM_STEREO, Nebulae::LIGHT_STEREO));

		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(86.118, 96.427), module, Nebulae::PARAM_SPREAD));

		addParam(createParamCentered<Sanguine1PPurple>(millimetersToPixelsVec(105.638, 96.427), module, Nebulae::PARAM_FEEDBACK));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(125.214, 96.427), module, Nebulae::PARAM_REVERB));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.677, 116.972), module, Nebulae::INPUT_LEFT));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(21.529, 116.972), module, Nebulae::INPUT_RIGHT));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(33.873, 117.002), module, Nebulae::INPUT_IN_GAIN));

		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(46.434, 117.002), module, Nebulae::PARAM_IN_GAIN));

		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(90.701, 117.002), module, Nebulae::PARAM_OUT_GAIN));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(103.25, 117.002), module, Nebulae::INPUT_OUT_GAIN));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(115.161, 116.972), module, Nebulae::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(129.013, 116.972), module, Nebulae::OUTPUT_RIGHT));


#ifndef METAMODULE
		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 58.816, 110.16);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 71.817, 117.093);
		addChild(mutantsLogo);

		// OLED displays
		Sanguine96x32OLEDDisplay* displayFreeze = new Sanguine96x32OLEDDisplay(module, 12.229, 26.927);
		nebulaeFramebuffer->addChild(displayFreeze);
		displayFreeze->fallbackString = nebulae::modeDisplays[0].labelFreeze;

		Sanguine96x32OLEDDisplay* displayPitch = new Sanguine96x32OLEDDisplay(module, 105.638, 54.874);
		nebulaeFramebuffer->addChild(displayPitch);
		displayPitch->fallbackString = nebulae::modeDisplays[0].labelPitch;

		Sanguine96x32OLEDDisplay* displayTrigger = new Sanguine96x32OLEDDisplay(module, 125.214, 54.874);
		nebulaeFramebuffer->addChild(displayTrigger);
		displayTrigger->fallbackString = nebulae::modeDisplays[0].labelTrigger;

		Sanguine96x32OLEDDisplay* displayPosition = new Sanguine96x32OLEDDisplay(module, 11.763, 75.163);
		nebulaeFramebuffer->addChild(displayPosition);
		displayPosition->fallbackString = nebulae::modeDisplays[0].labelPosition;

		Sanguine96x32OLEDDisplay* displayDensity = new Sanguine96x32OLEDDisplay(module, 29.722, 75.163);
		nebulaeFramebuffer->addChild(displayDensity);
		displayDensity->fallbackString = nebulae::modeDisplays[0].labelDensity;

		Sanguine96x32OLEDDisplay* displaySize = new Sanguine96x32OLEDDisplay(module, 47.682, 75.163);
		nebulaeFramebuffer->addChild(displaySize);
		displaySize->fallbackString = nebulae::modeDisplays[0].labelSize;

		Sanguine96x32OLEDDisplay* displayTexture = new Sanguine96x32OLEDDisplay(module, 65.644, 75.163);
		nebulaeFramebuffer->addChild(displayTexture);
		displayTexture->fallbackString = nebulae::modeDisplays[0].labelTexture;
#endif

		if (module) {
			displayModel->values.displayText = &module->textMode;

#ifndef METAMODULE
			displayFreeze->oledText = &module->textFreeze;
			displayPosition->oledText = &module->textPosition;
			displayDensity->oledText = &module->textDensity;
			displaySize->oledText = &module->textSize;
			displayTexture->oledText = &module->textTexture;
			displayPitch->oledText = &module->textPitch;
			displayTrigger->oledText = &module->textTrigger;
#endif
		}
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Nebulae* module = dynamic_cast<Nebulae*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < static_cast<int>(nebulae::modeListDisplay.size()); ++i) {
			modelLabels.push_back(nebulae::modeListDisplay[i] + ": " + nebulae::modeListMenu[i]);
		}
		menu->addChild(createIndexSubmenuItem("Mode", modelLabels,
			[=]() {return module->getModeParam(); },
			[=](int i) {module->setModeParam(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Options", "",
			[=](Menu* menu) {
				menu->addChild(createBoolPtrMenuItem("Handle triggers as gates", "", &module->bTriggersAreGates));

				std::vector<std::string> availableChannels;
				for (int i = 0; i < module->channelCount; ++i) {
					availableChannels.push_back(channelNumbers[i]);
				}
				menu->addChild(createIndexSubmenuItem("Display channel", availableChannels,
					[=]() {return module->displayChannel; },
					[=](int i) {module->displayChannel = i; }
				));
			}
		));
	}
};

Model* modelNebulae = createModel<Nebulae, NebulaeWidget>("Sanguine-Nebulae");