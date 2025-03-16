#include <string.h>

#include "plugin.hpp"
#include "nodicommon.hpp"

#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "braids/envelope.h"
#include "braids/quantizer.h"
#include "braids/quantizer_scales.h"

#include "sanguinehelpers.hpp"

#include "sanguinechannels.hpp"

#include "nodi.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Nodi : SanguineModule {
	enum ParamIds {
		PARAM_FINE,
		PARAM_COARSE,
		PARAM_FM,
		PARAM_TIMBRE,
		PARAM_MODULATION,
		PARAM_COLOR,
		PARAM_MODEL,

		PARAM_PITCH_RANGE,
		PARAM_PITCH_OCTAVE,
		PARAM_TRIGGER_DELAY,
		PARAM_ATTACK,
		PARAM_DECAY,
		PARAM_AD_TIMBRE,
		PARAM_AD_MODULATION,
		PARAM_AD_COLOR,
		PARAM_RATE,
		PARAM_BITS,
		PARAM_SCALE,
		PARAM_ROOT,

		// Unused; but kept for compatibility with existing patches. ----->
		PARAM_META,
		// <-----

		PARAM_MORSE,
		PARAM_VCA,
		PARAM_DRIFT,
		PARAM_FLAT,
		PARAM_SIGN,
		PARAM_AUTO,

		PARAMS_COUNT
	};

	enum InputIds {
		INPUT_TRIGGER,
		INPUT_PITCH,
		INPUT_FM,
		INPUT_TIMBRE,
		INPUT_COLOR,
		INPUT_META,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_OUT,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_MODEL, 1 * 3),
		LIGHT_MORSE,
		LIGHT_VCA,
		LIGHT_FLAT,
		LIGHT_AUTO,
		ENUMS(LIGHT_CHANNEL_MODEL, 16 * 3),
		LIGHTS_COUNT
	};

	braids::MacroOscillator osc[PORT_MAX_CHANNELS];
	braids::SettingsData settings[PORT_MAX_CHANNELS];
	braids::VcoJitterSource jitterSource[PORT_MAX_CHANNELS];
	braids::SignatureWaveshaper waveShaper[PORT_MAX_CHANNELS];
	braids::Envelope envelope[PORT_MAX_CHANNELS];
	braids::Quantizer quantizer[PORT_MAX_CHANNELS];

	uint8_t currentScale[PORT_MAX_CHANNELS] = {};

	uint8_t waveShaperValue = 0;
	uint8_t driftValue = 0;

	int16_t previousPitch[PORT_MAX_CHANNELS] = {};

	uint16_t gainLp[PORT_MAX_CHANNELS] = {};
	uint16_t triggerDelay[PORT_MAX_CHANNELS] = {};

	int channelCount = 0;
	int displayChannel = 0;

	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> drbOutputBuffer[PORT_MAX_CHANNELS];
	dsp::SampleRateConverter<1> sampleRateConverter[PORT_MAX_CHANNELS];

	static const int kClockUpdateFrequency = 16;
	dsp::ClockDivider clockDivider;

	bool bFlagTriggerDetected[PORT_MAX_CHANNELS] = {};
	bool bLastTrig[PORT_MAX_CHANNELS] = {};
	bool bTriggerFlag[PORT_MAX_CHANNELS] = {};

	bool bAutoTrigger = false;
	bool bFlattenEnabled = false;
	bool bPaques = false;
	bool bVCAEnabled = false;

	bool bLowCpu = false;

	bool bPerInstanceSignSeed = true;
	bool bNeedSeed = true;

	// Display stuff
	braids::SettingsData lastSettings = {};
	braids::Setting lastSettingChanged = braids::SETTING_OSCILLATOR_SHAPE;

	uint32_t displayTimeout = 0;
	uint32_t userSignSeed = 0;

	std::string displayText = "";

	Nodi() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODEL, 0.f, braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META, 0.f, "Model", nodi::menuLabels);
		configParam(PARAM_MODULATION, -1.f, 1.f, 0.f, "Modulation");
		configParam(PARAM_COARSE, -5.f, 3.f, -1.f, "Coarse frequency", " semitones", 0.f, 12.f, 12.f);
		configParam(PARAM_FINE, -1.f, 1.f, 0.f, "Fine frequency", " semitones");
		configParam(PARAM_ATTACK, 0.f, 15.f, 0.f, "Attack");
		paramQuantities[PARAM_ATTACK]->snapEnabled = true;

		configParam(PARAM_AD_TIMBRE, 0.f, 15.f, 0.f, "Timbre AD");
		paramQuantities[PARAM_AD_TIMBRE]->snapEnabled = true;

		configParam(PARAM_TIMBRE, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);
		configSwitch(PARAM_ROOT, 0.f, 11.f, 0.f, "Quantizer root note", nodiCommon::noteStrings);
		configSwitch(PARAM_SCALE, 0.f, 48.f, 0.f, "Quantizer scale", nodiCommon::scaleStrings);
		configParam(PARAM_DECAY, 0.f, 15.f, 7.f, "Decay");
		paramQuantities[PARAM_DECAY]->snapEnabled = true;

		configParam(PARAM_AD_COLOR, 0.f, 15.f, 0.f, "Color AD");
		paramQuantities[PARAM_AD_COLOR]->snapEnabled = true;
		configParam(PARAM_AD_MODULATION, 0.f, 15.f, 0.f, "FM AD");
		paramQuantities[PARAM_AD_MODULATION]->snapEnabled = true;

		configParam(PARAM_COLOR, 0.f, 1.f, 0.5f, "Color", "%", 0.f, 100.f);
		configSwitch(PARAM_PITCH_OCTAVE, 0.f, 4.f, 2.f, "Octave", nodiCommon::octaveStrings);
		configSwitch(PARAM_PITCH_RANGE, 0.f, 4.f, 0.f, "Pitch range", nodiCommon::pitchRangeStrings);
		configParam(PARAM_FM, -1.f, 1.f, 0.f, "FM");

		configSwitch(PARAM_TRIGGER_DELAY, 0.f, 6.f, 0.f, "Trigger delay", nodiCommon::triggerDelayStrings);

		configSwitch(PARAM_BITS, 0.f, 6.f, 6.f, "Bit crusher resolution", nodiCommon::bitsStrings);
		configSwitch(PARAM_RATE, 0.f, 6.0f, 6.0f, "Bit crusher sample rate", nodiCommon::ratesStrings);

		configInput(INPUT_TRIGGER, "Trigger");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");
		configInput(INPUT_FM, "FM");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_COLOR, "Color");
		configOutput(OUTPUT_OUT, "Audio");

		configInput(INPUT_META, "Meta modulation");
		// Unused: kept for compatibility.
		configButton(PARAM_META, "");
		configButton(PARAM_MORSE, "Toggle paques (morse code secret message)");
		configButton(PARAM_VCA, "Toggle AD VCA");
		configSwitch(PARAM_DRIFT, 0.f, 4.f, 0.f, "Oscillator drift", nodiCommon::intensityTooltipStrings);
		configButton(PARAM_FLAT, "Toggle lower and higher frequency detuning");
		configSwitch(PARAM_SIGN, 0.f, 4.f, 0.f, "Signature wave shaper", nodiCommon::intensityTooltipStrings);
		configButton(PARAM_AUTO, "Toggle auto trigger");

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&osc[channel], 0, sizeof(braids::MacroOscillator));
			memset(&quantizer[channel], 0, sizeof(braids::Quantizer));
			memset(&envelope[channel], 0, sizeof(braids::Envelope));
			memset(&jitterSource[channel], 0, sizeof(braids::VcoJitterSource));
			memset(&waveShaper[channel], 0, sizeof(braids::SignatureWaveshaper));
			memset(&settings[channel], 0, sizeof(braids::SettingsData));

			osc[channel].Init();
			quantizer[channel].Init();
			envelope[channel].Init();

			jitterSource[channel].Init();
			waveShaper[channel].Init(userSignSeed);

			bLastTrig[channel] = false;

			currentScale[channel] = 0xff;

			previousPitch[channel] = 0;
		}
		memset(&lastSettings, 0, sizeof(braids::SettingsData));

		clockDivider.setDivision(kClockUpdateFrequency);
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(std::max(inputs[INPUT_PITCH].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

		bVCAEnabled = params[PARAM_VCA].getValue();
		bFlattenEnabled = params[PARAM_FLAT].getValue();
		bAutoTrigger = params[PARAM_AUTO].getValue();
		bPaques = params[PARAM_MORSE].getValue();
		waveShaperValue = params[PARAM_SIGN].getValue();
		driftValue = params[PARAM_DRIFT].getValue();

		outputs[OUTPUT_OUT].setChannels(channelCount);

		for (int channel = 0; channel < channelCount; ++channel) {
			settings[channel].quantizer_scale = params[PARAM_SCALE].getValue();
			settings[channel].quantizer_root = params[PARAM_ROOT].getValue();
			settings[channel].pitch_range = params[PARAM_PITCH_RANGE].getValue();
			settings[channel].pitch_octave = params[PARAM_PITCH_OCTAVE].getValue();
			settings[channel].trig_delay = params[PARAM_TRIGGER_DELAY].getValue();
			settings[channel].sample_rate = params[PARAM_RATE].getValue();
			settings[channel].resolution = params[PARAM_BITS].getValue();
			settings[channel].ad_attack = params[PARAM_ATTACK].getValue();
			settings[channel].ad_decay = params[PARAM_DECAY].getValue();
			settings[channel].ad_timbre = params[PARAM_AD_TIMBRE].getValue();
			settings[channel].ad_fm = params[PARAM_AD_MODULATION].getValue();
			settings[channel].ad_color = params[PARAM_AD_COLOR].getValue();

			// Trigger
			bool bTriggerInput = inputs[INPUT_TRIGGER].getVoltage(channel) >= 1.f;
			if (!bLastTrig[channel] && bTriggerInput) {
				bFlagTriggerDetected[channel] = bTriggerInput;
			}
			bLastTrig[channel] = bTriggerInput;

			if (bFlagTriggerDetected[channel]) {
				triggerDelay[channel] = settings[channel].trig_delay ? (1 << settings[channel].trig_delay) : 0;
				++triggerDelay[channel];
				bFlagTriggerDetected[channel] = false;
			}

			if (triggerDelay[channel]) {
				--triggerDelay[channel];
				if (triggerDelay[channel] == 0) {
					bTriggerFlag[channel] = true;
				}
			}

			// Handle switches
			settings[channel].meta_modulation = 1;
			settings[channel].ad_vca = bVCAEnabled;
			settings[channel].vco_drift = driftValue;
			settings[channel].vco_flatten = bFlattenEnabled;
			settings[channel].signature = waveShaperValue;
			settings[channel].auto_trig = bAutoTrigger;

			// Quantizer
			if (currentScale[channel] != settings[channel].quantizer_scale) {
				currentScale[channel] = settings[channel].quantizer_scale;
				quantizer[channel].Configure(braids::scales[currentScale[channel]]);
			}

			// Render frames
			if (drbOutputBuffer[channel].empty()) {
				envelope[channel].Update(settings[channel].ad_attack * 8, settings[channel].ad_decay * 8);
				uint32_t adValue = envelope[channel].Render();

				float fm = params[PARAM_FM].getValue() * inputs[INPUT_FM].getVoltage(channel);

				// Set model
				if (!bPaques) {
					int model = params[PARAM_MODEL].getValue();
					if (inputs[INPUT_META].isConnected()) {
						model += roundf(inputs[INPUT_META].getVoltage(channel) / 10.f * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
					}

					settings[channel].shape = clamp(model, 0, braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);

					// Setup oscillator from settings
					osc[channel].set_shape(braids::MacroOscillatorShape(settings[channel].shape));
				} else {
					osc[channel].set_shape(braids::MACRO_OSC_SHAPE_QUESTION_MARK);
				}

				// Set timbre/modulation
				float timbre = params[PARAM_TIMBRE].getValue() + params[PARAM_MODULATION].getValue() * inputs[INPUT_TIMBRE].getVoltage(channel) / 5.f;
				float modulation = params[PARAM_COLOR].getValue() + inputs[INPUT_COLOR].getVoltage(channel) / 5.f;

				timbre += adValue / 65535.f * settings[channel].ad_timbre / 16.f;
				modulation += adValue / 65535.f * settings[channel].ad_color / 16.f;

				int16_t param1 = math::rescale(clamp(timbre, 0.f, 1.f), 0.f, 1.f, 0, INT16_MAX);
				int16_t param2 = math::rescale(clamp(modulation, 0.f, 1.f), 0.f, 1.f, 0, INT16_MAX);
				osc[channel].set_parameters(param1, param2);

				// Set pitch
				float pitchV = inputs[INPUT_PITCH].getVoltage(channel) + params[PARAM_COARSE].getValue() + params[PARAM_FINE].getValue() / 12.f;
				pitchV += fm;

				if (bLowCpu) {
					pitchV += log2f(96000.f / args.sampleRate);
				}

				int16_t pitch = (pitchV * 12.f + 60) * 128;

				// pitch_range
				switch (settings[channel].pitch_range)
				{
				case braids::PITCH_RANGE_EXTERNAL:
				case braids::PITCH_RANGE_LFO:
					// Do nothing: calibration not implemented.
					break;
				case braids::PITCH_RANGE_FREE:
					pitch -= 1638;
					break;
				case braids::PITCH_RANGE_440:
					pitch = 69 << 7;
					break;
				case braids::PITCH_RANGE_EXTENDED:
				default:
					pitch -= 60 << 7;
					pitch = (pitch - 1638) * 9 >> 1;
					pitch += 60 << 7;
					break;
				}

				pitch = quantizer[channel].Process(pitch, (60 + settings[channel].quantizer_root) << 7);

				// Check if the pitch has changed to cause an auto-retrigger
				int16_t pitchDelta = pitch - previousPitch[channel];
				bFlagTriggerDetected[channel] = settings[channel].auto_trig && (pitchDelta >= 0x40 || -pitchDelta >= 0x40);

				previousPitch[channel] = pitch;

				pitch += jitterSource[channel].Render(settings[channel].vco_drift);
				pitch += adValue * settings[channel].ad_fm >> 7;

				pitch = clamp(static_cast<int16_t>(pitch), 0, 16383);

				if (settings[channel].vco_flatten) {
					pitch = braids::Interpolate88(braids::lut_vco_detune, pitch << 2);
				}

				// Pitch transposition
				int32_t transposition = settings[channel].pitch_range == braids::PITCH_RANGE_LFO ? -(36 << 7) : 0;
				transposition += (static_cast<int16_t>(settings[channel].pitch_octave) - 2) * 12 * 128;
				osc[channel].set_pitch(pitch + transposition);

				if (bTriggerFlag[channel]) {
					osc[channel].Strike();
					envelope[channel].Trigger(braids::ENV_SEGMENT_ATTACK);
					bTriggerFlag[channel] = false;
				}

				// TODO: add a sync input buffer (must be sample rate converted)
				const uint8_t syncBuffer[nodiCommon::kBlockSize] = {};

				int16_t renderBuffer[nodiCommon::kBlockSize];
				osc[channel].Render(syncBuffer, renderBuffer, nodiCommon::kBlockSize);

				// Signature waveshaping, decimation, and bit reduction
				int16_t sample = 0;
				size_t decimationFactor = nodiCommon::decimationFactors[settings[channel].sample_rate];
				uint16_t bitMask = nodiCommon::bitReductionMasks[settings[channel].resolution];
				int32_t gain = settings[channel].ad_vca > 0 ? adValue : 65535;
				uint16_t signature = settings[channel].signature * settings[channel].signature * 4095;
				for (size_t block = 0; block < nodiCommon::kBlockSize; ++block) {
					if (block % decimationFactor == 0) {
						sample = renderBuffer[block] & bitMask;
					}
					sample = sample * gainLp[channel] >> 16;
					gainLp[channel] += (gain - gainLp[channel]) >> 4;
					int16_t warped = waveShaper[channel].Transform(sample);
					renderBuffer[block] = stmlib::Mix(sample, warped, signature);
				}

				if (!bLowCpu) {
					// Sample rate convert
					dsp::Frame<1> in[nodiCommon::kBlockSize];
					for (int block = 0; block < nodiCommon::kBlockSize; ++block) {
						in[block].samples[0] = renderBuffer[block] / 32768.f;
					}
					sampleRateConverter[channel].setRates(96000, args.sampleRate);

					int inLen = nodiCommon::kBlockSize;
					int outLen = drbOutputBuffer[channel].capacity();
					sampleRateConverter[channel].process(in, &inLen, drbOutputBuffer[channel].endData(), &outLen);
					drbOutputBuffer[channel].endIncr(outLen);
				} else {
					for (int block = 0; block < nodiCommon::kBlockSize; ++block) {
						dsp::Frame<1> inFrame;
						inFrame.samples[0] = renderBuffer[block] / 32768.f;
						drbOutputBuffer[channel].push(inFrame);
					}
				}
			}

			// Output
			if (!drbOutputBuffer[channel].empty()) {
				dsp::Frame<1> outFrame = drbOutputBuffer[channel].shift();
				outputs[OUTPUT_OUT].setVoltage(5.f * outFrame.samples[0], channel);
			}
		} // Channels

		if (clockDivider.process()) {
			const float sampleTime = args.sampleTime * kClockUpdateFrequency;

			pollSwitches(sampleTime);

			// Handle model light
			if (!bPaques) {
				int currentModel = settings[0].shape;
				lights[LIGHT_MODEL + 0].setBrightnessSmooth(nodi::lightColors[currentModel].red, sampleTime);
				lights[LIGHT_MODEL + 1].setBrightnessSmooth(nodi::lightColors[currentModel].green, sampleTime);
				lights[LIGHT_MODEL + 2].setBrightnessSmooth(nodi::lightColors[currentModel].blue, sampleTime);
			} else {
				lights[LIGHT_MODEL + 0].setBrightnessSmooth(nodi::lightColors[47].red, sampleTime);
				lights[LIGHT_MODEL + 1].setBrightnessSmooth(nodi::lightColors[47].green, sampleTime);
				lights[LIGHT_MODEL + 2].setBrightnessSmooth(nodi::lightColors[47].blue, sampleTime);
			}

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				int currentLight = LIGHT_CHANNEL_MODEL + channel * 3;

				if (channel < channelCount) {
					int selectedModel = settings[channel].shape;
					lights[currentLight + 0].setBrightnessSmooth(nodi::lightColors[selectedModel].red, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(nodi::lightColors[selectedModel].green, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(nodi::lightColors[selectedModel].blue, sampleTime);
				} else {
					for (int light = 0; light < 3; ++light) {
						lights[currentLight + light].setBrightnessSmooth(0.f, sampleTime);
					}
				}
			}

			if (displayChannel >= channelCount) {
				displayChannel = channelCount - 1;
			}
		}

		handleDisplay(args);
	}

	inline void handleDisplay(const ProcessArgs& args) {
		// Display handling
		// Display - return to model after 2s
		if (lastSettingChanged == braids::SETTING_OSCILLATOR_SHAPE) {
			if (!bPaques) {
				displayText = nodi::displayLabels[settings[displayChannel].shape];
			} else {
				displayText = nodi::displayLabels[47];
			}
		} else {
			int value;
			switch (lastSettingChanged)
			{
			case braids::SETTING_RESOLUTION: {
				value = settings[displayChannel].resolution;
				displayText = nodiCommon::bitsStrings[value];
				break;
			}
			case braids::SETTING_SAMPLE_RATE: {
				value = settings[displayChannel].sample_rate;
				displayText = nodiCommon::ratesStrings[value];
				break;
			}
			case braids::SETTING_TRIG_SOURCE: {
				displayText = nodiCommon::autoLabel;
				break;
			}
			case braids::SETTING_TRIG_DELAY: {
				value = settings[displayChannel].trig_delay;
				displayText = nodiCommon::triggerDelayStrings[value];
				break;
			}
			case braids::SETTING_AD_ATTACK: {
				value = settings[displayChannel].ad_attack;
				displayText = nodiCommon::numberStrings15[value];
				break;
			}
			case braids::SETTING_AD_DECAY: {
				value = settings[displayChannel].ad_decay;
				displayText = nodiCommon::numberStrings15[value];
				break;
			}
			case braids::SETTING_AD_FM: {
				value = settings[displayChannel].ad_fm;
				displayText = nodiCommon::numberStrings15[value];
				break;
			}
			case braids::SETTING_AD_TIMBRE: {
				value = settings[displayChannel].ad_timbre;
				displayText = nodiCommon::numberStrings15[value];
				break;
			}
			case braids::SETTING_AD_COLOR: {
				value = settings[displayChannel].ad_color;
				displayText = nodiCommon::numberStrings15[value];
				break;
			}
			case braids::SETTING_AD_VCA: {
				displayText = nodiCommon::vcaLabel;
				break;
			}
			case braids::SETTING_PITCH_RANGE: {
				value = settings[displayChannel].pitch_range;
				displayText = nodiCommon::pitchRangeStrings[value];
				break;
			}
			case braids::SETTING_PITCH_OCTAVE: {
				value = settings[displayChannel].pitch_octave;
				displayText = nodiCommon::octaveStrings[value];
				break;
			}
			case braids::SETTING_QUANTIZER_SCALE: {
				value = settings[displayChannel].quantizer_scale;
				displayText = nodiCommon::quantizationStrings[value];
				break;
			}
			case braids::SETTING_QUANTIZER_ROOT: {
				value = settings[displayChannel].quantizer_root;
				displayText = nodiCommon::noteStrings[value];
				break;
			}
			case braids::SETTING_VCO_FLATTEN: {
				displayText = nodiCommon::flatLabel;
				break;
			}
			case braids::SETTING_VCO_DRIFT: {
				value = settings[displayChannel].vco_drift;
				displayText = nodiCommon::intensityDisplayStrings[value];
				break;
			}
			case braids::SETTING_SIGNATURE: {
				value = settings[displayChannel].signature;
				displayText = nodiCommon::intensityDisplayStrings[value];
				break;
			}
			default: {
				break;
			}
			}

			++displayTimeout;
		}

		if (displayTimeout > args.sampleRate) {
			lastSettingChanged = braids::SETTING_OSCILLATOR_SHAPE;
			displayTimeout = 0;
		}

		uint8_t* arrayLastSettings = &lastSettings.shape;
		const uint8_t* arraySettings = &settings[displayChannel].shape;
		for (int i = 0; i <= braids::SETTING_LAST_EDITABLE_SETTING; ++i) {
			if (arraySettings[i] != arrayLastSettings[i]) {
				arrayLastSettings[i] = arraySettings[i];
				lastSettingChanged = static_cast<braids::Setting>(i);
				displayTimeout = 0;
			}
		}
	}

	inline void pollSwitches(const float sampleTime) {
		// Handle switch lights
		lights[LIGHT_MORSE].setBrightnessSmooth(bPaques ? kSanguineButtonLightValue : 0.f, sampleTime);
		lights[LIGHT_VCA].setBrightnessSmooth(bVCAEnabled ? kSanguineButtonLightValue : 0.f, sampleTime);
		lights[LIGHT_FLAT].setBrightnessSmooth(bFlattenEnabled ? kSanguineButtonLightValue : 0.f, sampleTime);
		lights[LIGHT_AUTO].setBrightnessSmooth(bAutoTrigger ? kSanguineButtonLightValue : 0.f, sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* lowCpuJ = json_boolean(bLowCpu);
		json_object_set_new(rootJ, "bLowCpu", lowCpuJ);

		json_t* displayChannelJ = json_integer(displayChannel);
		json_object_set_new(rootJ, "displayChannel", displayChannelJ);

		json_t* userSignSeedJ = json_integer(userSignSeed);
		json_object_set_new(rootJ, "userSignSeed", userSignSeedJ);

		json_t* perInstanceSignSeedJ = json_boolean(bPerInstanceSignSeed);
		json_object_set_new(rootJ, "perInstanceSignSeed", perInstanceSignSeedJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* lowCpuJ = json_object_get(rootJ, "bLowCpu");
		if (lowCpuJ) {
			bLowCpu = json_boolean_value(lowCpuJ);
		}

		json_t* displayChannelJ = json_object_get(rootJ, "displayChannel");
		if (displayChannelJ) {
			displayChannel = json_integer_value(displayChannelJ);
		}

		json_t* userSignSeedJ = json_object_get(rootJ, "userSignSeed");
		if (userSignSeedJ) {
			userSignSeed = json_integer_value(userSignSeedJ);
			setWaveShaperSeed(userSignSeed);
			bNeedSeed = false;
		}

		json_t* perInstanceSignSeedJ = json_object_get(rootJ, "perInstanceSignSeed");
		if (perInstanceSignSeedJ) {
			bPerInstanceSignSeed = json_boolean_value(perInstanceSignSeedJ);
		}
	}

	void setWaveShaperSeed(uint32_t seed) {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			waveShaper[channel].Init(seed);
		}
	}

	uint32_t getInstanceSeed() {
		uint64_t moduleId = getId();
		uint32_t waveShaperSeed = static_cast<uint32_t>(moduleId ^ (moduleId >> 16));
		return waveShaperSeed;
	}

	void togglePerInstanceSignSeed() {
		bPerInstanceSignSeed = !bPerInstanceSignSeed;
		if (bPerInstanceSignSeed) {
			userSignSeed = getInstanceSeed();
		} else {
			userSignSeed = 0x0000;
		}
		setWaveShaperSeed(userSignSeed);
	}

	void randomizeSignSeed() {
		userSignSeed = random::u32();
		setWaveShaperSeed(userSignSeed);
	}

	void setUserSeed(uint32_t newSeed) {
		userSignSeed = newSeed;
		setWaveShaperSeed(userSignSeed);
	}

	void onAdd(const AddEvent& e) override {
		if (bNeedSeed) {
			userSignSeed = getInstanceSeed();
			setWaveShaperSeed(userSignSeed);
		}
	}

	int getModelParam() {
		return params[PARAM_MODEL].getValue();
	}
	void setModelParam(int shape) {
		params[PARAM_MODEL].setValue(shape);
	}

	int getScaleParam() {
		return params[PARAM_SCALE].getValue();
	}

	void setScaleParam(int scale) {
		params[PARAM_SCALE].setValue(scale);
	}
};

struct NodiWidget : SanguineModuleWidget {
	explicit NodiWidget(Nodi* module) {
		setModule(module);

		moduleName = "nodi";
		panelSize = SIZE_28;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* nodiFrambuffer = new FramebufferWidget();
		addChild(nodiFrambuffer);

		const float lightXBase = 6.894f;
		const float lightXDelta = 4.f;

		const int offset = 8;
		float currentX = lightXBase;
		for (int component = 0; component < 8; ++component) {
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 20.308),
				module, Nodi::LIGHT_CHANNEL_MODEL + component * 3));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 24.308),
				module, Nodi::LIGHT_CHANNEL_MODEL + ((component + offset) * 3)));
			currentX += lightXDelta;
		}

		nodiCommon::NodiDisplay* nodiDisplay = new nodiCommon::NodiDisplay(4, module, 71.12, 20.996);
		nodiFrambuffer->addChild(nodiDisplay);
		nodiDisplay->fallbackString = nodi::displayLabels[0];

		if (module) {
			nodiDisplay->values.displayText = &module->displayText;
			nodiDisplay->displayTimeout = &module->displayTimeout;
		}

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(106.234, 20.996), module, Nodi::INPUT_META));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(71.12, 67.247), module, Nodi::PARAM_MODEL));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(71.12, 67.247),
			module, Nodi::LIGHT_MODEL));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.222, 36.606), module, Nodi::INPUT_TIMBRE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(22.768, 36.606), module, Nodi::PARAM_TIMBRE));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.46, 40.53), module, Nodi::PARAM_COARSE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(millimetersToPixelsVec(71.12, 42.184),
			module, Nodi::PARAM_MORSE, Nodi::LIGHT_MORSE));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.809, 40.53), module, Nodi::PARAM_FINE));


		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(119.474, 36.666), module, Nodi::PARAM_ATTACK));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 45.206), module, Nodi::PARAM_AD_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(107.074, 45.206),
			module, Nodi::PARAM_VCA, Nodi::LIGHT_VCA));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(10.076, 67.247), module, Nodi::PARAM_MODULATION));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(36.032, 67.247), module, Nodi::PARAM_ROOT));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(62.607, 105.967), module, Nodi::PARAM_DRIFT));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(71.12, 96.625),
			module, Nodi::PARAM_FLAT, Nodi::LIGHT_FLAT));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(106.234, 67.247), module, Nodi::PARAM_SCALE));
		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(132.166, 67.247), module, Nodi::PARAM_DECAY));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 88.962), module, Nodi::PARAM_AD_COLOR));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(107.074, 88.962), module, Nodi::PARAM_AD_MODULATION));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.222, 97.889), module, Nodi::INPUT_COLOR));
		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(22.768, 97.889), module, Nodi::PARAM_COLOR));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.457, 93.965), module, Nodi::PARAM_PITCH_OCTAVE));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(79.633, 105.967), module, Nodi::PARAM_SIGN));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.806, 93.965), module, Nodi::PARAM_PITCH_RANGE));

		addParam(createParamCentered<Sanguine1PSOrange>(millimetersToPixelsVec(119.474, 97.889), module, Nodi::PARAM_FM));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(133.968, 97.889), module, Nodi::INPUT_FM));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(8.222, 117.788), module, Nodi::INPUT_PITCH));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(21.722, 117.788), module, Nodi::INPUT_TRIGGER));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 117.788), module, Nodi::PARAM_TRIGGER_DELAY));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(46.798, 117.788),
			module, Nodi::PARAM_AUTO, Nodi::LIGHT_AUTO));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(62.607, 118.103), module, Nodi::PARAM_BITS));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(79.633, 118.103), module, Nodi::PARAM_RATE));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(133.968, 117.788), module, Nodi::OUTPUT_OUT));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 98.491, 112.723);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 111.492, 119.656);
		addChild(mutantsLogo);
	}

	struct TextFieldMenuItem : ui::TextField {
		uint32_t value;
		Nodi* module;

		TextFieldMenuItem(uint32_t TheValue, Nodi* theModule) {
			value = TheValue;
			module = theModule;
			multiline = false;
			box.size = Vec(150, 20);
			text = string::f("%u", value);
		}

		void step() override {
			// Keep selected
			APP->event->setSelectedWidget(this);
			TextField::step();
		}

		void onSelectKey(const SelectKeyEvent& e) override {
			if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				try {
					uint32_t newValue = std::stoul(text);
					module->setUserSeed(newValue);
				}
				catch (...) {

				}

				ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
				overlay->requestDelete();
				e.consume(this);
			}

			if (!e.getTarget()) {
				TextField::onSelectKey(e);
			}
		}
	};

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Nodi* module = dynamic_cast<Nodi*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < static_cast<int>(nodi::displayLabels.size() - 1); ++i) {
			modelLabels.push_back(nodi::displayLabels[i] + ": " + nodi::menuLabels[i]);
		}
		menu->addChild(createIndexSubmenuItem("Model", modelLabels,
			[=]() {return module->getModelParam(); },
			[=](int i) {module->setModelParam(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Scale", nodiCommon::scaleStrings,
			[=]() {return module->getScaleParam(); },
			[=](int i) {module->setScaleParam(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Options", "",
			[=](Menu* menu) {
				std::vector<std::string> availableChannels;
				for (int i = 0; i < module->channelCount; ++i) {
					availableChannels.push_back(channelNumbers[i]);
				}
				menu->addChild(createIndexSubmenuItem("Display channel", availableChannels,
					[=]() {return module->displayChannel; },
					[=](int i) {module->displayChannel = i; }
				));

				menu->addChild(new MenuSeparator);

				menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", "", &module->bLowCpu));

				menu->addChild(new MenuSeparator);

				menu->addChild(createSubmenuItem("Signature wave shaper (SIGN)", "",
					[=](Menu* menu) {
						menu->addChild(createCheckMenuItem("Instance seed", "",
							[=]() {return module->bPerInstanceSignSeed; },
							[=]() {module->togglePerInstanceSignSeed(); }));

						menu->addChild(new MenuSeparator);

						if (module->bPerInstanceSignSeed) {
							menu->addChild(createMenuItem("Random seed", "", [=]() {
								module->randomizeSignSeed();
								}));

							menu->addChild(new MenuSeparator);

							menu->addChild(createMenuLabel("Min: 0, Max: 4294967295, ENTER to set"));

							menu->addChild(createSubmenuItem("User seed", "",
								[=](Menu* menu) {
									menu->addChild(new TextFieldMenuItem(module->userSignSeed, module));
								}
							));
						} else {
							menu->addChild(createMenuLabel("Enable \"Instance seed\" to use random and custom seeds"));
						}
					}
				));
			}
		));
	}
};

Model* modelNodi = createModel<Nodi, NodiWidget>("Sanguine-Nodi");