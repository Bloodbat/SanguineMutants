#include <string.h>

#include "plugin.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "sanguinejson.hpp"

#include "renaissance/renaissance_macro_oscillator.h"
#include "renaissance/renaissance_signature_waveshaper.h"
#include "renaissance/renaissance_vco_jitter_source.h"
#include "renaissance/renaissance_envelope.h"
#include "renaissance/renaissance_quantizer.h"
#include "renaissance/renaissance_quantizer_scales.h"

#include "nodicommon.hpp"

#include "contextus.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Contextus : SanguineModule {
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

		// Unused: kept for compatibility. ----->
		PARAM_META,
		// <-----

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
		INPUT_ATTACK,
		INPUT_DECAY,
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

	renaissance::MacroOscillator oscillators[PORT_MAX_CHANNELS];
	renaissance::SettingsData settings[PORT_MAX_CHANNELS];
	renaissance::VcoJitterSource jitterSources[PORT_MAX_CHANNELS];
	renaissance::SignatureWaveshaper waveShapers[PORT_MAX_CHANNELS];
	renaissance::Envelope envelopes[PORT_MAX_CHANNELS];
	renaissance::Quantizer quantizers[PORT_MAX_CHANNELS];

	uint8_t selectedScales[PORT_MAX_CHANNELS] = {};
	uint8_t waveShaperValue = 0;
	uint8_t driftValue = 0;

	int16_t previousPitches[PORT_MAX_CHANNELS] = {};

	uint16_t gainLps[PORT_MAX_CHANNELS] = {};
	uint16_t triggerDelays[PORT_MAX_CHANNELS] = {};

	uint8_t knobScale = 0;
	uint8_t knobRoot = 0;
	uint8_t knobPitchRange = 0;
	uint8_t knobPitchOctave = 2;
	uint8_t knobTriggerDelay = 0;
	uint8_t knobSampleRate = 6;
	uint8_t knobResolution = 6;
	uint8_t knobAttack = 0;
	uint8_t knobDecay = 7;
	uint8_t knobAdTimbre = 0;
	uint8_t knobAdFm = 0;
	uint8_t knobAdColor = 0;

	uint8_t knobModel = 0;

	int channelCount = 0;
	int displayChannel = 0;
	static const int kLightsFrequency = 16;
	int jitteredLightsFrequency;

	dsp::DoubleRingBuffer<dsp::Frame<PORT_MAX_CHANNELS>, 256> drbOutputBuffers;
	dsp::SampleRateConverter<PORT_MAX_CHANNELS> srcOutput;
	dsp::ClockDivider lightsDivider;

	bool triggersDetected[PORT_MAX_CHANNELS] = {};
	bool lastTriggers[PORT_MAX_CHANNELS] = {};
	bool triggeredChannels[PORT_MAX_CHANNELS] = {};

	bool bAutoTrigger = false;
	bool bFlattenEnabled = false;
	bool bVCAEnabled = false;

	bool bHaveMetaCable = false;
	bool bHaveAttackCable = false;
	bool bHaveDecayCable = false;

	bool bWantLowCpu = false;

	bool bPerInstanceSignSeed = true;
	bool bNeedSignSeed = true;

	// Display stuff.
	renaissance::SettingsData lastSettings = {};
	renaissance::Setting lastSettingChanged = renaissance::SETTING_OSCILLATOR_SHAPE;

	uint32_t displayTimeout = 0;
	uint32_t userSignSeed = 0;

	std::string displayText = "";

	float log2SampleRate;

	float knobFm = 0.f;
	float knobTimbre = 0.5f;
	float knobModulation = 0.f;
	float knobColor = 0.5f;
	float knobCoarse = -1.f;
	float knobFine = 0.f;

	Contextus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODEL, 0.f, renaissance::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META, 0.f,
			"Model", contextus::menuLabels);
		configParam(PARAM_MODULATION, -1.f, 1.f, 0.f, "Modulation");
		configParam(PARAM_COARSE, -5.f, 3.f, -1.f, "Coarse frequency", " semitones", 0.f, 12.f, 12.f);
		configParam(PARAM_FINE, -1.f, 1.f, 0.f, "Fine frequency", " semitones");
		configSwitch(PARAM_ATTACK, 0.f, 15.f, 0.f, "Attack", nodiCommon::numberStrings15);

		configSwitch(PARAM_AD_TIMBRE, 0.f, 15.f, 0.f, "Timbre AD", nodiCommon::numberStrings15);

		configParam(PARAM_TIMBRE, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);
		configSwitch(PARAM_ROOT, 0.f, 11.f, 0.f, "Quantizer root note", nodiCommon::noteStrings);
		configSwitch(PARAM_SCALE, 0.f, 48.f, 0.f, "Quantizer scale", nodiCommon::scaleStrings);
		configSwitch(PARAM_DECAY, 0.f, 15.f, 7.f, "Decay", nodiCommon::numberStrings15);

		configSwitch(PARAM_AD_COLOR, 0.f, 15.f, 0.f, "Color AD", nodiCommon::numberStrings15);
		configSwitch(PARAM_AD_MODULATION, 0.f, 15.f, 0.f, "FM AD", nodiCommon::numberStrings15);

		configParam(PARAM_COLOR, 0.f, 1.f, 0.5f, "Color", "%", 0.f, 100.f);
		configSwitch(PARAM_PITCH_OCTAVE, 0.f, 4.f, 2.f, "Octave", nodiCommon::octaveStrings);
		configSwitch(PARAM_PITCH_RANGE, 0.f, 4.f, 0.f, "Pitch range", nodiCommon::pitchRangeStrings);
		configParam(PARAM_FM, -1.f, 1.f, 0.f, "FM");

		configSwitch(PARAM_TRIGGER_DELAY, 0.f, 6.f, 0.f, "Trigger delay", nodiCommon::triggerDelayStrings);

		configSwitch(PARAM_BITS, 0.f, 6.f, 6.f, "Bit crusher resolution", nodiCommon::bitsStrings);
		configSwitch(PARAM_RATE, 0.f, 6.f, 6.f, "Bit crusher sample rate", nodiCommon::ratesStrings);

		configInput(INPUT_TRIGGER, "Trigger");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");
		configInput(INPUT_FM, "FM");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_COLOR, "Color");
		configOutput(OUTPUT_OUT, "Audio");

		configInput(INPUT_META, "Meta modulation");
		configButton(PARAM_VCA, "Toggle AD VCA");
		configSwitch(PARAM_DRIFT, 0.f, 4.f, 0.f, "Oscillator drift", nodiCommon::intensityTooltipStrings);
		configButton(PARAM_FLAT, "Toggle lower and higher frequency detuning");
		configSwitch(PARAM_SIGN, 0.f, 4.f, 0.f, "Signature wave shaper", nodiCommon::intensityTooltipStrings);
		configButton(PARAM_AUTO, "Toggle auto trigger");

		configInput(INPUT_ATTACK, "Attack");
		configInput(INPUT_DECAY, "Decay");

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&oscillators[channel], 0, sizeof(renaissance::MacroOscillator));
			memset(&quantizers[channel], 0, sizeof(renaissance::Quantizer));
			memset(&envelopes[channel], 0, sizeof(renaissance::Envelope));
			memset(&jitterSources[channel], 0, sizeof(renaissance::VcoJitterSource));
			memset(&waveShapers[channel], 0, sizeof(renaissance::SignatureWaveshaper));
			memset(&settings[channel], 0, sizeof(renaissance::SettingsData));

			settings[channel].meta_modulation = 1;

			oscillators[channel].Init();
			quantizers[channel].Init();
			envelopes[channel].Init();

			jitterSources[channel].Init();
			waveShapers[channel].Init(userSignSeed);

			lastTriggers[channel] = false;

			selectedScales[channel] = 0xff;

			previousPitches[channel] = 0;
		}
		memset(&lastSettings, 0, sizeof(renaissance::SettingsData));
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(std::max(inputs[INPUT_PITCH].getChannels(),
			inputs[INPUT_TRIGGER].getChannels()), 1);

		// Render frames.
		if (drbOutputBuffers.empty()) {
			dsp::Frame<PORT_MAX_CHANNELS> renderFrames[nodiCommon::kBlockSize];

			for (int channel = 0; channel < channelCount; ++channel) {
				settings[channel].quantizer_scale = knobScale;
				settings[channel].quantizer_root = knobRoot;
				settings[channel].pitch_range = knobPitchRange;
				settings[channel].pitch_octave = knobPitchOctave;
				settings[channel].trig_delay = knobTriggerDelay;
				settings[channel].sample_rate = knobSampleRate;
				settings[channel].resolution = knobResolution;
				settings[channel].ad_attack = knobAttack;
				settings[channel].ad_decay = knobDecay;
				settings[channel].ad_timbre = knobAdTimbre;
				settings[channel].ad_fm = knobAdFm;
				settings[channel].ad_color = knobAdColor;

				// Trigger.
				bool bTriggerInput = inputs[INPUT_TRIGGER].getVoltage(channel) >= 1.f;
				if (!lastTriggers[channel] && bTriggerInput) {
					triggersDetected[channel] = bTriggerInput;
				}
				lastTriggers[channel] = bTriggerInput;

				if (triggersDetected[channel]) {
					triggerDelays[channel] = settings[channel].trig_delay ?
						(1 << settings[channel].trig_delay) : 0;
					++triggerDelays[channel];
					triggersDetected[channel] = false;
				}

				if (triggerDelays[channel]) {
					--triggerDelays[channel];
					if (triggerDelays[channel] == 0) {
						triggeredChannels[channel] = true;
					}
				}

				// Handle switches.
				settings[channel].ad_vca = bVCAEnabled;
				settings[channel].vco_drift = driftValue;
				settings[channel].vco_flatten = bFlattenEnabled;
				settings[channel].signature = waveShaperValue;
				settings[channel].auto_trig = bAutoTrigger;

				// Quantizer.
				if (selectedScales[channel] != settings[channel].quantizer_scale) {
					selectedScales[channel] = settings[channel].quantizer_scale;
					quantizers[channel].Configure(renaissance::scales[selectedScales[channel]]);
				}

				if (bHaveAttackCable) {
					float attackVoltage = inputs[INPUT_ATTACK].getVoltage(channel);
					attackVoltage = roundf(attackVoltage);
					attackVoltage = rescale(attackVoltage, -5.f, 5.f, -15.f, 15.f);
					int modulatedAttack = settings[channel].ad_attack + static_cast<int>(attackVoltage);
					modulatedAttack = clamp(modulatedAttack, 0, 15);
					settings[channel].ad_attack = modulatedAttack;
				}

				if (bHaveDecayCable) {
					float decayVoltage = inputs[INPUT_DECAY].getVoltage(channel);
					decayVoltage = roundf(decayVoltage);
					decayVoltage = rescale(decayVoltage, -5.f, 5.f, -15.f, 15.f);
					int modulatedDecay = settings[channel].ad_decay + static_cast<int>(decayVoltage);
					modulatedDecay = clamp(modulatedDecay, 0, 15);
					settings[channel].ad_decay = modulatedDecay;
				}

				envelopes[channel].Update(settings[channel].ad_attack << 3, settings[channel].ad_decay << 3);
				uint32_t adValue = envelopes[channel].Render();
				const float scaledAdValue = adValue / 65535.f;

				float fm = knobFm * inputs[INPUT_FM].getVoltage(channel);

				// Set model.
				uint8_t model = knobModel;
				if (bHaveMetaCable) {
					model += roundf(inputs[INPUT_META].getVoltage(channel) / 10.f *
						renaissance::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
				}

				settings[channel].shape = clamp(model, 0, renaissance::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);

				// Setup oscillator from settings.
				oscillators[channel].set_shape(renaissance::MacroOscillatorShape(settings[channel].shape));

				// Set timbre/modulation.
				float timbre = knobTimbre + knobModulation * inputs[INPUT_TIMBRE].getVoltage(channel) / 5.f;
				float modulation = knobColor + inputs[INPUT_COLOR].getVoltage(channel) / 5.f;

				timbre += scaledAdValue * settings[channel].ad_timbre / 16.f;
				modulation += scaledAdValue * settings[channel].ad_color / 16.f;

				int16_t param1 = math::rescale(clamp(timbre, 0.f, 1.f), 0.f, 1.f, 0, INT16_MAX);
				int16_t param2 = math::rescale(clamp(modulation, 0.f, 1.f), 0.f, 1.f, 0, INT16_MAX);
				oscillators[channel].set_parameters(param1, param2);

				// Set pitch.
				float pitchV = inputs[INPUT_PITCH].getVoltage(channel) + knobCoarse + knobFine / 12.f;
				pitchV += fm;

				if (bWantLowCpu) {
					pitchV += log2SampleRate;
				}

				int16_t pitch = (pitchV * 12.f + 60) * 128;

				// Pitch range.
				switch (settings[channel].pitch_range) {
				case renaissance::PITCH_RANGE_EXTERNAL:
				case renaissance::PITCH_RANGE_LFO:
					// Do nothing: calibration not implemented.
					break;
				case renaissance::PITCH_RANGE_FREE:
					pitch -= 1638;
					break;
				case renaissance::PITCH_RANGE_440:
					pitch = 69 << 7;
					break;
				case renaissance::PITCH_RANGE_EXTENDED:
				default:
					pitch -= 60 << 7;
					pitch = (pitch - 1638) * 9 >> 1;
					pitch += 60 << 7;
					break;
				}

				pitch = quantizers[channel].Process(pitch, (60 + settings[channel].quantizer_root) << 7);

				// Check if pitch has changed enough to cause an auto-retrigger.
				int16_t pitchDelta = pitch - previousPitches[channel];
				triggersDetected[channel] = bAutoTrigger & ((pitchDelta >= 0x40) | (-pitchDelta >= 0x40));

				previousPitches[channel] = pitch;

				pitch += jitterSources[channel].Render(settings[channel].vco_drift);
				pitch += adValue * settings[channel].ad_fm >> 7;

				pitch = clamp(static_cast<int16_t>(pitch), 0, 16383);

				if (settings[channel].vco_flatten) {
					pitch = renaissance::Interpolate88(renaissance::lut_vco_detune, pitch << 2);
				}

				// Pitch transposition.
				int32_t transposition = (settings[channel].pitch_range == renaissance::PITCH_RANGE_LFO) *
					(-(36 << 7));
				transposition += (static_cast<int16_t>(settings[channel].pitch_octave) - 2) * 12 * 128;
				oscillators[channel].set_pitch(pitch + transposition);

				if (triggeredChannels[channel]) {
					oscillators[channel].Strike();
					envelopes[channel].Trigger(renaissance::ENV_SEGMENT_ATTACK);
					triggeredChannels[channel] = false;
				}

				// TODO: Add a sync input buffer (must be sample rate converted).
				const uint8_t syncBuffer[nodiCommon::kBlockSize] = {};

				int16_t renderBuffer[nodiCommon::kBlockSize];
				oscillators[channel].Render(syncBuffer, renderBuffer, nodiCommon::kBlockSize);

				// Signature waveshaping, decimation, and bit reduction.
				int16_t sample = 0;
				size_t decimationFactor = nodiCommon::decimationFactors[settings[channel].sample_rate];
				uint16_t bitMask = nodiCommon::bitReductionMasks[settings[channel].resolution];
				int32_t gain = settings[channel].ad_vca > 0 ? adValue : 65535;
				uint16_t signature = settings[channel].signature * settings[channel].signature * 4095;
				for (size_t block = 0; block < nodiCommon::kBlockSize; ++block) {
					if (block % decimationFactor == 0) {
						sample = renderBuffer[block] & bitMask;
					}
					sample = sample * gainLps[channel] >> 16;
					gainLps[channel] += (gain - gainLps[channel]) >> 4;
					int16_t warped = waveShapers[channel].Transform(sample);
					renderBuffer[block] = stmlib::Mix(sample, warped, signature);

					renderFrames[block].samples[channel] = renderBuffer[block] / 32768.f;
				}
			} // Channels.

			if (!bWantLowCpu) {
				// Convert sample rate.
				int inLen = nodiCommon::kBlockSize;
				int outLen = drbOutputBuffers.capacity();
				srcOutput.setChannels(channelCount);
				srcOutput.process(renderFrames, &inLen, drbOutputBuffers.endData(), &outLen);
				drbOutputBuffers.endIncr(outLen);
			} else {
				int len = std::min(static_cast<int>(drbOutputBuffers.capacity()), nodiCommon::kBlockSize);
				std::memcpy(drbOutputBuffers.endData(), renderFrames, len * sizeof(renderFrames[0]));
				drbOutputBuffers.endIncr(len);
			}
		}

		// Output.
		if (!drbOutputBuffers.empty()) {
			dsp::Frame<PORT_MAX_CHANNELS> outFrame = drbOutputBuffers.shift();
			for (int channel = 0; channel < channelCount; ++channel) {
				outputs[OUTPUT_OUT].setVoltage(5.f * outFrame.samples[channel], channel);
			}
		}

		outputs[OUTPUT_OUT].setChannels(channelCount);

		if (lightsDivider.process()) {
			const float sampleTime = args.sampleTime * jitteredLightsFrequency;

			if (displayChannel >= channelCount) {
				displayChannel = channelCount - 1;
			}

			bVCAEnabled = params[PARAM_VCA].getValue();
			bFlattenEnabled = params[PARAM_FLAT].getValue();
			bAutoTrigger = params[PARAM_AUTO].getValue();
			waveShaperValue = params[PARAM_SIGN].getValue();
			driftValue = params[PARAM_DRIFT].getValue();

			knobScale = params[PARAM_SCALE].getValue();
			knobRoot = params[PARAM_ROOT].getValue();
			knobPitchRange = params[PARAM_PITCH_RANGE].getValue();
			knobPitchOctave = params[PARAM_PITCH_OCTAVE].getValue();
			knobTriggerDelay = params[PARAM_TRIGGER_DELAY].getValue();
			knobSampleRate = params[PARAM_RATE].getValue();
			knobResolution = params[PARAM_BITS].getValue();
			knobAttack = params[PARAM_ATTACK].getValue();
			knobDecay = params[PARAM_DECAY].getValue();
			knobAdTimbre = params[PARAM_AD_TIMBRE].getValue();
			knobAdFm = params[PARAM_AD_MODULATION].getValue();
			knobAdColor = params[PARAM_AD_COLOR].getValue();

			knobModel = params[PARAM_MODEL].getValue();

			knobFm = params[PARAM_FM].getValue();
			knobTimbre = params[PARAM_TIMBRE].getValue();
			knobModulation = params[PARAM_MODULATION].getValue();
			knobColor = params[PARAM_COLOR].getValue();
			knobCoarse = params[PARAM_COARSE].getValue();
			knobFine = params[PARAM_FINE].getValue();

			// Handle model light.
			lights[LIGHT_MODEL].setBrightnessSmooth(contextus::lightColors[settings[displayChannel].shape].red, sampleTime);
			lights[LIGHT_MODEL + 1].setBrightnessSmooth(contextus::lightColors[settings[displayChannel].shape].green, sampleTime);
			lights[LIGHT_MODEL + 2].setBrightnessSmooth(contextus::lightColors[settings[displayChannel].shape].blue, sampleTime);

			bool bIsChannelActive;
			int currentLight;
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				currentLight = LIGHT_CHANNEL_MODEL + channel * 3;
				bIsChannelActive = channel < channelCount;

				lights[currentLight].setBrightnessSmooth(contextus::lightColors[settings[channel].shape].red * bIsChannelActive, sampleTime);
				lights[currentLight + 1].setBrightnessSmooth(contextus::lightColors[settings[channel].shape].green * bIsChannelActive, sampleTime);
				lights[currentLight + 2].setBrightnessSmooth(contextus::lightColors[settings[channel].shape].blue * bIsChannelActive, sampleTime);
			}

			lights[LIGHT_VCA].setBrightnessSmooth(bVCAEnabled * kSanguineButtonLightValue, sampleTime);
			lights[LIGHT_FLAT].setBrightnessSmooth(bFlattenEnabled * kSanguineButtonLightValue, sampleTime);
			lights[LIGHT_AUTO].setBrightnessSmooth(bAutoTrigger * kSanguineButtonLightValue, sampleTime);
		}

		handleDisplay(args.sampleRate);
	}

	inline void handleDisplay(const float& sampleRate) {
		// Display handling.
		// Display: return to model after 2s.
		if (lastSettingChanged == renaissance::SETTING_OSCILLATOR_SHAPE) {
			displayText = contextus::displayLabels[settings[displayChannel].shape];
		} else {
			switch (lastSettingChanged) {
			case renaissance::SETTING_RESOLUTION:
				displayText =
					nodiCommon::bitsStrings[settings[displayChannel].resolution];
				break;

			case renaissance::SETTING_SAMPLE_RATE:
				displayText =
					nodiCommon::ratesStrings[settings[displayChannel].sample_rate];
				break;

			case renaissance::SETTING_TRIG_SOURCE:
				displayText = nodiCommon::autoLabel;
				break;

			case renaissance::SETTING_TRIG_DELAY:
				displayText =
					nodiCommon::triggerDelayStrings[settings[displayChannel].trig_delay];
				break;

			case renaissance::SETTING_AD_ATTACK:
				displayText =
					nodiCommon::numberStrings15[settings[displayChannel].ad_attack];
				break;

			case renaissance::SETTING_AD_DECAY:
				displayText =
					nodiCommon::numberStrings15[settings[displayChannel].ad_decay];
				break;

			case renaissance::SETTING_AD_FM:
				displayText =
					nodiCommon::numberStrings15[settings[displayChannel].ad_fm];
				break;

			case renaissance::SETTING_AD_TIMBRE:
				displayText =
					nodiCommon::numberStrings15[settings[displayChannel].ad_timbre];
				break;

			case renaissance::SETTING_AD_COLOR:
				displayText =
					nodiCommon::numberStrings15[settings[displayChannel].ad_color];
				break;

			case renaissance::SETTING_AD_VCA:
				displayText = nodiCommon::vcaLabel;
				break;

			case renaissance::SETTING_PITCH_RANGE:
				displayText =
					nodiCommon::pitchRangeStrings[settings[displayChannel].pitch_range];
				break;

			case renaissance::SETTING_PITCH_OCTAVE:
				displayText =
					nodiCommon::octaveStrings[settings[displayChannel].pitch_octave];
				break;

			case renaissance::SETTING_QUANTIZER_SCALE:
				displayText =
					nodiCommon::quantizationStrings[settings[displayChannel].quantizer_scale];
				break;

			case renaissance::SETTING_QUANTIZER_ROOT:
				displayText =
					nodiCommon::noteStrings[settings[displayChannel].quantizer_root];
				break;

			case renaissance::SETTING_VCO_FLATTEN:
				displayText = nodiCommon::flatLabel;
				break;

			case renaissance::SETTING_VCO_DRIFT:
				displayText =
					nodiCommon::intensityDisplayStrings[settings[displayChannel].vco_drift];
				break;

			case renaissance::SETTING_SIGNATURE:
				displayText =
					nodiCommon::intensityDisplayStrings[settings[displayChannel].signature];
				break;

			default:
				break;
			}

			++displayTimeout;
		}

		if (displayTimeout > sampleRate) {
			lastSettingChanged = renaissance::SETTING_OSCILLATOR_SHAPE;
			displayTimeout = 0;
		}

		uint8_t* arrayLastSettings = &lastSettings.shape;
		const uint8_t* arraySettings = &settings[displayChannel].shape;
		for (int setting = 0; setting <= renaissance::SETTING_LAST_EDITABLE_SETTING; ++setting) {
			if (arraySettings[setting] != arrayLastSettings[setting]) {
				if (bHaveAttackCable && setting == renaissance::SETTING_AD_ATTACK) {
					lastSettingChanged = renaissance::SETTING_OSCILLATOR_SHAPE;
					break;
				}

				if (bHaveDecayCable && setting == renaissance::SETTING_AD_DECAY) {
					lastSettingChanged = renaissance::SETTING_OSCILLATOR_SHAPE;
					break;
				}

				arrayLastSettings[setting] = arraySettings[setting];
				lastSettingChanged = static_cast<renaissance::Setting>(setting);
				displayTimeout = 0;
				break;
			}
		}
	}

	void onPortChange(const PortChangeEvent& e) override {
		if (e.type == Port::INPUT) {
			switch (e.portId) {
			case INPUT_META:
				bHaveMetaCable = e.connecting;
				break;

			case INPUT_ATTACK:
				bHaveAttackCable = e.connecting;
				break;

			case INPUT_DECAY:
				bHaveDecayCable = e.connecting;
				break;

			default:
				break;
			}
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonBoolean(rootJ, "bWantLowCpu", bWantLowCpu);
		setJsonInt(rootJ, "displayChannel", displayChannel);
		setJsonInt(rootJ, "userSignSeed", userSignSeed);
		setJsonBoolean(rootJ, "perInstanceSignSeed", bPerInstanceSignSeed);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue;

		/* Readded for compatibility with really old patches due to my
		   overzealousness when renaming vars. */
		getJsonBoolean(rootJ, "bLowCpu", bWantLowCpu);

		getJsonBoolean(rootJ, "bWantLowCpu", bWantLowCpu);

		if (getJsonInt(rootJ, "displayChannel", intValue)) {
			displayChannel = intValue;
		}

		if (getJsonInt(rootJ, "userSignSeed", intValue)) {
			userSignSeed = intValue;
			setWaveShaperSeed(userSignSeed);
			bNeedSignSeed = false;
		}

		getJsonBoolean(rootJ, "perInstanceSignSeed", bPerInstanceSignSeed);
	}

	void setWaveShaperSeed(uint32_t seed) {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			waveShapers[channel].Init(seed);
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
		if (bNeedSignSeed) {
			userSignSeed = getInstanceSeed();
			setWaveShaperSeed(userSignSeed);
		}

		jitteredLightsFrequency = kLightsFrequency + (getId() % kLightsFrequency);
		lightsDivider.setDivision(jitteredLightsFrequency);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		log2SampleRate = log2f(nodiCommon::kHardwareRate / e.sampleRate);

		srcOutput.setRates(nodiCommon::kHardwareRate, static_cast<int>(e.sampleRate));
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

struct ContextusWidget : SanguineModuleWidget {
	explicit ContextusWidget(Contextus* module) {
		setModule(module);

		moduleName = "contextus";
		panelSize = sanguineThemes::SIZE_28;
		backplateColor = sanguineThemes::PLATE_RED;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* nodiFramebuffer = new FramebufferWidget();
		addChild(nodiFramebuffer);

		const float lightXBase = 6.894f;
		const float lightXDelta = 4.0f;

		const int offset = 8;
		float currentX = lightXBase;
		for (int component = 0; component < 8; ++component) {
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 20.308),
				module, Contextus::LIGHT_CHANNEL_MODEL + component * 3));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 24.308),
				module, Contextus::LIGHT_CHANNEL_MODEL + ((component + offset) * 3)));
			currentX += lightXDelta;
		}

		nodiCommon::NodiDisplay* nodiDisplay = new nodiCommon::NodiDisplay(4, module, 71.12, 20.996);
		nodiFramebuffer->addChild(nodiDisplay);
		nodiDisplay->fallbackString = contextus::displayLabels[0];

		if (module) {
			nodiDisplay->values.displayText = &module->displayText;
			nodiDisplay->displayTimeout = &module->displayTimeout;
		}

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(106.234, 20.996), module, Contextus::INPUT_META));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(71.12, 67.247), module, Contextus::PARAM_MODEL));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(71.12, 67.247), module, Contextus::LIGHT_MODEL));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.222, 36.606), module, Contextus::INPUT_TIMBRE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(22.768, 36.606), module, Contextus::PARAM_TIMBRE));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.46, 40.53), module, Contextus::PARAM_COARSE));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.809, 40.53), module, Contextus::PARAM_FINE));


		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(119.474, 36.666), module, Contextus::PARAM_ATTACK));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(134.018, 36.606), module, Contextus::INPUT_ATTACK));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(134.018, 52.992), module, Contextus::INPUT_DECAY));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 45.206), module, Contextus::PARAM_AD_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(107.074, 45.206),
			module, Contextus::PARAM_VCA, Contextus::LIGHT_VCA));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(10.076, 67.247), module, Contextus::PARAM_MODULATION));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(36.032, 67.247), module, Contextus::PARAM_ROOT));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(62.607, 105.967), module, Contextus::PARAM_DRIFT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(71.12, 96.625),
			module, Contextus::PARAM_FLAT, Contextus::LIGHT_FLAT));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(106.234, 67.247), module, Contextus::PARAM_SCALE));
		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(132.166, 67.247), module, Contextus::PARAM_DECAY));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 88.962), module, Contextus::PARAM_AD_COLOR));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(107.074, 88.962), module, Contextus::PARAM_AD_MODULATION));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.222, 97.889), module, Contextus::INPUT_COLOR));
		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(22.768, 97.889), module, Contextus::PARAM_COLOR));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.457, 93.965), module, Contextus::PARAM_PITCH_OCTAVE));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(79.633, 105.967), module, Contextus::PARAM_SIGN));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.806, 93.965), module, Contextus::PARAM_PITCH_RANGE));

		addParam(createParamCentered<Sanguine1PSOrange>(millimetersToPixelsVec(119.474, 97.889), module, Contextus::PARAM_FM));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(133.968, 97.889), module, Contextus::INPUT_FM));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(8.222, 117.788), module, Contextus::INPUT_PITCH));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(21.722, 117.788), module, Contextus::INPUT_TRIGGER));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 117.788), module, Contextus::PARAM_TRIGGER_DELAY));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(46.798, 117.788),
			module, Contextus::PARAM_AUTO, Contextus::LIGHT_AUTO));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(62.607, 118.103), module, Contextus::PARAM_BITS));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(79.633, 118.103), module, Contextus::PARAM_RATE));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(133.968, 117.788), module, Contextus::OUTPUT_OUT));

#ifndef METAMODULE
		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 98.491, 112.723);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 111.492, 119.656);
		addChild(mutantsLogo);
#endif
	}

	struct TextFieldMenuItem : ui::TextField {
		uint32_t value;
		Contextus* module;

		TextFieldMenuItem(uint32_t TheValue, Contextus* theModule) {
			value = TheValue;
			module = theModule;
			multiline = false;
			box.size = Vec(150, 20);
			text = string::f("%u", value);
		}

		void step() override {
			// Keep selected.
			APP->event->setSelectedWidget(this);
			TextField::step();
		}

		void onSelectKey(const SelectKeyEvent& e) override {
			if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				uint32_t newValue = 0;
				if (strToUInt32(text.c_str(), newValue)) {
					module->setUserSeed(newValue);
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

		Contextus* module = dynamic_cast<Contextus*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < static_cast<int>(contextus::displayLabels.size()); ++i) {
			modelLabels.push_back(contextus::displayLabels[i] + ": " + contextus::menuLabels[i]);
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

				menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", "", &module->bWantLowCpu));

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

Model* modelContextus = createModel<Contextus, ContextusWidget>("Sanguine-Contextus");