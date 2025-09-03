#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"
#include "sanguinechannels.hpp"
#include "array"

#include "clouds_parasite/dsp/etesia_granular_processor.h"

#include "etesia.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

using simd::float_4;

struct Etesia : SanguineModule {
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
		PARAM_REVERSE,
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
		INPUT_REVERSE,
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
		LIGHT_REVERSE,
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

	std::string textMode = etesia::modeListDisplay[0];
#ifndef METAMODULE
	std::string textFreeze = etesia::modeDisplays[0].labelFreeze;
	std::string textPosition = etesia::modeDisplays[0].labelPosition;
	std::string textDensity = etesia::modeDisplays[0].labelDensity;
	std::string textSize = etesia::modeDisplays[0].labelSize;
	std::string textTexture = etesia::modeDisplays[0].labelTexture;
	std::string textPitch = etesia::modeDisplays[0].labelPitch;
	std::string textTrigger = etesia::modeDisplays[0].labelTrigger;
	std::string textBlend = etesia::modeDisplays[0].labelBlend;
	std::string textSpread = etesia::modeDisplays[0].labelSpread;
	std::string textFeedback = etesia::modeDisplays[0].labelFeedback;
	std::string textReverb = etesia::modeDisplays[0].labelReverb;
#endif

	dsp::SampleRateConverter<2> srcInputs[PORT_MAX_CHANNELS];
	dsp::SampleRateConverter<2> srcOutputs[PORT_MAX_CHANNELS];
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> drbInputBuffers[PORT_MAX_CHANNELS];
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> drbOutputBuffers[PORT_MAX_CHANNELS];
	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider lightsDivider;
	dsp::BooleanTrigger btLedsMode;

	etesia::PlaybackMode knobPlaybackMode = etesia::PLAYBACK_MODE_GRANULAR;
	etesia::PlaybackMode lastPlaybackMode = etesia::PLAYBACK_MODE_LAST;

	std::array<etesia::PlaybackMode, PORT_MAX_CHANNELS> channelModes;

	float freezeLight = 0.f;

	float_4 voltages1[PORT_MAX_CHANNELS];

	int channelCount;
	int displayChannel = 0;

	static const int kLightsFrequency = 64;

	bool lastFrozen[PORT_MAX_CHANNELS];
	bool bDisplaySwitched = false;
	bool bTriggersAreGates = true;
	bool lastTriggered[PORT_MAX_CHANNELS];

	bool bHaveModeCable = false;
	bool bRightInputConnected = false;
	bool bLeftOutputConnected = false;
	bool bRightOutputConnected = false;

	uint8_t* buffersLarge[PORT_MAX_CHANNELS];
	uint8_t* buffersSmall[PORT_MAX_CHANNELS];

	etesia::EtesiaGranularProcessor* etesiaProcessors[PORT_MAX_CHANNELS];

	Etesia() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configInput(INPUT_FREEZE, "Freeze");
		configSwitch(PARAM_FREEZE, 0.f, 1.f, 0.f, "Freeze", cloudyCommon::freezeButtonLabels);

		configInput(INPUT_REVERSE, "Reverse");
		configSwitch(PARAM_REVERSE, 0.f, 1.f, 0.f, "Reverse", cloudyCommon::freezeButtonLabels);

		configButton(PARAM_LEDS_MODE, "LED display value: Input");

