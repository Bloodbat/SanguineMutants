#include <string.h>

#include "plugin.hpp"
#include "nodicommon.hpp"

#include "renaissance/renaissance_macro_oscillator.h"
#include "renaissance/renaissance_signature_waveshaper.h"
#include "renaissance/renaissance_vco_jitter_source.h"
#include "renaissance/renaissance_envelope.h"
#include "renaissance/renaissance_quantizer.h"
#include "renaissance/renaissance_quantizer_scales.h"

#include "sanguinehelpers.hpp"

#include "sanguinechannels.hpp"

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

		// Unused: kept for compatibility.
		PARAM_META,
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
		LIGHT_DRIFT,
		LIGHT_FLAT,
		LIGHT_SIGN,
		LIGHT_AUTO,
		ENUMS(LIGHT_CHANNEL_MODEL, 16 * 3),
		LIGHTS_COUNT
	};

	renaissance::MacroOscillator osc[PORT_MAX_CHANNELS];
	renaissance::SettingsData settings[PORT_MAX_CHANNELS];
	renaissance::VcoJitterSource jitterSource[PORT_MAX_CHANNELS];
	renaissance::SignatureWaveshaper waveShaper[PORT_MAX_CHANNELS];
	renaissance::Envelope envelope[PORT_MAX_CHANNELS];
	renaissance::Quantizer quantizer[PORT_MAX_CHANNELS];

	uint8_t currentScale[PORT_MAX_CHANNELS] = {};

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
	bool bDritfEnabled = false;
	bool bFlattenEnabled = false;
	bool bSignatureEnabled = false;
	bool bVCAEnabled = false;

	bool bLowCpu = false;

	// Display stuff
	renaissance::SettingsData lastSettings = {};
	renaissance::Setting lastSettingChanged = renaissance::SETTING_OSCILLATOR_SHAPE;

	uint32_t displayTimeout = 0;

	std::string displayText = "";

	Contextus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODEL, 0.f, renaissance::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META, 0.f,
			"Model", contextusMenuLabels);
		configParam(PARAM_MODULATION, -1.f, 1.f, 0.f, "Modulation");
		configParam(PARAM_COARSE, -5.f, 3.f, -1.f, "Coarse frequency", " semitones", 0.f, 12.f, 12.f);
		configParam(PARAM_FINE, -1.f, 1.f, 0.f, "Fine frequency", " semitones");
		configParam(PARAM_ATTACK, 0.f, 15.f, 0.f, "Attack");
		paramQuantities[PARAM_ATTACK]->snapEnabled = true;

		configParam(PARAM_AD_TIMBRE, 0.f, 15.f, 0.f, "Timbre AD");
		paramQuantities[PARAM_AD_TIMBRE]->snapEnabled = true;

		configParam(PARAM_TIMBRE, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);
		configSwitch(PARAM_ROOT, 0.f, 11.f, 0.f, "Quantizer root note", nodiNoteStrings);
		configSwitch(PARAM_SCALE, 0.f, 48.f, 0.f, "Quantizer scale", nodiScaleStrings);
		configParam(PARAM_DECAY, 0.f, 15.f, 7.f, "Decay");
		paramQuantities[PARAM_DECAY]->snapEnabled = true;

		configParam(PARAM_AD_COLOR, 0.f, 15.f, 0.f, "Color AD");
		paramQuantities[PARAM_AD_COLOR]->snapEnabled = true;
		configParam(PARAM_AD_MODULATION, 0.f, 15.f, 0.f, "FM AD");
		paramQuantities[PARAM_AD_MODULATION]->snapEnabled = true;

		configParam(PARAM_COLOR, 0.f, 1.f, 0.5f, "Color", "%", 0.f, 100.f);
		configSwitch(PARAM_PITCH_OCTAVE, 0.f, 4.f, 2.f, "Octave", nodiOctaveStrings);
		configSwitch(PARAM_PITCH_RANGE, 0.f, 3.f, 0.f, "Pitch range", nodiPitchRangeStrings);
		configParam(PARAM_FM, -1.f, 1.f, 0.f, "FM");

		configSwitch(PARAM_TRIGGER_DELAY, 0.f, 6.f, 0.f, "Trigger delay", nodiTriggerDelayStrings);

		configSwitch(PARAM_BITS, 0.f, 6.f, 6.f, "Bit crusher resolution", nodiBitsStrings);
		configSwitch(PARAM_RATE, 0.f, 6.0f, 6.0f, "Bit crusher sample rate", nodiRatesStrings);

		configInput(INPUT_TRIGGER, "Trigger");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");
		configInput(INPUT_FM, "FM");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_COLOR, "Color");
		configOutput(OUTPUT_OUT, "Audio");

		configInput(INPUT_META, "Meta modulation");
		// Unused: kept for compatibility.
		configButton(PARAM_META, "");
		configButton(PARAM_VCA, "Toggle AD VCA");
		configButton(PARAM_DRIFT, "Toggle oscillator drift");
		configButton(PARAM_FLAT, "Toggle lower and higher frequency detuning");
		configButton(PARAM_SIGN, "Toggle output imperfections");
		configButton(PARAM_AUTO, "Toggle auto trigger");

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&osc[channel], 0, sizeof(renaissance::MacroOscillator));
			memset(&quantizer[channel], 0, sizeof(renaissance::Quantizer));
			memset(&envelope[channel], 0, sizeof(renaissance::Envelope));
			memset(&jitterSource[channel], 0, sizeof(renaissance::VcoJitterSource));
			memset(&waveShaper[channel], 0, sizeof(renaissance::SignatureWaveshaper));
			memset(&settings[channel], 0, sizeof(renaissance::SettingsData));

			osc[channel].Init();
			quantizer[channel].Init();
			envelope[channel].Init();

			jitterSource[channel].Init();
			waveShaper[channel].Init(0x0000);


			bLastTrig[channel] = false;

			currentScale[channel] = 0xff;

			previousPitch[channel] = 0;
		}
		memset(&lastSettings, 0, sizeof(renaissance::SettingsData));

		clockDivider.setDivision(kClockUpdateFrequency);
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(std::max(inputs[INPUT_PITCH].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

		bVCAEnabled = params[PARAM_VCA].getValue();
		bDritfEnabled = params[PARAM_DRIFT].getValue();
		bFlattenEnabled = params[PARAM_FLAT].getValue();
		bSignatureEnabled = params[PARAM_SIGN].getValue();
		bAutoTrigger = params[PARAM_AUTO].getValue();

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
			settings[channel].invert_encoder = false;

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
			settings[channel].vco_drift = bDritfEnabled;
			settings[channel].vco_flatten = bFlattenEnabled;
			settings[channel].signature = bSignatureEnabled;
			settings[channel].auto_trig = bAutoTrigger;

			// Quantizer
			if (currentScale[channel] != settings[channel].quantizer_scale) {
				currentScale[channel] = settings[channel].quantizer_scale;
				quantizer[channel].Configure(renaissance::scales[currentScale[channel]]);
			}

			// Render frames
			if (drbOutputBuffer[channel].empty()) {
				envelope[channel].Update(settings[channel].ad_attack * 8, settings[channel].ad_decay * 8);
				uint32_t adValue = envelope[channel].Render();

				float fm = params[PARAM_FM].getValue() * inputs[INPUT_FM].getVoltage(channel);

				// Set model
				int model = params[PARAM_MODEL].getValue();
				if (inputs[INPUT_META].isConnected()) {
					model += roundf(inputs[INPUT_META].getVoltage(channel) / 10.f * renaissance::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
				}

				settings[channel].shape = clamp(model, 0, renaissance::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);

				// Setup oscillator from settings
				osc[channel].set_shape(renaissance::MacroOscillatorShape(settings[channel].shape));

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
				case renaissance::PITCH_RANGE_EXTERNAL:
					//case renaissance::PITCH_RANGE_LFO:
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

				pitch = quantizer[channel].Process(pitch, (60 + settings[channel].quantizer_root) << 7);

				// Check if the pitch has changed to cause an auto-retrigger
				int16_t pitchDelta = pitch - previousPitch[channel];
				bFlagTriggerDetected[channel] = settings[channel].auto_trig && (pitchDelta >= 0x40 || -pitchDelta >= 0x40);

				previousPitch[channel] = pitch;

				pitch += jitterSource[channel].Render(settings[channel].vco_drift);
				pitch += adValue * settings[channel].ad_fm >> 7;

				pitch = clamp(static_cast<int16_t>(pitch), 0, 16383);

				if (settings[channel].vco_flatten) {
					pitch = renaissance::Interpolate88(renaissance::lut_vco_detune, pitch << 2);
				}

				// Pitch transposition
				int16_t transposition = 0;
				// TODO!!! Fix disabled LFO: disabled in hardware and prone to crashing Rack, probably needs firmware work.
				//transposition = settings[channel].pitch_range == renaissance::PITCH_RANGE_LFO ? -(36 << 7) : 0;
				transposition += (static_cast<int16_t>(settings[channel].pitch_octave) - 2) * 12 * 128;
				osc[channel].set_pitch(pitch + transposition);

				if (bTriggerFlag[channel]) {
					osc[channel].Strike();
					envelope[channel].Trigger(renaissance::ENV_SEGMENT_ATTACK);
					bTriggerFlag[channel] = false;
				}

				// TODO: add a sync input buffer (must be sample rate converted)
				uint8_t syncBuffer[kBlockSize] = {};

				int16_t renderBuffer[kBlockSize];
				osc[channel].Render(syncBuffer, renderBuffer, kBlockSize);

				// Signature waveshaping, decimation, and bit reduction
				int16_t sample = 0;
				size_t decimationFactor = nodiDecimationFactors[settings[channel].sample_rate];
				uint16_t bitMask = nodiBitReductionMasks[settings[channel].resolution];
				int32_t gain = settings[channel].ad_vca > 0 ? adValue : 65535;
				uint16_t signature = settings[channel].signature * settings[channel].signature * 4095;
				for (size_t block = 0; block < kBlockSize; ++block) {
					if ((block % decimationFactor) == 0) {
						sample = renderBuffer[block] & bitMask;
					}
					sample = sample * gainLp[channel] >> 16;
					gainLp[channel] += (gain - gainLp[channel]) >> 4;
					int16_t warped = waveShaper[channel].Transform(sample);
					renderBuffer[block] = stmlib::Mix(sample, warped, signature);
				}

				if (!bLowCpu) {
					// Sample rate convert
					dsp::Frame<1> in[kBlockSize];
					for (int block = 0; block < kBlockSize; ++block) {
						in[block].samples[0] = renderBuffer[block] / 32768.f;
					}
					sampleRateConverter[channel].setRates(96000, args.sampleRate);

					int inLen = kBlockSize;
					int outLen = drbOutputBuffer[channel].capacity();
					sampleRateConverter[channel].process(in, &inLen, drbOutputBuffer[channel].endData(), &outLen);
					drbOutputBuffer[channel].endIncr(outLen);
				} else {
					for (int block = 0; block < kBlockSize; ++block) {
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
			int currentModel = settings[0].shape;
			lights[LIGHT_MODEL + 0].setBrightnessSmooth(contextusLightColors[currentModel].red, sampleTime);
			lights[LIGHT_MODEL + 1].setBrightnessSmooth(contextusLightColors[currentModel].green, sampleTime);
			lights[LIGHT_MODEL + 2].setBrightnessSmooth(contextusLightColors[currentModel].blue, sampleTime);

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				int currentLight = LIGHT_CHANNEL_MODEL + channel * 3;

				for (int light = 0; light < 3; ++light) {
					lights[currentLight + light].setBrightnessSmooth(0.f, sampleTime);
				}

				if (channel < channelCount) {
					int selectedModel = settings[channel].shape;
					lights[currentLight + 0].setBrightnessSmooth(contextusLightColors[selectedModel].red, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(contextusLightColors[selectedModel].green, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(contextusLightColors[selectedModel].blue, sampleTime);
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
		if (lastSettingChanged == renaissance::SETTING_OSCILLATOR_SHAPE) {
			displayText = contextusDisplayLabels[settings[displayChannel].shape];
		} else {
			int value;
			switch (lastSettingChanged)
			{
			case renaissance::SETTING_RESOLUTION: {
				value = settings[displayChannel].resolution;
				displayText = nodiBitsStrings[value];
				break;
			}
			case renaissance::SETTING_SAMPLE_RATE: {
				value = settings[displayChannel].sample_rate;
				displayText = nodiRatesStrings[value];
				break;
			}
			case renaissance::SETTING_TRIG_SOURCE: {
				displayText = nodiAutoLabel;
				break;
			}
			case renaissance::SETTING_TRIG_DELAY: {
				value = settings[displayChannel].trig_delay;
				displayText = nodiTriggerDelayStrings[value];
				break;
			}
			case renaissance::SETTING_AD_ATTACK: {
				value = settings[displayChannel].ad_attack;
				displayText = nodiNumberStrings[value];
				break;
			}
			case renaissance::SETTING_AD_DECAY: {
				value = settings[displayChannel].ad_decay;
				displayText = nodiNumberStrings[value];
				break;
			}
			case renaissance::SETTING_AD_FM: {
				value = settings[displayChannel].ad_fm;
				displayText = nodiNumberStrings[value];
				break;
			}
			case renaissance::SETTING_AD_TIMBRE: {
				value = settings[displayChannel].ad_timbre;
				displayText = nodiNumberStrings[value];
				break;
			}
			case renaissance::SETTING_AD_COLOR: {
				value = settings[displayChannel].ad_color;
				displayText = nodiNumberStrings[value];
				break;
			}
			case renaissance::SETTING_AD_VCA: {
				displayText = nodiVCALabel;
				break;
			}
			case renaissance::SETTING_PITCH_RANGE: {
				value = settings[displayChannel].pitch_range;
				displayText = nodiPitchRangeStrings[value];
				break;
			}
			case renaissance::SETTING_PITCH_OCTAVE: {
				value = settings[displayChannel].pitch_octave;
				displayText = nodiOctaveStrings[value];
				break;
			}
			case renaissance::SETTING_QUANTIZER_SCALE: {
				value = settings[displayChannel].quantizer_scale;
				displayText = nodiQuantizationStrings[value];
				break;
			}
			case renaissance::SETTING_QUANTIZER_ROOT: {
				value = settings[displayChannel].quantizer_root;
				displayText = nodiNoteStrings[value];
				break;
			}
			case renaissance::SETTING_VCO_FLATTEN: {
				displayText = nodiFlatLabel;
				break;
			}
			case renaissance::SETTING_VCO_DRIFT: {
				displayText = nodiDriftLabel;
				break;
			}
			case renaissance::SETTING_SIGNATURE: {
				displayText = nodiSignLabel;
				break;
			}
			default: {
				break;
			}
			}

			++displayTimeout;
		}

		if (displayTimeout > args.sampleRate) {
			lastSettingChanged = renaissance::SETTING_OSCILLATOR_SHAPE;
			displayTimeout = 0;
		}

		uint8_t* arrayLastSettings = &lastSettings.shape;
		uint8_t* arraySettings = &settings[displayChannel].shape;
		for (int i = 0; i <= renaissance::SETTING_LAST_EDITABLE_SETTING; ++i) {
			if (arraySettings[i] != arrayLastSettings[i]) {
				arrayLastSettings[i] = arraySettings[i];
				lastSettingChanged = static_cast<renaissance::Setting>(i);
				displayTimeout = 0;
			}
		}
	}

	inline void pollSwitches(const float sampleTime) {
		// Handle switch lights
		lights[LIGHT_VCA].setBrightnessSmooth(bVCAEnabled ? 0.75f : 0.f, sampleTime);
		lights[LIGHT_DRIFT].setBrightnessSmooth(bDritfEnabled ? 0.75f : 0.f, sampleTime);
		lights[LIGHT_FLAT].setBrightnessSmooth(bFlattenEnabled ? 0.75f : 0.f, sampleTime);
		lights[LIGHT_SIGN].setBrightnessSmooth(bSignatureEnabled ? 0.75f : 0.f, sampleTime);
		lights[LIGHT_AUTO].setBrightnessSmooth(bAutoTrigger ? 0.75f : 0.f, sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_t* lowCpuJ = json_boolean(bLowCpu);
		json_object_set_new(rootJ, "bLowCpu", lowCpuJ);

		json_t* displayChannelJ = json_integer(displayChannel);
		json_object_set_new(rootJ, "displayChannel", displayChannelJ);

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
	}

	int getModelParam() {
		return params[PARAM_MODEL].getValue();
	}
	void setModelParam(int shape) {
		params[PARAM_MODEL].setValue(shape);
	}
};

struct ContextusWidget : SanguineModuleWidget {
	ContextusWidget(Contextus* module) {
		setModule(module);

		moduleName = "contextus";
		panelSize = SIZE_28;
		backplateColor = PLATE_RED;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* nodiFrambuffer = new FramebufferWidget();
		addChild(nodiFrambuffer);

		const float lightXBase = 5.256f;
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

		NodiDisplay* nodiDisplay = new NodiDisplay(4, module, 71.12, 20.996);
		nodiFrambuffer->addChild(nodiDisplay);
		nodiDisplay->fallbackString = contextusDisplayLabels[0];

		if (module) {
			nodiDisplay->values.displayText = &module->displayText;
			nodiDisplay->displayTimeout = &module->displayTimeout;
		}

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(106.234, 20.996), module, Contextus::INPUT_META));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(71.12, 67.247), module, Contextus::PARAM_MODEL));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(71.12, 67.247), module, Contextus::LIGHT_MODEL));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.222, 36.606), module, Contextus::INPUT_TIMBRE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(22.768, 36.606), module, Contextus::PARAM_TIMBRE));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.46, 40.534), module, Contextus::PARAM_COARSE));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.809, 40.534), module, Contextus::PARAM_FINE));


		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(119.474, 36.606), module, Contextus::PARAM_ATTACK));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(23.804, 54.231), module, Contextus::PARAM_AD_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(119.4, 54.231),
			module, Contextus::PARAM_VCA, Contextus::LIGHT_VCA));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(10.076, 67.247), module, Contextus::PARAM_MODULATION));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(36.032, 67.247), module, Contextus::PARAM_ROOT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(millimetersToPixelsVec(48.572, 80.197),
			module, Contextus::PARAM_DRIFT, Contextus::LIGHT_DRIFT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(93.673, 80.197),
			module, Contextus::PARAM_FLAT, Contextus::LIGHT_FLAT));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(106.234, 67.247), module, Contextus::PARAM_SCALE));
		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(132.166, 67.247), module, Contextus::PARAM_DECAY));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(23.804, 76.712), module, Contextus::PARAM_AD_COLOR));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(119.4, 76.712), module, Contextus::PARAM_AD_MODULATION));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(8.222, 97.889), module, Contextus::INPUT_COLOR));
		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(22.768, 97.889), module, Contextus::PARAM_COLOR));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.457, 93.965), module, Contextus::PARAM_PITCH_OCTAVE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<PurpleLight>>>(millimetersToPixelsVec(71.12, 93.962),
			module, Contextus::PARAM_SIGN, Contextus::LIGHT_SIGN));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.806, 93.965), module, Contextus::PARAM_PITCH_RANGE));

		addParam(createParamCentered<Sanguine1PSOrange>(millimetersToPixelsVec(119.474, 97.889), module, Contextus::PARAM_FM));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(133.968, 97.889), module, Contextus::INPUT_FM));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(8.222, 117.788), module, Contextus::INPUT_PITCH));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(21.722, 117.788), module, Contextus::INPUT_TRIGGER));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 117.788), module, Contextus::PARAM_TRIGGER_DELAY));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(46.798, 117.788),
			module, Contextus::PARAM_AUTO, Contextus::LIGHT_AUTO));
		addParam(createParamCentered<Sanguine1PSYellow>(millimetersToPixelsVec(62.4, 113.511), module, Contextus::PARAM_BITS));
		addParam(createParamCentered<Sanguine1PSYellow>(millimetersToPixelsVec(79.841, 113.511), module, Contextus::PARAM_RATE));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(133.968, 117.788), module, Contextus::OUTPUT_OUT));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 98.491, 110.323);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 111.492, 117.256);
		addChild(mutantsLogo);
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Contextus* module = dynamic_cast<Contextus*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < static_cast<int>(contextusDisplayLabels.size()); ++i) {
			modelLabels.push_back(contextusDisplayLabels[i] + ": " + contextusMenuLabels[i]);
		}
		menu->addChild(createIndexSubmenuItem("Model", modelLabels,
			[=]() {return module->getModelParam(); },
			[=](int i) {module->setModelParam(i); }
		));

		menu->addChild(new MenuSeparator);

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
	}
};

Model* modelContextus = createModel<Contextus, ContextusWidget>("Sanguine-Contextus");