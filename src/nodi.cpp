#include <string.h>

#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "nodiconsts.hpp"

#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "braids/envelope.h"
#include "braids/quantizer.h"
#include "braids/quantizer_scales.h"

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
		LIGHTS_COUNT
	};

	struct RGBColor {
		float red;
		float green;
		float blue;
	};

	const RGBColor lightColors[48]{
		{1, 0, 0},
		{0.9375, 0, 0.0625},
		{0.875, 0, 0.125},
		{0.8125, 0, 0.1875},
		{0.75, 0, 0.25},
		{0.6875, 0, 0.3125},
		{0.625, 0, 0.375},
		{0.5625, 0, 0.4375},
		{0.5, 0, 0.5},
		{0.4375, 0, 0.5625},
		{0.375, 0, 0.625},
		{0.3125, 0, 0.6875},
		{0.25, 0, 0.75},
		{0.1875, 0, 0.8125},
		{0.125, 0, 0.875},
		{0.0625, 0, 0.9375},
		{0, 0.0625, 0.9375},
		{0, 0.125, 0.875},
		{0, 0.1875, 0.8125},
		{0, 0.25, 0.75},
		{0, 0.3125, 0.6875},
		{0, 0.375, 0.625},
		{0, 0.4375, 0.5625},
		{0, 0.5, 0.5},
		{0, 0.5625, 0.4375},
		{0, 0.625, 0.375},
		{0, 0.6875, 0.3125},
		{0, 0.75, 0.25},
		{0, 0.8125, 0.1875},
		{0, 0.875, 0.125},
		{0, 0.9375, 0.0625},
		{0,1, 0},
		{0.0625, 0.9375, 0},
		{0.125, 0.875, 0},
		{0.1875, 0.8125, 0},
		{0.25, 0.75, 0},
		{0.3125, 0.6875, 0},
		{0.375, 0.625, 0},
		{0.4375, 0.5625, 0},
		{0.5, 0.5, 0},
		{0.5625, 0.4375, 0},
		{0.625, 0.375, 0},
		{0.6875, 0.3125, 0},
		{0.75, 0.25, 0},
		{0.8125, 0.1875, 0},
		{0.875, 0.125, 0},
		{0.9375, 0.0625, 0},
		{ 1.f, 1.f, 1.f }
	};

	braids::MacroOscillator osc;
	braids::SettingsData settings;
	braids::VcoJitterSource jitterSource;
	braids::SignatureWaveshaper waveShaper;
	braids::Envelope envelope;
	braids::Quantizer quantizer;

	uint8_t currentScale = 0xff;

	int16_t previousPitch = 0;

	uint16_t gainLp;
	uint16_t triggerDelay;

	const uint16_t bitReductionMasks[7] = {
		0xc000,
		0xe000,
		0xf000,
		0xf800,
		0xff00,
		0xfff0,
		0xffff
	};

	const uint16_t decimationFactors[7] = {
		24,
		12,
		6,
		4,
		3,
		2,
		1
	};

	dsp::DoubleRingBuffer<dsp::Frame<1>, 256> drbOutputBuffer;
	dsp::SampleRateConverter<1> sampleRateConverter;

	static const int kClockUpdateFrequency = 16;
	dsp::ClockDivider clockDivider;

	bool bFlagTriggerDetected;
	bool bLastTrig = false;
	bool bLowCpu = false;
	bool bPaques = false;
	bool bTriggerFlag;

	// Display stuff
	braids::SettingsData lastSettings;
	braids::Setting lastSettingChanged;

	uint32_t displayTimeout = 0;

	std::string displayText;

	Nodi() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configParam(PARAM_MODEL, 0.f, 1.f, 0.f, "Model", "", 0.f, braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
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

		memset(&osc, 0, sizeof(osc));
		osc.Init();
		memset(&quantizer, 0, sizeof(quantizer));
		quantizer.Init();
		memset(&envelope, 0, sizeof(envelope));
		envelope.Init();

		memset(&jitterSource, 0, sizeof(jitterSource));
		jitterSource.Init();
		memset(&waveShaper, 0, sizeof(waveShaper));
		waveShaper.Init(0x0000);
		memset(&settings, 0, sizeof(settings));

		clockDivider.setDivision(kClockUpdateFrequency);
	}

	void process(const ProcessArgs& args) override {
		settings.quantizer_scale = params[PARAM_SCALE].getValue();
		settings.quantizer_root = params[PARAM_ROOT].getValue();
		settings.pitch_range = params[PARAM_PITCH_RANGE].getValue();
		settings.pitch_octave = params[PARAM_PITCH_OCTAVE].getValue();
		settings.trig_delay = params[PARAM_TRIGGER_DELAY].getValue();
		settings.sample_rate = params[PARAM_RATE].getValue();
		settings.resolution = params[PARAM_BITS].getValue();
		settings.ad_attack = params[PARAM_ATTACK].getValue();
		settings.ad_decay = params[PARAM_DECAY].getValue();
		settings.ad_timbre = params[PARAM_AD_TIMBRE].getValue();
		settings.ad_fm = params[PARAM_AD_MODULATION].getValue();
		settings.ad_color = params[PARAM_AD_COLOR].getValue();

		// Trigger
		bool bTriggerInput = inputs[INPUT_TRIGGER].getVoltage() >= 1.0;
		if (!bLastTrig && bTriggerInput) {
			bFlagTriggerDetected = bTriggerInput;
		}
		bLastTrig = bTriggerInput;

		if (bFlagTriggerDetected) {
			triggerDelay = settings.trig_delay ? (1 << settings.trig_delay) : 0;
			++triggerDelay;
			bFlagTriggerDetected = false;
		}
		if (triggerDelay) {
			--triggerDelay;
			if (triggerDelay == 0) {
				bTriggerFlag = true;
			}
		}

		// Quantizer
		if (currentScale != settings.quantizer_scale) {
			currentScale = settings.quantizer_scale;
			quantizer.Configure(braids::scales[currentScale]);
		}

		// Render frames
		if (drbOutputBuffer.empty()) {
			envelope.Update(settings.ad_attack * 8, settings.ad_decay * 8);
			uint32_t adValue = envelope.Render();

			float fm = params[PARAM_FM].getValue() * inputs[INPUT_FM].getVoltage();

			// Set shape
			if (!bPaques) {
				int shape = roundf(params[PARAM_MODEL].getValue() * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
				if (settings.meta_modulation) {
					shape += roundf(fm / 10.0 * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
				}
				settings.shape = clamp(shape, 0, braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);

				// Setup oscillator from settings
				osc.set_shape((braids::MacroOscillatorShape)settings.shape);
			}
			else {
				osc.set_shape(braids::MACRO_OSC_SHAPE_QUESTION_MARK);
			}

			// Set timbre/modulation
			float timbre = params[PARAM_TIMBRE].getValue() + params[PARAM_MODULATION].getValue() * inputs[INPUT_TIMBRE].getVoltage() / 5.0;
			float modulation = params[PARAM_COLOR].getValue() + inputs[INPUT_COLOR].getVoltage() / 5.0;

			timbre += adValue / 65535. * settings.ad_timbre / 16.;
			modulation += adValue / 65535. * settings.ad_color / 16.;

			int16_t param1 = rescale(clamp(timbre, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
			int16_t param2 = rescale(clamp(modulation, 0.0, 1.0), 0.0, 1.0, 0, INT16_MAX);
			osc.set_parameters(param1, param2);

			// Set pitch
			float pitchV = inputs[INPUT_PITCH].getVoltage() + params[PARAM_COARSE].getValue() + params[PARAM_FINE].getValue() / 12.0;
			if (!settings.meta_modulation)
				pitchV += fm;
			if (bLowCpu)
				pitchV += log2f(96000.0 / args.sampleRate);
			int32_t pitch = (pitchV * 12.0 + 60) * 128;

			// pitch_range
			if (settings.pitch_range == braids::PITCH_RANGE_EXTERNAL || settings.pitch_range == braids::PITCH_RANGE_LFO) {
			}
			else if (settings.pitch_range == braids::PITCH_RANGE_FREE) {
				pitch = pitch - 1638;
			}
			else if (settings.pitch_range == braids::PITCH_RANGE_440) {
				pitch = 69 << 7;
			}
			else { // PITCH_RANGE_EXTENDED
				pitch -= 60 << 7;
				pitch = (pitch - 1638) * 9 >> 1;
				pitch += 60 << 7;
			}

			pitch = quantizer.Process(pitch, (60 + settings.quantizer_root) << 7);

			// Check if the pitch has changed to cause an auto-retrigger
			int32_t pitch_delta = pitch - previousPitch;
			if (settings.auto_trig && (pitch_delta >= 0x40 || -pitch_delta >= 0x40)) {
				bFlagTriggerDetected = true;
			}
			previousPitch = pitch;

			pitch += jitterSource.Render(settings.vco_drift);
			pitch += adValue * settings.ad_fm >> 7;

			pitch = clamp(int(pitch), 0, 16383);

			if (settings.vco_flatten) {
				pitch = braids::Interpolate88(braids::lut_vco_detune, pitch << 2);
			}

			// Pitch transposition
			int32_t t = settings.pitch_range == braids::PITCH_RANGE_LFO ? -(36 << 7) : 0;
			t += (static_cast<int32_t>(settings.pitch_octave) - 2) * 12 * 128;
			osc.set_pitch(pitch + t);

			if (bTriggerFlag) {
				osc.Strike();
				envelope.Trigger(braids::ENV_SEGMENT_ATTACK);
				bTriggerFlag = false;
			}

			// TODO: add a sync input buffer (must be sample rate converted)
			uint8_t syncBuffer[24] = {};

			int16_t renderBuffer[24];
			osc.Render(syncBuffer, renderBuffer, 24);

			// Signature waveshaping, decimation, and bit reduction
			int16_t sample = 0;
			size_t decimationFactor = decimationFactors[settings.sample_rate];
			uint16_t bitMask = bitReductionMasks[settings.resolution];
			int32_t gain = settings.ad_vca > 0 ? adValue : 65535;
			uint16_t signature = settings.signature * settings.signature * 4095;
			for (size_t i = 0; i < 24; i++) {
				if ((i % decimationFactor) == 0) {
					sample = renderBuffer[i] & bitMask;
				}
				sample = sample * gainLp >> 16;
				gainLp += (gain - gainLp) >> 4;
				int16_t warped = waveShaper.Transform(sample);
				renderBuffer[i] = stmlib::Mix(sample, warped, signature);
			}

			if (bLowCpu) {
				for (int i = 0; i < 24; i++) {
					dsp::Frame<1> f;
					f.samples[0] = renderBuffer[i] / 32768.0;
					drbOutputBuffer.push(f);
				}
			}
			else {
				// Sample rate convert
				dsp::Frame<1> in[24];
				for (int i = 0; i < 24; i++) {
					in[i].samples[0] = renderBuffer[i] / 32768.0;
				}
				sampleRateConverter.setRates(96000, args.sampleRate);

				int inLen = 24;
				int outLen = drbOutputBuffer.capacity();
				sampleRateConverter.process(in, &inLen, drbOutputBuffer.endData(), &outLen);
				drbOutputBuffer.endIncr(outLen);
			}
		}
		// Output
		if (!drbOutputBuffer.empty()) {
			dsp::Frame<1> f = drbOutputBuffer.shift();
			outputs[OUTPUT_OUT].setVoltage(5.0 * f.samples[0]);
		}

		if (clockDivider.process()) {
			pollSwitches();
		}


		// Handle model light
		if (!bPaques) {
			int currentModel = settings.shape;
			lights[LIGHT_MODEL + 0].setBrightnessSmooth(lightColors[currentModel].red, args.sampleTime);
			lights[LIGHT_MODEL + 1].setBrightnessSmooth(lightColors[currentModel].green, args.sampleTime);
			lights[LIGHT_MODEL + 2].setBrightnessSmooth(lightColors[currentModel].blue, args.sampleTime);
		}
		else {
			lights[LIGHT_MODEL + 0].setBrightnessSmooth(lightColors[47].red, args.sampleTime);
			lights[LIGHT_MODEL + 1].setBrightnessSmooth(lightColors[47].green, args.sampleTime);
			lights[LIGHT_MODEL + 2].setBrightnessSmooth(lightColors[47].blue, args.sampleTime);
		}

		handleDisplay(args);
	}

	inline void handleDisplay(const ProcessArgs& args) {
		// Display handling
		// Display - return to model after 2s
		if (lastSettingChanged == braids::SETTING_OSCILLATOR_SHAPE) {
			if (!bPaques)
			{
				displayText = modelInfos[settings.shape].code;
			}
			else
			{
				displayText = modelInfos[47].code;
			}
		}
		else {
			int value;
			switch (lastSettingChanged)
			{
			case braids::SETTING_META_MODULATION: {
				displayText = "META";
				break;
			}
			case braids::SETTING_RESOLUTION: {
				value = settings.resolution;
				displayText = bitsStrings[value];
				break;
			}
			case braids::SETTING_SAMPLE_RATE: {
				value = settings.sample_rate;
				displayText = ratesStrings[value];
				break;
			}
			case braids::SETTING_TRIG_SOURCE: {
				displayText = "AUTO";
				break;
			}
			case braids::SETTING_TRIG_DELAY: {
				value = settings.trig_delay;
				displayText = triggerDelayStrings[value];
				break;
			}
			case braids::SETTING_AD_ATTACK: {
				value = settings.ad_attack;
				displayText = numberStrings[value];
				break;
			}
			case braids::SETTING_AD_DECAY: {
				value = settings.ad_decay;
				displayText = numberStrings[value];
				break;
			}
			case braids::SETTING_AD_FM: {
				value = settings.ad_fm;
				displayText = numberStrings[value];
				break;
			}
			case braids::SETTING_AD_TIMBRE: {
				value = settings.ad_timbre;
				displayText = numberStrings[value];
				break;
			}
			case braids::SETTING_AD_COLOR: {
				value = settings.ad_color;
				displayText = numberStrings[value];
				break;
			}
			case braids::SETTING_AD_VCA: {
				displayText = "\\VCA";
				break;
			}
			case braids::SETTING_PITCH_RANGE: {
				value = settings.pitch_range;
				displayText = pitchRangeStrings[value];
				break;
			}
			case braids::SETTING_PITCH_OCTAVE: {
				value = settings.pitch_octave;
				displayText = octaveStrings[value];
				break;
			}
			case braids::SETTING_QUANTIZER_SCALE: {
				value = settings.quantizer_scale;
				displayText = quantizationStrings[value];
				break;
			}
			case braids::SETTING_QUANTIZER_ROOT: {
				value = settings.quantizer_root;
				displayText = noteStrings[value];
				break;
			}
			case braids::SETTING_VCO_FLATTEN: {
				displayText = "FLAT";
				break;
			}
			case braids::SETTING_VCO_DRIFT: {
				displayText = "DRFT";
				break;
			}
			case braids::SETTING_SIGNATURE: {
				displayText = "SIGN";
				break;
			}
			default: {
				break;
			}
			}

			displayTimeout++;
		}

		if (displayTimeout > 1.0 * args.sampleRate) {
			lastSettingChanged = braids::SETTING_OSCILLATOR_SHAPE;
			displayTimeout = 0;
		}

		uint8_t* arrayLastSettings = &lastSettings.shape;
		uint8_t* arraySettings = &settings.shape;
		for (int i = 0; i < 20; i++) {
			if (arraySettings[i] != arrayLastSettings[i]) {
				arrayLastSettings[i] = arraySettings[i];
				lastSettingChanged = static_cast<braids::Setting>(i);
				displayTimeout = 0;
			}
		}
	}

	inline void pollSwitches() {
		const float sampleTime = APP->engine->getSampleTime() * kClockUpdateFrequency;
		// Handle switches
		settings.meta_modulation = params[PARAM_META].getValue();
		bPaques = params[PARAM_MORSE].getValue();
		settings.ad_vca = params[PARAM_VCA].getValue();
		settings.vco_drift = params[PARAM_DRIFT].getValue();
		settings.vco_flatten = params[PARAM_FLAT].getValue();
		settings.signature = params[PARAM_SIGN].getValue();
		settings.auto_trig = params[PARAM_AUTO].getValue();

		// Handle switch lights
		lights[LIGHT_META].setBrightnessSmooth(settings.meta_modulation, sampleTime);
		lights[LIGHT_MORSE].setBrightnessSmooth(bPaques, sampleTime);
		lights[LIGHT_VCA].setBrightnessSmooth(settings.ad_vca, sampleTime);
		lights[LIGHT_DRIFT].setBrightnessSmooth(settings.vco_drift, sampleTime);
		lights[LIGHT_FLAT].setBrightnessSmooth(settings.vco_flatten, sampleTime);
		lights[LIGHT_SIGN].setBrightnessSmooth(settings.signature, sampleTime);
		lights[LIGHT_AUTO].setBrightnessSmooth(settings.auto_trig, sampleTime);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_t* settingsJ = json_array();
		uint8_t* arraySettings = &settings.shape;
		for (int i = 0; i < 20; i++) {
			json_t* settingJ = json_integer(arraySettings[i]);
			json_array_insert_new(settingsJ, i, settingJ);
		}
		json_object_set_new(rootJ, "settings", settingsJ);

		json_t* lowCpuJ = json_boolean(bLowCpu);
		json_object_set_new(rootJ, "bLowCpu", lowCpuJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* settingsJ = json_object_get(rootJ, "settings");
		if (settingsJ) {
			uint8_t* arraySettings = &settings.shape;
			for (int i = 0; i < 20; i++) {
				json_t* settingJ = json_array_get(settingsJ, i);
				if (settingJ)
					arraySettings[i] = json_integer_value(settingJ);
			}
		}

		json_t* lowCpuJ = json_object_get(rootJ, "bLowCpu");
		if (lowCpuJ) {
			bLowCpu = json_boolean_value(lowCpuJ);
		}
	}

	int getModelParam() {
		return std::round(params[PARAM_MODEL].getValue() * braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
	}
	void setModelParam(int shape) {
		params[PARAM_MODEL].setValue(shape / (float)braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
	}
};

struct NodiDisplay : SanguineAlphaDisplay {
	NodiDisplay(uint32_t newCharacterCount) : SanguineAlphaDisplay(newCharacterCount) {

	}

	uint32_t* displayTimeout = nullptr;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			if (module && !module->isBypassed()) {
				if (font) {
					// Text
					nvgFontSize(args.vg, fontSize);
					nvgFontFaceId(args.vg, font->handle);
					nvgTextLetterSpacing(args.vg, 2.5);

					Vec textPos = Vec(9, 52);
					nvgFillColor(args.vg, nvgTransRGBA(textColor, 16));
					// Background of all segments
					std::string backgroundText = "";
					for (uint32_t i = 0; i < characterCount; i++)
						backgroundText += "~";
					nvgText(args.vg, textPos.x, textPos.y, backgroundText.c_str(), NULL);

					// Blink effect
					if (!(displayTimeout && *displayTimeout & 0x1000)) {
						nvgFillColor(args.vg, textColor);
						if (values.displayText && !(values.displayText->empty()))
						{
							// TODO: Make sure we only display max. display chars.
							nvgText(args.vg, textPos.x, textPos.y, values.displayText->c_str(), NULL);
						}
					}
					drawRectHalo(args, box.size, textColor, haloOpacity, 0.f);
				}
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct NodiWidget : ModuleWidget {
	NodiWidget(Nodi* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/nodi_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* nodiFrambuffer = new FramebufferWidget();
		addChild(nodiFrambuffer);

		NodiDisplay* nodiDisplay = new NodiDisplay(4);
		nodiDisplay->box.pos = mm2px(Vec(45.92, 10.396));
		nodiDisplay->module = module;
		nodiDisplay->values.displayText = &module->displayText;
		nodiDisplay->displayTimeout = &module->displayTimeout;
		nodiFrambuffer->addChild(nodiDisplay);

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(105.031, 20.996)), module, Nodi::PARAM_META, Nodi::LIGHT_META));

		addParam(createParamCentered<Rogan6PSWhite>(mm2px(Vec(71.12, 67.247)), module, Nodi::PARAM_MODEL));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(mm2px(Vec(71.12, 67.247)), module, Nodi::LIGHT_MODEL));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(8.222, 36.606)), module, Nodi::INPUT_TIMBRE));
		addParam(createParamCentered<Sanguine1PSPurple>(mm2px(Vec(22.768, 36.606)), module, Nodi::PARAM_TIMBRE));

		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(51.46, 40.534)), module, Nodi::PARAM_COARSE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(71.12, 42.184)), module, Nodi::PARAM_MORSE, Nodi::LIGHT_MORSE));
		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(90.809, 40.534)), module, Nodi::PARAM_FINE));


		addParam(createParamCentered<Sanguine1PSGreen>(mm2px(Vec(119.474, 36.606)), module, Nodi::PARAM_ATTACK));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(23.804, 54.231)), module, Nodi::PARAM_AD_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(119.4, 54.231)), module, Nodi::PARAM_VCA, Nodi::LIGHT_VCA));

		addParam(createParamCentered<Sanguine1PSPurple>(mm2px(Vec(10.076, 67.247)), module, Nodi::PARAM_MODULATION));
		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(36.032, 67.247)), module, Nodi::PARAM_ROOT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<YellowLight>>>(mm2px(Vec(48.572, 80.197)), module, Nodi::PARAM_DRIFT, Nodi::LIGHT_DRIFT));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<OrangeLight>>>(mm2px(Vec(93.673, 80.197)), module, Nodi::PARAM_FLAT, Nodi::LIGHT_FLAT));
		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(106.234, 67.247)), module, Nodi::PARAM_SCALE));
		addParam(createParamCentered<Sanguine1PSGreen>(mm2px(Vec(132.166, 67.247)), module, Nodi::PARAM_DECAY));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(23.804, 76.712)), module, Nodi::PARAM_AD_COLOR));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(119.4, 76.712)), module, Nodi::PARAM_AD_MODULATION));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(8.222, 97.889)), module, Nodi::INPUT_COLOR));
		addParam(createParamCentered<Sanguine1PSBlue>(mm2px(Vec(22.768, 97.889)), module, Nodi::PARAM_COLOR));

		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(51.457, 93.965)), module, Nodi::PARAM_PITCH_OCTAVE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<PurpleLight>>>(mm2px(Vec(71.12, 93.962)), module, Nodi::PARAM_SIGN, Nodi::LIGHT_SIGN));
		addParam(createParamCentered<Sanguine1PSRed>(mm2px(Vec(90.806, 93.965)), module, Nodi::PARAM_PITCH_RANGE));

		addParam(createParamCentered<Sanguine1PSOrange>(mm2px(Vec(119.474, 97.889)), module, Nodi::PARAM_FM));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(133.968, 97.889)), module, Nodi::INPUT_FM));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(8.222, 117.788)), module, Nodi::INPUT_PITCH));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(21.722, 117.788)), module, Nodi::INPUT_TRIGGER));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(35.151, 117.788)), module, Nodi::PARAM_TRIGGER_DELAY));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(46.798, 117.788)), module, Nodi::PARAM_AUTO, Nodi::LIGHT_AUTO));
		addParam(createParamCentered<Sanguine1PSYellow>(mm2px(Vec(62.4, 113.511)), module, Nodi::PARAM_BITS));
		addParam(createParamCentered<Sanguine1PSYellow>(mm2px(Vec(79.841, 113.511)), module, Nodi::PARAM_RATE));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(133.968, 117.788)), module, Nodi::OUTPUT_OUT));

		SanguineShapedLight* bloodLogo = new SanguineShapedLight();
		bloodLogo->box.pos = mm2px(Vec(96.594, 106.386));
		bloodLogo->wrap();
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		nodiFrambuffer->addChild(bloodLogo);

		SanguineShapedLight* mutantsLogo = new SanguineShapedLight();
		mutantsLogo->box.pos = mm2px(Vec(105.385, 114.755));
		mutantsLogo->wrap();
		mutantsLogo->module = module;
		mutantsLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/mutants_glowy.svg")));
		nodiFrambuffer->addChild(mutantsLogo);
	}

	void appendContextMenu(Menu* menu) override {
		Nodi* module = dynamic_cast<Nodi*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> shapeLabels;
		for (int i = 0; i < int(modelInfos.size() - 1); i++) {
			shapeLabels.push_back(modelInfos[i].code + ": " + modelInfos[i].label);
		}
		menu->addChild(createIndexSubmenuItem("Model", shapeLabels,
			[=]() {return module->getModelParam(); },
			[=](int i) {module->setModelParam(i); }
		));

		menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", "", &module->bLowCpu));
	}
};

Model* modelNodi = createModel<Nodi, NodiWidget>("Sanguine-Nodi");