		configSwitch(PARAM_MODE, 0.f, 5.f, 0.f, "Mode", etesia::modeListMenu);

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
			etesiaProcessors[channel] = new etesia::EtesiaGranularProcessor();
			memset(etesiaProcessors[channel], 0, sizeof(etesiaProcessors));
			etesiaProcessors[channel]->Init(buffersLarge[channel], cloudyCommon::kBigBufferLength,
				buffersSmall[channel], cloudyCommon::kSmallBufferLength);
		}

		channelModes.fill(etesia::PLAYBACK_MODE_GRANULAR);

		lightsDivider.setDivision(kLightsFrequency);
	}

	~Etesia() {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			delete etesiaProcessors[channel];
			delete[] buffersLarge[channel];
			delete[] buffersSmall[channel];
		}
	}

	void process(const ProcessArgs& args) override {
		int stereoChannels = static_cast<bool>(params[PARAM_STEREO].getValue()) ? 2 : 1;
		bool bWantLoFi = !static_cast<bool>(params[PARAM_HI_FI].getValue());
#ifndef METAMODULE
		bool bFrozen = static_cast<bool>(params[PARAM_FREEZE].getValue());
		bool bReversed = static_cast<bool>(params[PARAM_REVERSE].getValue());
#else
		bool bFrozen = static_cast<bool>(std::round(params[PARAM_FREEZE].getValue()));
		bool bReversed = static_cast<bool>(std::round(params[PARAM_REVERSE].getValue()));
#endif

		float_4 knobValues;
		float_4 sliderValues;

		knobValues[0] = params[PARAM_BLEND].getValue();
		knobValues[1] = params[PARAM_SPREAD].getValue();
		knobValues[2] = params[PARAM_FEEDBACK].getValue();
		knobValues[3] = params[PARAM_REVERB].getValue();

		sliderValues[0] = params[PARAM_POSITION].getValue();
		sliderValues[1] = params[PARAM_DENSITY].getValue();
		sliderValues[2] = params[PARAM_SIZE].getValue();
		sliderValues[3] = params[PARAM_TEXTURE].getValue();

		float paramPitch = params[PARAM_PITCH].getValue();

		float inputGain = params[PARAM_IN_GAIN].getValue();
		float outputGain = params[PARAM_OUT_GAIN].getValue();

		dsp::Frame<2> inputFrames[PORT_MAX_CHANNELS];
		dsp::Frame<2> outputFrames[PORT_MAX_CHANNELS] = {};

		channelCount = std::max(std::max(inputs[INPUT_LEFT].getChannels(), inputs[INPUT_RIGHT].getChannels()), 1);

		if (displayChannel >= channelCount) {
			displayChannel = channelCount - 1;
		}

		etesia::Parameters* etesiaParameters[PORT_MAX_CHANNELS];

		for (int channel = 0; channel < channelCount; ++channel) {
			// Get input.
			if (!drbInputBuffers[channel].full()) {
				float finalInputGain = clamp(inputGain + (inputs[INPUT_IN_GAIN].getVoltage(channel) / 5.f), 0.f, 1.f);
				inputFrames[channel].samples[0] = inputs[INPUT_LEFT].getVoltage(channel) * finalInputGain / 5.f;
				inputFrames[channel].samples[1] = bRightInputConnected ? inputs[INPUT_RIGHT].getVoltage(channel)
					* finalInputGain / 5.f : inputFrames[channel].samples[0];
				drbInputBuffers[channel].push(inputFrames[channel]);
			}

			etesiaParameters[channel] = etesiaProcessors[channel]->mutable_parameters();

			voltages1[channel][0] = inputs[INPUT_POSITION].getVoltage(channel);
			voltages1[channel][1] = inputs[INPUT_DENSITY].getVoltage(channel);
			voltages1[channel][2] = inputs[INPUT_SIZE].getVoltage(channel);
			voltages1[channel][3] = inputs[INPUT_TEXTURE].getVoltage(channel);

			// Render frames.
			if (drbOutputBuffers[channel].empty()) {
				etesia::ShortFrame input[cloudyCommon::kMaxFrames] = {};

				// Convert input buffer.
				srcInputs[channel].setRates(args.sampleRate, 32000);
				dsp::Frame<2> inputFrames[cloudyCommon::kMaxFrames];
				int inputLength = drbInputBuffers[channel].size();
				int outputLength = cloudyCommon::kMaxFrames;
				srcInputs[channel].process(drbInputBuffers[channel].startData(), &inputLength,
					inputFrames, &outputLength);
				drbInputBuffers[channel].startIncr(inputLength);

				/*
				   We might not fill all of the input buffer if there is a deficiency, but this cannot be avoided due to imprecisions
				   between the input and output SRC.
				*/
				for (int frame = 0; frame < outputLength; ++frame) {
					input[frame].l = clamp(inputFrames[frame].samples[0] * 32767.0, -32768, 32767);
					input[frame].r = clamp(inputFrames[frame].samples[1] * 32767.0, -32768, 32767);
				}

				// Set up Etesia processor.
				etesiaProcessors[channel]->set_playback_mode(channelModes[channel]);

				etesiaProcessors[channel]->set_num_channels(stereoChannels);
				etesiaProcessors[channel]->set_low_fidelity(bWantLoFi);
				etesiaProcessors[channel]->Prepare();

				float_4 scaledVoltages;

				scaledVoltages[0] = inputs[INPUT_BLEND].getVoltage(channel);
				scaledVoltages[1] = inputs[INPUT_SPREAD].getVoltage(channel);
				scaledVoltages[2] = inputs[INPUT_FEEDBACK].getVoltage(channel);
				scaledVoltages[3] = inputs[INPUT_REVERB].getVoltage(channel);

				scaledVoltages /= 5.f;

				scaledVoltages += knobValues;

				scaledVoltages = clamp(scaledVoltages, 0.f, 1.f);

				etesiaParameters[channel]->dry_wet = scaledVoltages[0];
				etesiaParameters[channel]->stereo_spread = scaledVoltages[1];
				etesiaParameters[channel]->feedback = scaledVoltages[2];
				etesiaParameters[channel]->reverb = scaledVoltages[3];

				scaledVoltages = voltages1[channel];
				scaledVoltages /= 5.f;

				scaledVoltages += sliderValues;

				scaledVoltages = clamp(scaledVoltages, 0.f, 1.f);

				etesiaParameters[channel]->position = scaledVoltages[0];
				etesiaParameters[channel]->density = scaledVoltages[1];
				etesiaParameters[channel]->size = scaledVoltages[2];
				etesiaParameters[channel]->texture = scaledVoltages[3];

				// Trigger.
				bool bIsGate = inputs[INPUT_TRIGGER].getVoltage(channel) >= 1.f;

				etesiaParameters[channel]->trigger = (bTriggersAreGates & bIsGate) |
					((!bTriggersAreGates) & (bIsGate & (!lastTriggered[channel])));
				etesiaParameters[channel]->gate = bIsGate;

				lastTriggered[channel] = bIsGate;

				etesiaParameters[channel]->freeze = ((inputs[INPUT_FREEZE].getVoltage(channel) >= 1.f) | bFrozen);
				etesiaParameters[channel]->granular.reverse = ((inputs[INPUT_REVERSE].getVoltage(channel) >= 1.f) | bReversed);

				etesiaParameters[channel]->pitch = clamp((paramPitch + inputs[INPUT_PITCH].getVoltage(channel)) * 12.f, -48.f, 48.f);

				etesia::ShortFrame output[cloudyCommon::kMaxFrames];
				etesiaProcessors[channel]->Process(input, output, cloudyCommon::kMaxFrames);

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

				// Convert output buffer.
				dsp::Frame<2> outputFrames[cloudyCommon::kMaxFrames];
				for (int frame = 0; frame < cloudyCommon::kMaxFrames; ++frame) {
					outputFrames[frame].samples[0] = output[frame].l / 32768.f;
					outputFrames[frame].samples[1] = output[frame].r / 32768.f;
				}

				srcOutputs[channel].setRates(32000, args.sampleRate);
				int inCount = cloudyCommon::kMaxFrames;
				int outCount = drbOutputBuffers[channel].capacity();
				srcOutputs[channel].process(outputFrames, &inCount, drbOutputBuffers[channel].endData(), &outCount);
				drbOutputBuffers[channel].endIncr(outCount);
			}

			// Set output.
			if (!drbOutputBuffers[channel].empty()) {
				outputFrames[channel] = drbOutputBuffers[channel].shift();
				float finalOutputGain = clamp(outputGain + (inputs[INPUT_OUT_GAIN].getVoltage(channel) / 5.f), 0.f, 2.f);
				if (bLeftOutputConnected) {
					outputFrames[channel].samples[0] *= finalOutputGain;
					outputs[OUTPUT_LEFT].setVoltage(5.f * outputFrames[channel].samples[0], channel);
				}
				if (bRightOutputConnected) {
					outputFrames[channel].samples[1] *= finalOutputGain;
					outputs[OUTPUT_RIGHT].setVoltage(5.f * outputFrames[channel].samples[1], channel);
				}
			}
		}

		outputs[OUTPUT_LEFT].setChannels(channelCount);
		outputs[OUTPUT_RIGHT].setChannels(channelCount);

		// Lights.
		if (lightsDivider.process()) { // Expensive, so call this infrequently!
			const float sampleTime = args.sampleTime * kLightsFrequency;

			if (btLedsMode.process(params[PARAM_LEDS_MODE].getValue())) {
				ledMode = cloudyCommon::LedModes((ledMode + 1) % cloudyCommon::LED_MODES_LAST);
				lastLedMode = ledMode;

				paramQuantities[PARAM_LEDS_MODE]->name = cloudyCommon::kLedButtonPrefix +
					cloudyCommon::vuButtonLabels[ledMode];

				bDisplaySwitched = lastFrozen[displayChannel];
			}

			dsp::Frame<2> lightFrame = {};

			switch (ledMode) {
			case cloudyCommon::LEDS_OUTPUT:
				lightFrame = outputFrames[displayChannel];
				break;
			default:
				lightFrame = inputFrames[displayChannel];
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

			lights[LIGHT_FREEZE].setBrightnessSmooth(etesiaParameters[displayChannel]->freeze *
				kSanguineButtonLightValue, sampleTime);

			lights[LIGHT_REVERSE].setBrightnessSmooth(etesiaParameters[displayChannel]->granular.reverse *
				kSanguineButtonLightValue, sampleTime);

			knobPlaybackMode = etesia::PlaybackMode(params[PARAM_MODE].getValue());
			etesia::PlaybackMode channelPlaybackMode = knobPlaybackMode;

			channelModes.fill(knobPlaybackMode);

			if (bHaveModeCable) {
				float_4 modeVoltages;
				for (int channel = 0; channel < channelCount; channel += 4) {
					modeVoltages = inputs[INPUT_MODE].getVoltageSimd<float_4>(channel);
					modeVoltages = simd::round(modeVoltages);
					modeVoltages = simd::clamp(modeVoltages, 0.f, 5.f);
					channelModes[channel] = static_cast<etesia::PlaybackMode>(modeVoltages[0]);
					channelModes[channel + 1] = static_cast<etesia::PlaybackMode>(modeVoltages[1]);
					channelModes[channel + 2] = static_cast<etesia::PlaybackMode>(modeVoltages[2]);
					channelModes[channel + 3] = static_cast<etesia::PlaybackMode>(modeVoltages[3]);
				}

				channelPlaybackMode = channelModes[displayChannel];
			}

			int currentLight;
			etesia::PlaybackMode currentChannelMode;
			bool bIsChannelActive;
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				currentLight = LIGHT_CHANNEL_1 + channel * 3;
				currentChannelMode = etesiaProcessors[channel]->playback_mode();
				bIsChannelActive = channel < channelCount;

				lights[currentLight].setBrightnessSmooth(bIsChannelActive &
					((currentChannelMode == etesia::PLAYBACK_MODE_STRETCH) |
						(currentChannelMode == etesia::PLAYBACK_MODE_LOOPING_DELAY) |
						(currentChannelMode == etesia::PLAYBACK_MODE_OLIVERB)), sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(bIsChannelActive &
					((currentChannelMode == etesia::PLAYBACK_MODE_GRANULAR) |
						(currentChannelMode == etesia::PLAYBACK_MODE_STRETCH) |
						(currentChannelMode == etesia::PLAYBACK_MODE_RESONESTOR)), sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(bIsChannelActive &
					((currentChannelMode == etesia::PLAYBACK_MODE_SPECTRAL) |
						(currentChannelMode == etesia::PLAYBACK_MODE_OLIVERB) |
						(currentChannelMode == etesia::PLAYBACK_MODE_RESONESTOR)), sampleTime);
			}

			if (channelPlaybackMode != lastPlaybackMode) {
				textMode = etesia::modeListDisplay[channelPlaybackMode];

#ifndef METAMODULE
				textFreeze = etesia::modeDisplays[channelPlaybackMode].labelFreeze;
				textPosition = etesia::modeDisplays[channelPlaybackMode].labelPosition;
				textDensity = etesia::modeDisplays[channelPlaybackMode].labelDensity;
				textSize = etesia::modeDisplays[channelPlaybackMode].labelSize;
				textTexture = etesia::modeDisplays[channelPlaybackMode].labelTexture;
				textPitch = etesia::modeDisplays[channelPlaybackMode].labelPitch;
				textTrigger = etesia::modeDisplays[channelPlaybackMode].labelTrigger;
				// Parasite.
				textBlend = etesia::modeDisplays[channelPlaybackMode].labelBlend;
				textSpread = etesia::modeDisplays[channelPlaybackMode].labelSpread;
				textFeedback = etesia::modeDisplays[channelPlaybackMode].labelFeedback;
				textReverb = etesia::modeDisplays[channelPlaybackMode].labelReverb;
#endif

				paramQuantities[PARAM_FREEZE]->name = etesia::modeTooltips[channelPlaybackMode].labelFreeze;
				inputInfos[INPUT_FREEZE]->name = etesia::modeInputTooltips[channelPlaybackMode].labelFreeze;

				paramQuantities[PARAM_POSITION]->name = etesia::modeTooltips[channelPlaybackMode].labelPosition;
				inputInfos[INPUT_POSITION]->name = etesia::modeInputTooltips[channelPlaybackMode].labelPosition;

				paramQuantities[PARAM_DENSITY]->name = etesia::modeTooltips[channelPlaybackMode].labelDensity;
				inputInfos[INPUT_DENSITY]->name = etesia::modeInputTooltips[channelPlaybackMode].labelDensity;

				paramQuantities[PARAM_SIZE]->name = etesia::modeTooltips[channelPlaybackMode].labelSize;
				inputInfos[INPUT_SIZE]->name = etesia::modeInputTooltips[channelPlaybackMode].labelSize;

				paramQuantities[PARAM_TEXTURE]->name = etesia::modeTooltips[channelPlaybackMode].labelTexture;
				inputInfos[INPUT_TEXTURE]->name = etesia::modeInputTooltips[channelPlaybackMode].labelTexture;

				paramQuantities[PARAM_PITCH]->name = etesia::modeTooltips[channelPlaybackMode].labelPitch;
				inputInfos[INPUT_PITCH]->name = etesia::modeInputTooltips[channelPlaybackMode].labelPitch;

				inputInfos[INPUT_TRIGGER]->name = etesia::modeInputTooltips[channelPlaybackMode].labelTrigger;

				// Parasite.
				paramQuantities[PARAM_BLEND]->name = etesia::modeTooltips[channelPlaybackMode].labelBlend;
				inputInfos[INPUT_BLEND]->name = etesia::modeInputTooltips[channelPlaybackMode].labelBlend;

				paramQuantities[PARAM_SPREAD]->name = etesia::modeTooltips[channelPlaybackMode].labelSpread;
				inputInfos[INPUT_SPREAD]->name = etesia::modeInputTooltips[channelPlaybackMode].labelSpread;

				paramQuantities[PARAM_FEEDBACK]->name = etesia::modeTooltips[channelPlaybackMode].labelFeedback;
				inputInfos[INPUT_FEEDBACK]->name = etesia::modeInputTooltips[channelPlaybackMode].labelFeedback;

				paramQuantities[PARAM_REVERB]->name = etesia::modeTooltips[channelPlaybackMode].labelReverb;
				inputInfos[INPUT_REVERB]->name = etesia::modeInputTooltips[channelPlaybackMode].labelReverb;

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

struct EtesiaWidget : SanguineModuleWidget {
	explicit EtesiaWidget(Etesia* module) {
		setModule(module);

		moduleName = "etesia";
		panelSize = sanguineThemes::SIZE_27;
		backplateColor = sanguineThemes::PLATE_RED;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* etesiaFramebuffer = new FramebufferWidget();
		addChild(etesiaFramebuffer);

		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(68.374, 13.96), module, Etesia::LIGHT_BLEND));
		addChild(createLightCentered<MediumLight<GreenLight>>(millimetersToPixelsVec(79.578, 13.96), module, Etesia::LIGHT_SPREAD));
		addChild(createLightCentered<MediumLight<YellowLight>>(millimetersToPixelsVec(90.781, 13.96), module, Etesia::LIGHT_FEEDBACK));
		addChild(createLightCentered<MediumLight<RedLight>>(millimetersToPixelsVec(101.985, 13.96), module, Etesia::LIGHT_REVERB));

		addParam(createParamCentered<TL1105>(millimetersToPixelsVec(111.383, 13.96), module, Etesia::PARAM_LEDS_MODE));

		CKD6* freezeButton = createParamCentered<CKD6>(millimetersToPixelsVec(12.229, 18.268), module, Etesia::PARAM_FREEZE);
		freezeButton->latch = true;
		freezeButton->momentary = false;
		addParam(freezeButton);
		addChild(createLightCentered<CKD6Light<BlueLight>>(millimetersToPixelsVec(12.229, 18.268), module, Etesia::LIGHT_FREEZE));

		CKD6* reverseButton = createParamCentered<CKD6>(millimetersToPixelsVec(29.904, 18.268), module, Etesia::PARAM_REVERSE);
		reverseButton->latch = true;
		reverseButton->momentary = false;
		addParam(reverseButton);
		addChild(createLightCentered<CKD6Light<WhiteLight>>(millimetersToPixelsVec(29.904, 18.268), module, Etesia::LIGHT_REVERSE));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(44.176, 26.927), module, Etesia::INPUT_MODE));

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 85.18, 26.927);
		etesiaFramebuffer->addChild(displayModel);
		displayModel->fallbackString = etesia::modeListDisplay[0];

		addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(128.505, 26.927), module, Etesia::PARAM_MODE));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(12.229, 35.987), module, Etesia::INPUT_FREEZE));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(29.904, 35.987), module, Etesia::INPUT_REVERSE));

		const float xDelta = 2.241f;

		float currentX = 68.374;

		for (int light = 0; light < PORT_MAX_CHANNELS; ++light) {
			addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 35.318), module,
				Etesia::LIGHT_CHANNEL_1 + light * 3));
			currentX += xDelta;
		}

		addParam(createParamCentered<Sanguine1PGreen>(millimetersToPixelsVec(86.118, 44.869), module, Etesia::PARAM_BLEND));

		addParam(createParamCentered<Sanguine1PRed>(millimetersToPixelsVec(105.638, 44.869), module, Etesia::PARAM_PITCH));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(11.763, 57.169),
			module, Etesia::PARAM_POSITION, Etesia::LIGHT_POSITION_CV));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(29.722, 57.169),
			module, Etesia::PARAM_DENSITY, Etesia::LIGHT_DENSITY_CV));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(47.682, 57.169),
			module, Etesia::PARAM_SIZE, Etesia::LIGHT_SIZE_CV));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(65.644, 57.169),
			module, Etesia::PARAM_TEXTURE, Etesia::LIGHT_TEXTURE_CV));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(86.118, 63.587), module, Etesia::INPUT_BLEND));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(105.638, 63.587), module, Etesia::INPUT_PITCH));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(125.214, 63.587), module, Etesia::INPUT_TRIGGER));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(86.118, 77.713), module, Etesia::INPUT_SPREAD));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(105.638, 77.713), module, Etesia::INPUT_FEEDBACK));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(125.214, 77.713), module, Etesia::INPUT_REVERB));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(11.763, 83.772), module, Etesia::INPUT_POSITION));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(29.722, 83.772), module, Etesia::INPUT_DENSITY));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(47.682, 83.772), module, Etesia::INPUT_SIZE));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(65.644, 83.772), module, Etesia::INPUT_TEXTURE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(18.631, 96.427),
			module, Etesia::PARAM_HI_FI, Etesia::LIGHT_HI_FI));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(43.823, 96.427),
			module, Etesia::PARAM_STEREO, Etesia::LIGHT_STEREO));

		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(86.118, 96.427), module, Etesia::PARAM_SPREAD));

		addParam(createParamCentered<Sanguine1PPurple>(millimetersToPixelsVec(105.638, 96.427), module, Etesia::PARAM_FEEDBACK));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(125.214, 96.427), module, Etesia::PARAM_REVERB));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.677, 116.972), module, Etesia::INPUT_LEFT));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(21.529, 116.972), module, Etesia::INPUT_RIGHT));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(33.873, 117.002), module, Etesia::INPUT_IN_GAIN));

		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(46.434, 117.002), module, Etesia::PARAM_IN_GAIN));

		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(90.701, 117.002), module, Etesia::PARAM_OUT_GAIN));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(103.25, 117.002), module, Etesia::INPUT_OUT_GAIN));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(115.161, 116.972), module, Etesia::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(129.013, 116.972), module, Etesia::OUTPUT_RIGHT));


