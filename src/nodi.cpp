#include <string.h>

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "nodicommon.hpp"

#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "braids/envelope.h"
#include "braids/quantizer.h"
#include "braids/quantizer_scales.h"

#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

static const std::vector<NodiModelInfo> nodiModelInfos = {
	{"CSAW", "Quirky sawtooth"},
	{"/\\-_", "Triangle to saw"},
	{"//-_", "Sawtooth wave with dephasing"},
	{"FOLD", "Wavefolded sine/triangle"},
	{"uuuu", "Buzz"},
	{"SUB-", "Square sub"},
	{"SUB/", "Saw sub"},
	{"SYN-", "Square sync"},
	{"SYN/", "Saw sync"},
	{"//x3", "Triple saw"},
	{"-_x3", "Triple square"},
	{"/\\x3", "Triple triangle"},
	{"SIx3", "Triple sine"},
	{"RING", "Triple ring mod"},
	{"////", "Saw swarm"},
	{"//uu", "Saw comb"},
	{"TOY*", "Circuit-bent toy"},
	{"ZLPF", "Low-pass filtered waveform"},
	{"ZPKF", "Peak filtered waveform"},
	{"ZBPF", "Band-pass filtered waveform"},
	{"ZHPF", "High-pass filtered waveform"},
	{"VOSM", "VOSIM formant"},
	{"VOWL", "Speech synthesis"},
	{"VFOF", "FOF speech synthesis"},
	{"HARM", "12 sine harmonics"},
	{"FM  ", "2-operator phase-modulation"},
	{"FBFM", "2-operator phase-modulation with feedback"},
	{"WTFM", "2-operator phase-modulation with chaotic feedback"},
	{"PLUK", "Plucked string"},
	{"BOWD", "Bowed string"},
	{"BLOW", "Blown reed"},
	{"FLUT", "Flute"},
	{"BELL", "Bell"},
	{"DRUM", "Drum"},
	{"KICK", "Kick drum circuit simulation"},
	{"CYMB", "Cymbal"},
	{"SNAR", "Snare"},
	{"WTBL", "Wavetable"},
	{"WMAP", "2D wavetable"},
	{"WLIN", "1D wavetable"},
	{"WTx4", "4-voice paraphonic 1D wavetable"},
	{"NOIS", "Filtered noise"},
	{"TWNQ", "Twin peaks noise"},
	{"CLKN", "Clocked noise"},
	{"CLOU", "Granular cloud"},
	{"PRTC", "Particle noise"},
	{"QPSK", "Digital modulation"},
	{"  49", "Paques morse code"}, // "Paques"
};

static const RGBLightColor nodiLightColors[48]{
{1.f, 0.f, 0.f},
{0.9375f, 0.f, 0.0625f},
{0.875f, 0.f, 0.125f},
{0.8125f, 0.f, 0.1875f},
{0.75f, 0.f, 0.25f},
{0.6875f, 0.f, 0.3125f},
{0.625f, 0.f, 0.375f},
{0.5625f, 0.f, 0.4375f},
{0.5f, 0.f, 0.5f},
{0.4375f, 0.f, 0.5625f},
{0.375f, 0.f, 0.625f},
{0.3125f, 0.f, 0.6875f},
{0.25f, 0.f, 0.75f},
{0.1875f, 0.f, 0.8125f},
{0.125f, 0.f, 0.875f},
{0.0625f, 0.f, 0.9375f},
{0.f, 0.f, 1.f},
{0.f, 0.125f, 0.875f},
{0.f, 0.1875f, 0.8125f},
{0.f, 0.25f, 0.75f},
{0.f, 0.3125f, 0.6875f},
{0.f, 0.375f, 0.625f},
{0.f, 0.4375f, 0.5625f},
{0.f, 0.5f, 0.5f},
{0.f, 0.5625f, 0.4375f},
{0.f, 0.625f, 0.375f},
{0.f, 0.6875f, 0.3125f},
{0.f, 0.75f, 0.25f},
{0.f, 0.8125f, 0.1875f},
{0.f, 0.875f, 0.125f},
{0.f, 0.9375f, 0.0625f},
{0.f,1.f, 0.f},
{0.0625f, 0.9375f, 0.f},
{0.125f, 0.875f, 0.f},
{0.1875f, 0.8125f, 0.f},
{0.25f, 0.75f, 0.f},
{0.3125f, 0.6875f, 0.f},
{0.375f, 0.625f, 0.f},
{0.4375f, 0.5625f, 0.f},
{0.5f, 0.5f, 0.f},
{0.5625f, 0.4375f, 0.f},
{0.625f, 0.375f, 0.f},
{0.6875f, 0.3125f, 0.f},
{0.75f, 0.25f, 0.f},
{0.8125f, 0.1875f, 0.f},
{0.875f, 0.125f, 0.f},
{0.9375f, 0.0625f, 0.f},
{1.f, 1.f, 1.f}
};