#ifndef METAMODULE
		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 58.816, 110.16);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 71.817, 117.093);
		addChild(mutantsLogo);

		// OLED displays
		Sanguine96x32OLEDDisplay* displayFreeze = new Sanguine96x32OLEDDisplay(module, 12.229, 26.927);
		etesiaFramebuffer->addChild(displayFreeze);
		displayFreeze->fallbackString = etesia::modeDisplays[0].labelFreeze;

		Sanguine96x32OLEDDisplay* displayBlend = new Sanguine96x32OLEDDisplay(module, 86.118, 54.874);
		etesiaFramebuffer->addChild(displayBlend);
		displayBlend->fallbackString = etesia::modeDisplays[0].labelBlend;

		Sanguine96x32OLEDDisplay* displayPitch = new Sanguine96x32OLEDDisplay(module, 105.638, 54.874);
		etesiaFramebuffer->addChild(displayPitch);
		displayPitch->fallbackString = etesia::modeDisplays[0].labelPitch;

		Sanguine96x32OLEDDisplay* displayTrigger = new Sanguine96x32OLEDDisplay(module, 125.214, 54.874);
		etesiaFramebuffer->addChild(displayTrigger);
		displayTrigger->fallbackString = etesia::modeDisplays[0].labelTrigger;

		Sanguine96x32OLEDDisplay* displayPosition = new Sanguine96x32OLEDDisplay(module, 11.763, 75.163);
		etesiaFramebuffer->addChild(displayPosition);
		displayPosition->fallbackString = etesia::modeDisplays[0].labelPosition;

		Sanguine96x32OLEDDisplay* displayDensity = new Sanguine96x32OLEDDisplay(module, 29.722, 75.163);
		etesiaFramebuffer->addChild(displayDensity);
		displayDensity->fallbackString = etesia::modeDisplays[0].labelDensity;

		Sanguine96x32OLEDDisplay* displaySize = new Sanguine96x32OLEDDisplay(module, 47.682, 75.163);
		etesiaFramebuffer->addChild(displaySize);
		displaySize->fallbackString = etesia::modeDisplays[0].labelSize;

		Sanguine96x32OLEDDisplay* displayTexture = new Sanguine96x32OLEDDisplay(module, 65.644, 75.163);
		etesiaFramebuffer->addChild(displayTexture);
		displayTexture->fallbackString = etesia::modeDisplays[0].labelTexture;

		Sanguine96x32OLEDDisplay* displaySpread = new Sanguine96x32OLEDDisplay(module, 86.118, 86.409);
		etesiaFramebuffer->addChild(displaySpread);
		displaySpread->fallbackString = etesia::modeDisplays[0].labelSpread;

		Sanguine96x32OLEDDisplay* displayFeedback = new Sanguine96x32OLEDDisplay(module, 105.638, 86.409);
		etesiaFramebuffer->addChild(displayFeedback);
		displayFeedback->fallbackString = etesia::modeDisplays[0].labelFeedback;

		Sanguine96x32OLEDDisplay* displayReverb = new Sanguine96x32OLEDDisplay(module, 125.214, 86.409);
		etesiaFramebuffer->addChild(displayReverb);
		displayReverb->fallbackString = etesia::modeDisplays[0].labelReverb;
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
			displayBlend->oledText = &module->textBlend;
			displaySpread->oledText = &module->textSpread;
			displayFeedback->oledText = &module->textFeedback;
			displayReverb->oledText = &module->textReverb;
#endif
		}
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Etesia* module = dynamic_cast<Etesia*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < static_cast<int>(etesia::modeListDisplay.size()); ++i) {
			modelLabels.push_back(etesia::modeListDisplay[i] + ": " + etesia::modeListMenu[i]);
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

Model* modelEtesia = createModel<Etesia, EtesiaWidget>("Sanguine-Etesia");