struct Nodi : Module {
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

		PARAM_META,
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
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_OUT,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_MODEL, 1 * 3),
		LIGHT_META,
		LIGHT_MORSE,
		LIGHT_VCA,
		LIGHT_DRIFT,
		LIGHT_FLAT,
		LIGHT_SIGN,
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

	uint8_t currentScale[PORT_MAX_CHANNELS];

	int16_t previousPitch[PORT_MAX_CHANNELS];

	uint16_t gainLp[PORT_MAX_CHANNELS];
	uint16_t triggerDelay[PORT_MAX_CHANNELS];

	int channelCount;

	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> drbOutputBuffer[PORT_MAX_CHANNELS];
	dsp::SampleRateConverter<1> sampleRateConverter[PORT_MAX_CHANNELS];

	static const int kClockUpdateFrequency = 16;
	dsp::ClockDivider clockDivider;

	bool bFlagTriggerDetected[PORT_MAX_CHANNELS];
	bool bLastTrig[PORT_MAX_CHANNELS];
	bool bTriggerFlag[PORT_MAX_CHANNELS];

	bool bAutoTrigger = false;
	bool bDritfEnabled = false;
	bool bFlattenEnabled = false;
	bool bMetaModulation = false;
	bool bPaques = false;
	bool bSignatureEnabled = false;
	bool bVCAEnabled = false;

	bool bLowCpu = false;

	// Display stuff	
	braids::SettingsData lastSettings;
	braids::Setting lastSettingChanged;

	uint32_t displayTimeout = 0;

	std::string displayText;

	Nodi() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_MODEL, 0.f, braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META, 0.f, "Model", "", 0.f, 1.f, 1.f);
		paramQuantities[PARAM_MODEL]->snapEnabled = true;
		configParam(PARAM_MODULATION, -1.0, 1.0, 0.0, "Modulation");
		configParam(PARAM_COARSE, -5.0, 3.0, -1.0, "Coarse frequency", " semitones", 0.f, 12.f, 12.f);
		configParam(PARAM_FINE, -1.0, 1.0, 0.0, "Fine frequency", " semitones");
		configParam(PARAM_ATTACK, 0.0, 15.0, 0.0, "Attack");
		paramQuantities[PARAM_ATTACK]->snapEnabled = true;

		configParam(PARAM_AD_TIMBRE, 0.0, 15.f, 0.0, "Timbre AD");
		paramQuantities[PARAM_AD_TIMBRE]->snapEnabled = true;

		configParam(PARAM_TIMBRE, 0.0, 1.0, 0.5, "Timbre", "%", 0.f, 100.f);
		configParam(PARAM_ROOT, 0.0, 11.0, 0.0, "Quantizer root note", "", 0.f, 1.0, 1.f);
		paramQuantities[PARAM_ROOT]->snapEnabled = true;
		configParam(PARAM_SCALE, 0.0, 48.0, 0.0, "Quantizer scale");
		paramQuantities[PARAM_SCALE]->snapEnabled = true;
		configParam(PARAM_DECAY, 0.0, 15.f, 7.0, "Decay");
		paramQuantities[PARAM_DECAY]->snapEnabled = true;

		configParam(PARAM_AD_COLOR, 0.f, 15.f, 0.f, "Color AD");
		paramQuantities[PARAM_AD_COLOR]->snapEnabled = true;
		configParam(PARAM_AD_MODULATION, 0.f, 15.f, 0.f, "FM AD");
		paramQuantities[PARAM_AD_MODULATION]->snapEnabled = true;

		configParam(PARAM_COLOR, 0.0, 1.0, 0.5, "Color", "%", 0.f, 100.f);
		configParam(PARAM_PITCH_OCTAVE, 0.0, 4.0, 2.f, "Octave", "", 0.f, 1.f, -2.f);
		paramQuantities[PARAM_PITCH_OCTAVE]->snapEnabled = true;
		configParam(PARAM_PITCH_RANGE, 0.f, 4.f, 0.f, "Pitch range");
		paramQuantities[PARAM_PITCH_RANGE]->snapEnabled = true;
		configParam(PARAM_FM, -1.0, 1.0, 0.0, "FM");

		configParam(PARAM_TRIGGER_DELAY, 0.f, 6.f, 0.f, "Trigger delay");
		paramQuantities[PARAM_TRIGGER_DELAY]->snapEnabled = true;

		configParam(PARAM_BITS, 0.f, 6.f, 6.f, "Bit crusher resolution");
		paramQuantities[PARAM_BITS]->snapEnabled = true;
		configParam(PARAM_RATE, 0.f, 6.0f, 6.0f, "Bit crusher sample rate");
		paramQuantities[PARAM_RATE]->snapEnabled = true;

		configInput(INPUT_TRIGGER, "Trigger");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");
		configInput(INPUT_FM, "FM");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_COLOR, "Color");
		configOutput(OUTPUT_OUT, "Audio");

		configButton(PARAM_META, "Toggle meta modulation");
		configButton(PARAM_MORSE, "Toggle paques (morse code secret message)");
		configButton(PARAM_VCA, "Toggle AD VCA");
		configButton(PARAM_DRIFT, "Toggle oscillator drift");
		configButton(PARAM_FLAT, "Toggle lower and higher frequency detuning");
		configButton(PARAM_SIGN, "Toggle output imperfections");
		configButton(PARAM_AUTO, "Toggle auto trigger");

		for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
			memset(&osc[i], 0, sizeof(braids::MacroOscillator));
			osc[i].Init();
			memset(&quantizer[i], 0, sizeof(braids::Quantizer));
			quantizer[i].Init();
			memset(&envelope[i], 0, sizeof(braids::Envelope));
			envelope[i].Init();

			memset(&jitterSource[i], 0, sizeof(braids::VcoJitterSource));
			jitterSource[i].Init();
			memset(&waveShaper[i], 0, sizeof(braids::SignatureWaveshaper));
			waveShaper[i].Init(0x0000);
			memset(&settings[i], 0, sizeof(braids::SettingsData));

			bLastTrig[i] = false;

			currentScale[i] = 0xff;

			previousPitch[i] = 0;
		}

		clockDivider.setDivision(kClockUpdateFrequency);
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(std::max(inputs[INPUT_PITCH].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

		bMetaModulation = params[PARAM_META].getValue();
		bVCAEnabled = params[PARAM_VCA].getValue();
		bDritfEnabled = params[PARAM_DRIFT].getValue();
		bFlattenEnabled = params[PARAM_FLAT].getValue();
		bSignatureEnabled = params[PARAM_SIGN].getValue();
		bAutoTrigger = params[PARAM_AUTO].getValue();
		bPaques = params[PARAM_MORSE].getValue();

		outputs[OUTPUT_OUT].setChannels(channelCount);

		for (int channel = 0; channel < channelCount; channel++) {
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
			bool bTriggerInput = inputs[INPUT_TRIGGER].getVoltage(channel) >= 1.0;
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
			settings[channel].meta_modulation = bMetaModulation;
			settings[channel].ad_vca = bVCAEnabled;
			settings[channel].vco_drift = bDritfEnabled;
			settings[channel].vco_flatten = bFlattenEnabled;
			settings[channel].signature = bSignatureEnabled;
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
					if (settings[channel].meta_modulation) {
						model += roundf(fm / 10.0 * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
					}
					settings[channel].shape = clamp(model, 0, braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);

					// Setup oscillator from settings
					osc[channel].set_shape(braids::MacroOscillatorShape(settings[channel].shape));
				}
				else {
					osc[channel].set_shape(braids::MACRO_OSC_SHAPE_QUESTION_MARK);
				}

				// Set timbre/modulation
				float timbre = params[PARAM_TIMBRE].getValue() + params[PARAM_MODULATION].getValue() * inputs[INPUT_TIMBRE].getVoltage(channel) / 5.0;
				float modulation = params[PARAM_COLOR].getValue() + inputs[INPUT_COLOR].getVoltage(channel) / 5.0;

				timbre += adValue / 65535. * settings[channel].ad_timbre / 16.;
				modulation += adValue / 65535. * settings[channel].ad_color / 16.;

				int16_t param1 = math::rescale(clamp(timbre, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
				int16_t param2 = math::rescale(clamp(modulation, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
				osc[channel].set_parameters(param1, param2);

				// Set pitch
				float pitchV = inputs[INPUT_PITCH].getVoltage(channel) + params[PARAM_COARSE].getValue() + params[PARAM_FINE].getValue() / 12.0;
				if (!settings[channel].meta_modulation)
					pitchV += fm;
				if (bLowCpu)
					pitchV += log2f(96000.0 / args.sampleRate);
				int32_t pitch = (pitchV * 12.0 + 60) * 128;

				// pitch_range
				if (settings[channel].pitch_range == braids::PITCH_RANGE_EXTERNAL || settings[channel].pitch_range == braids::PITCH_RANGE_LFO) {
				}
				else if (settings[channel].pitch_range == braids::PITCH_RANGE_FREE) {
					pitch = pitch - 1638;
				}
				else if (settings[channel].pitch_range == braids::PITCH_RANGE_440) {
					pitch = 69 << 7;
				}
				else { // PITCH_RANGE_EXTENDED
					pitch -= 60 << 7;
					pitch = (pitch - 1638) * 9 >> 1;
					pitch += 60 << 7;
				}

				pitch = quantizer[channel].Process(pitch, (60 + settings[channel].quantizer_root) << 7);

				// Check if the pitch has changed to cause an auto-retrigger
				int32_t pitch_delta = pitch - previousPitch[channel];
				if (settings[channel].auto_trig && (pitch_delta >= 0x40 || -pitch_delta >= 0x40)) {
					bFlagTriggerDetected[channel] = true;
				}
				previousPitch[channel] = pitch;

				pitch += jitterSource[channel].Render(settings[channel].vco_drift);
				pitch += adValue * settings[channel].ad_fm >> 7;

				pitch = clamp(int(pitch), 0, 16383);

				if (settings[channel].vco_flatten) {
					pitch = braids::Interpolate88(braids::lut_vco_detune, pitch << 2);
				}

				// Pitch transposition
				int32_t t = settings[channel].pitch_range == braids::PITCH_RANGE_LFO ? -(36 << 7) : 0;
				t += (static_cast<int32_t>(settings[channel].pitch_octave) - 2) * 12 * 128;
				osc[channel].set_pitch(pitch + t);

				if (bTriggerFlag[channel]) {
					osc[channel].Strike();
					envelope[channel].Trigger(braids::ENV_SEGMENT_ATTACK);
					bTriggerFlag[channel] = false;
				}

				// TODO: add a sync input buffer (must be sample rate converted)
				uint8_t syncBuffer[24] = {};

				int16_t renderBuffer[24];
				osc[channel].Render(syncBuffer, renderBuffer, 24);

				// Signature waveshaping, decimation, and bit reduction
				int16_t sample = 0;
				size_t decimationFactor = nodiDecimationFactors[settings[channel].sample_rate];
				uint16_t bitMask = nodiBitReductionMasks[settings[channel].resolution];
				int32_t gain = settings[channel].ad_vca > 0 ? adValue : 65535;
				uint16_t signature = settings[channel].signature * settings[channel].signature * 4095;
				for (size_t i = 0; i < 24; i++) {
					if ((i % decimationFactor) == 0) {
						sample = renderBuffer[i] & bitMask;
					}
					sample = sample * gainLp[channel] >> 16;
					gainLp[channel] += (gain - gainLp[channel]) >> 4;
					int16_t warped = waveShaper[channel].Transform(sample);
					renderBuffer[i] = stmlib::Mix(sample, warped, signature);
				}

				if (bLowCpu) {
					for (int i = 0; i < 24; i++) {
						dsp::Frame<1> f;
						f.samples[0] = renderBuffer[i] / 32768.0;
						drbOutputBuffer[channel].push(f);
					}
				}
				else {
					// Sample rate convert
					dsp::Frame<1> in[24];
					for (int i = 0; i < 24; i++) {
						in[i].samples[0] = renderBuffer[i] / 32768.0;
					}
					sampleRateConverter[channel].setRates(96000, args.sampleRate);

					int inLen = 24;
					int outLen = drbOutputBuffer[channel].capacity();
					sampleRateConverter[channel].process(in, &inLen, drbOutputBuffer[channel].endData(), &outLen);
					drbOutputBuffer[channel].endIncr(outLen);
				}
			}

			// Output
			if (!drbOutputBuffer[channel].empty()) {
				dsp::Frame<1> f = drbOutputBuffer[channel].shift();
				outputs[OUTPUT_OUT].setVoltage(5.0 * f.samples[0], channel);
			}
		} // Channels

		if (clockDivider.process()) {
			const float sampleTime = args.sampleTime * kClockUpdateFrequency;
			pollSwitches(sampleTime);

			// Handle model light
			if (!bPaques) {
				int currentModel = settings[0].shape;
				lights[LIGHT_MODEL + 0].setBrightnessSmooth(nodiLightColors[currentModel].red, sampleTime);
				lights[LIGHT_MODEL + 1].setBrightnessSmooth(nodiLightColors[currentModel].green, sampleTime);
				lights[LIGHT_MODEL + 2].setBrightnessSmooth(nodiLightColors[currentModel].blue, sampleTime);
			}
			else {
				lights[LIGHT_MODEL + 0].setBrightnessSmooth(nodiLightColors[47].red, sampleTime);
				lights[LIGHT_MODEL + 1].setBrightnessSmooth(nodiLightColors[47].green, sampleTime);
				lights[LIGHT_MODEL + 2].setBrightnessSmooth(nodiLightColors[47].blue, sampleTime);
			}

			for (int i = 0; i < PORT_MAX_CHANNELS; i++) {
				int currentLight = LIGHT_CHANNEL_MODEL + i * 3;
				int currentModel = settings[i].shape;
				if (i < channelCount) {
					if (!bPaques) {
						lights[currentLight + 0].setBrightnessSmooth(nodiLightColors[currentModel].red, sampleTime);
						lights[currentLight + 1].setBrightnessSmooth(nodiLightColors[currentModel].green, sampleTime);
						lights[currentLight + 2].setBrightnessSmooth(nodiLightColors[currentModel].blue, sampleTime);
					}
					else {
						lights[currentLight + 0].setBrightnessSmooth(nodiLightColors[47].red, sampleTime);
						lights[currentLight + 1].setBrightnessSmooth(nodiLightColors[47].green, sampleTime);
						lights[currentLight + 2].setBrightnessSmooth(nodiLightColors[47].blue, sampleTime);
					}
				}
				else {
					lights[currentLight + 0].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 1].setBrightnessSmooth(0.f, sampleTime);
					lights[currentLight + 2].setBrightnessSmooth(0.f, sampleTime);
				}
			}
		}

		handleDisplay(args);
	}

	inline void handleDisplay(const ProcessArgs& args) {
		// Display handling
		// Display - return to model after 2s
		if (lastSettingChanged == braids::SETTING_OSCILLATOR_SHAPE) {
			if (!bPaques)
			{
				displayText = nodiModelInfos[settings[0].shape].code;
			}
			else
			{
				displayText = nodiModelInfos[47].code;
			}
		}
		else {
			int value;
			switch (lastSettingChanged)
			{
			case braids::SETTING_META_MODULATION: {
				displayText = nodiMetaLabel;
				break;
			}
			case braids::SETTING_RESOLUTION: {
				value = settings[0].resolution;
				displayText = nodiBitsStrings[value];
				break;
			}
			case braids::SETTING_SAMPLE_RATE: {
				value = settings[0].sample_rate;
				displayText = nodiRatesStrings[value];
				break;
			}
			case braids::SETTING_TRIG_SOURCE: {
				displayText = nodiAutoLabel;
				break;
			}
			case braids::SETTING_TRIG_DELAY: {
				value = settings[0].trig_delay;
				displayText = nodiTriggerDelayStrings[value];
				break;
			}
			case braids::SETTING_AD_ATTACK: {
				value = settings[0].ad_attack;
				displayText = nodiNumberStrings[value];
				break;
			}
			case braids::SETTING_AD_DECAY: {
				value = settings[0].ad_decay;
				displayText = nodiNumberStrings[value];
				break;
			}
			case braids::SETTING_AD_FM: {
				value = settings[0].ad_fm;
				displayText = nodiNumberStrings[value];
				break;
			}
			case braids::SETTING_AD_TIMBRE: {
				value = settings[0].ad_timbre;
				displayText = nodiNumberStrings[value];
				break;
			}
			case braids::SETTING_AD_COLOR: {
				value = settings[0].ad_color;
				displayText = nodiNumberStrings[value];
				break;
			}
			case braids::SETTING_AD_VCA: {
				displayText = nodiVCALabel;
				break;
			}
			case braids::SETTING_PITCH_RANGE: {
				value = settings[0].pitch_range;
				displayText = nodiPitchRangeStrings[value];
				break;
			}
			case braids::SETTING_PITCH_OCTAVE: {
				value = settings[0].pitch_octave;
				displayText = nodiOctaveStrings[value];
				break;
			}
			case braids::SETTING_QUANTIZER_SCALE: {
				value = settings[0].quantizer_scale;
				displayText = nodiQuantizationStrings[value];
				break;
			}
			case braids::SETTING_QUANTIZER_ROOT: {
				value = settings[0].quantizer_root;
				displayText = nodiNoteStrings[value];
				break;
			}
			case braids::SETTING_VCO_FLATTEN: {
				displayText = nodiFlatLabel;
				break;
			}
			case braids::SETTING_VCO_DRIFT: {
				displayText = nodiDriftLabel;
				break;
			}
			case braids::SETTING_SIGNATURE: {
				displayText = nodiSignLabel;
				break;
			}
			default: {
				break;
			}
			}

			displayTimeout++;
		}

		if (displayTimeout > args.sampleRate) {
			lastSettingChanged = braids::SETTING_OSCILLATOR_SHAPE;
			displayTimeout = 0;
		}

		uint8_t* arrayLastSettings = &lastSettings.shape;
		uint8_t* arraySettings = &settings[0].shape;
		for (int i = 0; i <= braids::SETTING_LAST_EDITABLE_SETTING; i++) {
			if (arraySettings[i] != arrayLastSettings[i]) {
				arrayLastSettings[i] = arraySettings[i];
				lastSettingChanged = static_cast<braids::Setting>(i);
				displayTimeout = 0;
			}
		}
	}

	inline void pollSwitches(const float sampleTime) {
		// Handle switch lights
		lights[LIGHT_META].setBrightnessSmooth(bMetaModulation, sampleTime);
		lights[LIGHT_MORSE].setBrightnessSmooth(bPaques, sampleTime);
		lights[LIGHT_VCA].setBrightnessSmooth(bVCAEnabled, sampleTime);
		lights[LIGHT_DRIFT].setBrightnessSmooth(bDritfEnabled, sampleTime);
		lights[LIGHT_FLAT].setBrightnessSmooth(bFlattenEnabled, sampleTime);
		lights[LIGHT_SIGN].setBrightnessSmooth(bSignatureEnabled, sampleTime);
		lights[LIGHT_AUTO].setBrightnessSmooth(bAutoTrigger, sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* lowCpuJ = json_boolean(bLowCpu);
		json_object_set_new(rootJ, "bLowCpu", lowCpuJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* lowCpuJ = json_object_get(rootJ, "bLowCpu");
		if (lowCpuJ) {
			bLowCpu = json_boolean_value(lowCpuJ);
		}
	}

	int getModelParam() {
		return params[PARAM_MODEL].getValue();
	}
	void setModelParam(int shape) {
		params[PARAM_MODEL].setValue(shape);
	}
};

struct NodiWidget : ModuleWidget {
	NodiWidget(Nodi* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_28hp_purple.svg", "res/nodi_faceplate.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* nodiFrambuffer = new FramebufferWidget();
		addChild(nodiFrambuffer);

		const float lightXBase = 5.256f;
		const float lightXDelta = 4.0f;

		const int offset = 8;
		float currentX = lightXBase;
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 20.308), module, Nodi::LIGHT_CHANNEL_MODEL + i * 3));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(millimetersToPixelsVec(currentX, 24.308), module, Nodi::LIGHT_CHANNEL_MODEL + ((i + offset) * 3)));
			currentX += lightXDelta;
		}

		NodiDisplay* nodiDisplay = new NodiDisplay(4, module, 71.12, 20.996);
		nodiFrambuffer->addChild(nodiDisplay);
		nodiDisplay->fallbackString = nodiModelInfos[0].code;

		if (module) {
			nodiDisplay->values.displayText = &module->displayText;
			nodiDisplay->displayTimeout = &module->displayTimeout;
		}

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(105.031, 20.996),
			module, Nodi::PARAM_META, Nodi::LIGHT_META));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(71.12, 67.247), module, Nodi::PARAM_MODEL));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(71.12, 67.247),
			module, Nodi::LIGHT_MODEL));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(8.222, 36.606), module, Nodi::INPUT_TIMBRE));
		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(22.768, 36.606), module, Nodi::PARAM_TIMBRE));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.46, 40.534), module, Nodi::PARAM_COARSE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(millimetersToPixelsVec(71.12, 42.184),
			module, Nodi::PARAM_MORSE, Nodi::LIGHT_MORSE));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.809, 40.534), module, Nodi::PARAM_FINE));


		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(119.474, 36.606), module, Nodi::PARAM_ATTACK));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(23.804, 54.231), module, Nodi::PARAM_AD_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(119.4, 54.231),
			module, Nodi::PARAM_VCA, Nodi::LIGHT_VCA));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(10.076, 67.247), module, Nodi::PARAM_MODULATION));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(36.032, 67.247), module, Nodi::PARAM_ROOT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(millimetersToPixelsVec(48.572, 80.197),
			module, Nodi::PARAM_DRIFT, Nodi::LIGHT_DRIFT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(millimetersToPixelsVec(93.673, 80.197),
			module, Nodi::PARAM_FLAT, Nodi::LIGHT_FLAT));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(106.234, 67.247), module, Nodi::PARAM_SCALE));
		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(132.166, 67.247), module, Nodi::PARAM_DECAY));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(23.804, 76.712), module, Nodi::PARAM_AD_COLOR));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(119.4, 76.712), module, Nodi::PARAM_AD_MODULATION));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(8.222, 97.889), module, Nodi::INPUT_COLOR));
		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(22.768, 97.889), module, Nodi::PARAM_COLOR));

		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(51.457, 93.965), module, Nodi::PARAM_PITCH_OCTAVE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<PurpleLight>>>(millimetersToPixelsVec(71.12, 93.962),
			module, Nodi::PARAM_SIGN, Nodi::LIGHT_SIGN));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(90.806, 93.965), module, Nodi::PARAM_PITCH_RANGE));

		addParam(createParamCentered<Sanguine1PSOrange>(millimetersToPixelsVec(119.474, 97.889), module, Nodi::PARAM_FM));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(133.968, 97.889), module, Nodi::INPUT_FM));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(8.222, 117.788), module, Nodi::INPUT_PITCH));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(21.722, 117.788), module, Nodi::INPUT_TRIGGER));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(35.151, 117.788), module, Nodi::PARAM_TRIGGER_DELAY));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(46.798, 117.788), module, Nodi::PARAM_AUTO, Nodi::LIGHT_AUTO));
		addParam(createParamCentered<Sanguine1PSYellow>(millimetersToPixelsVec(62.4, 113.511), module, Nodi::PARAM_BITS));
		addParam(createParamCentered<Sanguine1PSYellow>(millimetersToPixelsVec(79.841, 113.511), module, Nodi::PARAM_RATE));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(133.968, 117.788), module, Nodi::OUTPUT_OUT));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 98.491, 110.323);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 111.492, 117.256);
		addChild(mutantsLogo);
	}

	void appendContextMenu(Menu* menu) override {
		Nodi* module = dynamic_cast<Nodi*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < int(nodiModelInfos.size() - 1); i++) {
			modelLabels.push_back(nodiModelInfos[i].code + ": " + nodiModelInfos[i].label);
		}
		menu->addChild(createIndexSubmenuItem("Model", modelLabels,
			[=]() {return module->getModelParam(); },
			[=](int i) {module->setModelParam(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createCheckMenuItem("Low CPU (disable resampling)", "",
			[=]() {return module->bLowCpu; },
			[=]() {module->bLowCpu = !module->bLowCpu; }
		));
	}
};

Model* modelNodi = createModel<Nodi, NodiWidget>("Sanguine-Nodi");
