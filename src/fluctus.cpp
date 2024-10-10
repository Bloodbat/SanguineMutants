#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "fluctus/dsp/fluctus_granular_processor.h"
#include "cloudycommon.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

static const std::vector<NebulaeModeInfo> fluctusModeList{
	{ "GRANULAR", "Granular mode" },
	{ "STRETCH", "Pitch shifter/time stretcher" },
	{ "LOOPING DLY", "Looping delay" },
	{ "SPCT. CLOUDS", "Spectral clouds" },
	{ "BEAT-REPEAT", "Beat-repeat " },
};

static const std::vector<EtesiaModeDisplay> fluctusModeDisplays{
	{"Freeze",  "Position",     "Density",          "Size",             "Texture",     "Pitch",         "Trigger",   "Blend",       "Spread",         "Feedback",   "Reverb"},
	{"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP/HP",       "Pitch",         "Time",      "Blend",       "Spread",         "Feedback",   "Reverb"},
	{"Stutter", "Time / Start", "Diffusion",        "Overlap / Duratn", "LP/HP",       "Pitch",         "Time",      "Blend",       "Spread",         "Feedback",   "Reverb"},
	{"Freeze",  "Fq. Bnd Prb.", "Flt. Smooth",      "Fq. Bnd. Div.",    "Flt. Text.",  "Pitch shift",   "Randomize", "Dry/wet",     "Rnd. Flt. Prob", "Warm dist.", "Reverb"},
	{"Freeze",  "Loop begin",   "Loop size mod.",   "Loop size",        "Slice step",  "Playback spd.", "Clock",     "Slice prob.", "Clock div.",     "Pitch mod.", "Feedback"}
};

static const std::vector<EtesiaModeDisplay> fluctusModeTooltips{
	{"Freeze",  "Position",                    "Density",              "Size",                    "Texture",        "Pitch",          "Trigger",   "Blend",             "Spread",                    "Feedback",         "Reverb"},
	{"Stutter", "Scrub",                       "Diffusion",            "Overlap",                 "LP/HP",          "Pitch",          "Time",      "Blend",             "Spread",                    "Feedback",         "Reverb"},
	{"Stutter", "Time / Start",                "Diffusion",            "Overlap / Duration",      "LP/HP",          "Pitch",          "Time",      "Blend",             "Spread",                    "Feedback",         "Reverb"},
	{"Freeze",  "Frequency band Probability",  "Filter Smoothing",     "Frequency band division", "Filter texture", "Pitch shift",    "Randomize", "Dry/wet",           "Random filter probability", "Warm distortion",  "Reverb"},
	{"Freeze",  "Loop begin",                  "Loop size modulation", "Loop size",               "Slice step",     "Playback speed", "Clock",     "Slice probability", "Clock division",            "Pitch modulation", "Feedback"}
};

struct Fluctus : SanguineModule {
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
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_LEFT,
		OUTPUT_RIGHT,
		OUTPUTS_COUNT
	};
	enum LightIds {
		LIGHT_FREEZE,
		ENUMS(LIGHT_BLEND, 2),
		ENUMS(LIGHT_SPREAD, 2),
		ENUMS(LIGHT_FEEDBACK, 2),
		ENUMS(LIGHT_REVERB, 2),
		ENUMS(LIGHT_POSITION_CV, 2),
		ENUMS(LIGHT_DENSITY_CV, 2),
		ENUMS(LIGHT_SIZE_CV, 2),
		ENUMS(LIGHT_TEXTURE_CV, 2),
		LIGHT_HI_FI,
		LIGHT_STEREO,
		LIGHTS_COUNT
	};

	enum LedModes {
		LEDS_INPUT,
		LEDS_OUTPUT,
		LEDS_PARAMETERS,
		LEDS_MOMENTARY,
		LEDS_QUALITY_MOMENTARY,
		LEDS_MODE_MOMENTARY
	} ledMode = LEDS_INPUT;

	LedModes lastLedMode = LEDS_INPUT;

	std::string textMode = fluctusModeList[0].display;
	std::string textFreeze = fluctusModeDisplays[0].labelFreeze;
	std::string textPosition = fluctusModeDisplays[0].labelPosition;
	std::string textDensity = fluctusModeDisplays[0].labelDensity;
	std::string textSize = fluctusModeDisplays[0].labelSize;
	std::string textTexture = fluctusModeDisplays[0].labelTexture;
	std::string textPitch = fluctusModeDisplays[0].labelPitch;
	std::string textTrigger = fluctusModeDisplays[0].labelTrigger;
	std::string textBlend = fluctusModeDisplays[0].labelBlend;
	std::string textSpread = fluctusModeDisplays[0].labelSpread;
	std::string textFeedback = fluctusModeDisplays[0].labelFeeback;
	std::string textReverb = fluctusModeDisplays[0].labelReverb;

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;
	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider lightDivider;
	dsp::BooleanTrigger btLedsMode;

	fluctus::PlaybackMode playbackMode = fluctus::PLAYBACK_MODE_GRANULAR;
	fluctus::PlaybackMode lastPlaybackMode = fluctus::PLAYBACK_MODE_GRANULAR;
	fluctus::PlaybackMode lastLEDPlaybackMode = fluctus::PLAYBACK_MODE_GRANULAR;

	float freezeLight = 0.f;

	float lastBlend;
	float lastSpread;
	float lastFeedback;
	float lastReverb;

	int lastHiFi;
	int lastStereo;

	const int kClockDivider = 512;
	int bufferSize = 1;
	int currentBufferSize = 1;

	uint32_t displayTimeout = 0;

	bool bLastFrozen = false;
	bool bDisplaySwitched = false;
	bool bTriggered = false;

	uint8_t* bufferLarge;
	uint8_t* bufferSmall;

	fluctus::FluctusGranularProcessor* fluctusProcessor;

	Fluctus() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configInput(INPUT_FREEZE, "Freeze");
		configParam(PARAM_FREEZE, 0.f, 1.f, 0.f, "Freeze");

		configButton(PARAM_LEDS_MODE, "LED display value: Input");

		configParam(PARAM_MODE, 0.f, 4.f, 0.f, "Mode", "", 0.f, 1.f, 1.f);
		paramQuantities[PARAM_MODE]->snapEnabled = true;

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

		configParam(PARAM_BLEND, 0.f, 1.f, 0.5f, "Dry/wet", "%", 0.f, 100.f);
		configInput(INPUT_BLEND, "Dry/wet CV");

		configInput(INPUT_TRIGGER, "Trigger");

		configInput(INPUT_SPREAD, "Spread CV");
		configParam(PARAM_SPREAD, 0.f, 1.f, 0.5f, "Spread", "%", 0.f, 100.f);

		configInput(INPUT_FEEDBACK, "Feedback CV");
		configParam(PARAM_FEEDBACK, 0.f, 1.f, 0.5f, "Feedback", "%", 0.f, 100.f);

		configInput(INPUT_REVERB, "Reverb CV");
		configParam(PARAM_REVERB, 0.f, 1.f, 0.5f, "Reverb", "%", 0.f, 100.f);

		configParam(PARAM_IN_GAIN, 0.f, 1.f, 0.5f, "Input gain", "%", 0.f, 100.f);

		configInput(INPUT_LEFT, "Left");
		configInput(INPUT_RIGHT, "Right");

		configParam(PARAM_HI_FI, 0.f, 1.f, 1.f, "Toggle Hi-Fi");
		configParam(PARAM_STEREO, 0.f, 1.f, 1.f, "Toggle stereo");

		configOutput(OUTPUT_LEFT, "Left");
		configOutput(OUTPUT_RIGHT, "Right");

		configBypass(INPUT_LEFT, OUTPUT_LEFT);
		configBypass(INPUT_RIGHT, OUTPUT_RIGHT);

		lastBlend = 0.5f;
		lastSpread = 0.5f;
		lastFeedback = 0.5f;
		lastReverb = 0.5f;

		lastHiFi = 1;
		lastStereo = 1;

		const int memLen = 118784;
		const int ccmLen = 65536 - 128;
		bufferLarge = new uint8_t[memLen]();
		bufferSmall = new uint8_t[ccmLen]();
		fluctusProcessor = new fluctus::FluctusGranularProcessor();
		memset(fluctusProcessor, 0, sizeof(*fluctusProcessor));

		lightDivider.setDivision(kClockDivider);

		fluctusProcessor->Init(bufferLarge, memLen, bufferSmall, ccmLen);
	}

	~Fluctus() {
		delete fluctusProcessor;
		delete[] bufferLarge;
		delete[] bufferSmall;
	}

	void process(const ProcessArgs& args) override {
		dsp::Frame<2> inputFrame;
		dsp::Frame<2> outputFrame = {};

		// Get input
		if (!inputBuffer.full()) {
			inputFrame.samples[0] = inputs[INPUT_LEFT].getVoltageSum() * params[PARAM_IN_GAIN].getValue() / 5.f;
			inputFrame.samples[1] = inputs[INPUT_RIGHT].isConnected() ? inputs[INPUT_RIGHT].getVoltageSum()
				* params[PARAM_IN_GAIN].getValue() / 5.f : inputFrame.samples[0];
			inputBuffer.push(inputFrame);
		}

		// Trigger
		bTriggered = inputs[INPUT_TRIGGER].getVoltage() >= 1.f;

		fluctus::Parameters* fluctusParameters = fluctusProcessor->mutable_parameters();

		// Render frames
		if (outputBuffer.empty()) {
			fluctus::ShortFrame input[32] = {};
			// Convert input buffer
			{
				inputSrc.setRates(args.sampleRate, 32000);
				dsp::Frame<2> inputFrames[32];
				int inLen = inputBuffer.size();
				int outLen = 32;
				inputSrc.process(inputBuffer.startData(), &inLen, inputFrames, &outLen);
				inputBuffer.startIncr(inLen);

				// We might not fill all of the input buffer if there is a deficiency, but this cannot be avoided due to imprecisions between the input and output SRC.
				for (int i = 0; i < outLen; i++) {
					input[i].l = clamp(inputFrames[i].samples[0] * 32767.0, -32768, 32767);
					input[i].r = clamp(inputFrames[i].samples[1] * 32767.0, -32768, 32767);
				}
			}
			if (currentBufferSize != bufferSize) {
				// Re-init fluctusProcessor with new size.
				delete fluctusProcessor;
				delete[] bufferLarge;
				int memLen = 118784 * bufferSize;
				const int ccmLen = 65536 - 128;
				bufferLarge = new uint8_t[memLen]();
				fluctusProcessor = new fluctus::FluctusGranularProcessor();
				memset(fluctusProcessor, 0, sizeof(*fluctusProcessor));
				fluctusProcessor->Init(bufferLarge, memLen, bufferSmall, ccmLen);
				currentBufferSize = bufferSize;
			}

			// Set up Fluctus processor
			fluctusProcessor->set_playback_mode(playbackMode);
			fluctusProcessor->set_num_channels(static_cast<bool>(params[PARAM_STEREO].getValue()) ? 2 : 1);
			fluctusProcessor->set_low_fidelity(!static_cast<bool>(params[PARAM_HI_FI].getValue()));
			fluctusProcessor->Prepare();

			bool frozen = params[PARAM_FREEZE].getValue();

			fluctusParameters->trigger = bTriggered;
			fluctusParameters->gate = bTriggered;
			fluctusParameters->freeze = (inputs[INPUT_FREEZE].getVoltage() >= 1.f || frozen);
			fluctusParameters->position = clamp(params[PARAM_POSITION].getValue() + inputs[INPUT_POSITION].getVoltage() / 5.f, 0.f, 1.f);
			fluctusParameters->size = clamp(params[PARAM_SIZE].getValue() + inputs[INPUT_SIZE].getVoltage() / 5.f, 0.f, 1.f);
			fluctusParameters->pitch = clamp((params[PARAM_PITCH].getValue() + inputs[INPUT_PITCH].getVoltage()) * 12.f, -48.f, 48.f);
			fluctusParameters->density = clamp(params[PARAM_DENSITY].getValue() + inputs[INPUT_DENSITY].getVoltage() / 5.f, 0.f, 1.f);
			fluctusParameters->texture = clamp(params[PARAM_TEXTURE].getValue() + inputs[INPUT_TEXTURE].getVoltage() / 5.f, 0.f, 1.f);
			float blend = clamp(params[PARAM_BLEND].getValue() + inputs[INPUT_BLEND].getVoltage() / 5.f, 0.f, 1.f);
			fluctusParameters->dry_wet = blend;
			fluctusParameters->stereo_spread = clamp(params[PARAM_SPREAD].getValue() + inputs[INPUT_SPREAD].getVoltage() / 5.f, 0.f, 1.f);
			fluctusParameters->feedback = clamp(params[PARAM_FEEDBACK].getValue() + inputs[INPUT_FEEDBACK].getVoltage() / 5.f, 0.f, 1.f);
			fluctusParameters->reverb = clamp(params[PARAM_REVERB].getValue() + inputs[INPUT_REVERB].getVoltage() / 5.f, 0.f, 1.f);
			fluctusParameters->kammerl.probability = fluctusParameters->dry_wet;
			fluctusParameters->kammerl.clock_divider = fluctusParameters->stereo_spread;
			fluctusParameters->kammerl.pitch_mode = fluctusParameters->feedback;
			fluctusParameters->kammerl.distortion = fluctusParameters->reverb;
			fluctusParameters->kammerl.pitch = clamp((math::rescale(params[PARAM_PITCH].getValue(), -2.f, 2.f, 0.f, 1.f) + inputs[INPUT_PITCH].getVoltage() / 5.f), 0.f, 1.f);

			fluctus::ShortFrame output[32];
			fluctusProcessor->Process(input, output, 32);

			if (frozen && !bLastFrozen) {
				bLastFrozen = true;
				if (!bDisplaySwitched) {
					ledMode = LEDS_OUTPUT;
					lastLedMode = LEDS_OUTPUT;
				}
			}
			else if (!frozen && bLastFrozen) {
				bLastFrozen = false;
				if (!bDisplaySwitched) {
					ledMode = LEDS_INPUT;
					lastLedMode = LEDS_INPUT;
				}
				else {
					bDisplaySwitched = false;
				}
			}

			// Convert output buffer
			{
				dsp::Frame<2> outputFrames[32];
				for (int i = 0; i < 32; i++) {
					outputFrames[i].samples[0] = output[i].l / 32768.f;
					outputFrames[i].samples[1] = output[i].r / 32768.f;
				}

				outputSrc.setRates(32000, args.sampleRate);
				int inLen = 32;
				int outLen = outputBuffer.capacity();
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}

			bTriggered = false;
		}

		// Set output			
		if (!outputBuffer.empty()) {
			outputFrame = outputBuffer.shift();
			if (outputs[OUTPUT_LEFT].isConnected()) {
				outputs[OUTPUT_LEFT].setVoltage(5.f * outputFrame.samples[0]);
			}
			if (outputs[OUTPUT_RIGHT].isConnected()) {
				outputs[OUTPUT_RIGHT].setVoltage(5.f * outputFrame.samples[1]);
			}
		}

		// Lights
		dsp::Frame<2> lightFrame = {};

		switch (ledMode)
		{
		case LEDS_OUTPUT: {
			lightFrame = outputFrame;
			break;
		}
		default: {
			lightFrame = inputFrame;
			break;
		}
		}

		vuMeter.process(args.sampleTime, fmaxf(fabsf(lightFrame.samples[0]), fabsf(lightFrame.samples[1])));

		lights[LIGHT_FREEZE].setBrightnessSmooth(fluctusParameters->freeze ? 0.75f : 0.f, args.sampleTime);

		if (params[PARAM_BLEND].getValue() != lastBlend || params[PARAM_SPREAD].getValue() != lastSpread ||
			params[PARAM_FEEDBACK].getValue() != lastFeedback || params[PARAM_REVERB].getValue() != lastReverb) {
			if (ledMode != LEDS_MOMENTARY) {
				ledMode = LEDS_MOMENTARY;
			}
			displayTimeout++;
		}

		if (params[PARAM_HI_FI].getValue() != lastHiFi || params[PARAM_STEREO].getValue() != lastStereo) {
			if (ledMode != LEDS_QUALITY_MOMENTARY) {
				ledMode = LEDS_QUALITY_MOMENTARY;
			}
			displayTimeout++;
		}

		if (playbackMode != lastLEDPlaybackMode) {
			if (ledMode != LEDS_MODE_MOMENTARY) {
				ledMode = LEDS_MODE_MOMENTARY;
			}
			displayTimeout++;
		}

		if (displayTimeout > args.sampleRate * 2) {
			ledMode = lastLedMode;
			displayTimeout = 0;
			lastBlend = params[PARAM_BLEND].getValue();
			lastSpread = params[PARAM_SPREAD].getValue();
			lastFeedback = params[PARAM_FEEDBACK].getValue();
			lastReverb = params[PARAM_REVERB].getValue();

			lastHiFi = params[PARAM_HI_FI].getValue();
			lastStereo = params[PARAM_STEREO].getValue();

			lastLEDPlaybackMode = playbackMode;
		}

		if (lightDivider.process()) { // Expensive, so call this infrequently
			playbackMode = fluctus::PlaybackMode(params[PARAM_MODE].getValue());

			if (playbackMode != lastPlaybackMode) {
				textMode = fluctusModeList[playbackMode].display;

				textFreeze = fluctusModeDisplays[playbackMode].labelFreeze;
				textPosition = fluctusModeDisplays[playbackMode].labelPosition;
				textDensity = fluctusModeDisplays[playbackMode].labelDensity;
				textSize = fluctusModeDisplays[playbackMode].labelSize;
				textTexture = fluctusModeDisplays[playbackMode].labelTexture;
				textPitch = fluctusModeDisplays[playbackMode].labelPitch;
				textTrigger = fluctusModeDisplays[playbackMode].labelTrigger;
				// Parasite
				textBlend = fluctusModeDisplays[playbackMode].labelBlend;
				textSpread = fluctusModeDisplays[playbackMode].labelSpread;
				textFeedback = fluctusModeDisplays[playbackMode].labelFeeback;
				textReverb = fluctusModeDisplays[playbackMode].labelReverb;

				paramQuantities[PARAM_FREEZE]->name = fluctusModeTooltips[playbackMode].labelFreeze;
				inputInfos[INPUT_FREEZE]->name = fluctusModeTooltips[playbackMode].labelFreeze + nebulaeCVSuffix;

				paramQuantities[PARAM_POSITION]->name = fluctusModeTooltips[playbackMode].labelPosition;
				inputInfos[INPUT_POSITION]->name = fluctusModeTooltips[playbackMode].labelPosition + nebulaeCVSuffix;

				paramQuantities[PARAM_DENSITY]->name = fluctusModeTooltips[playbackMode].labelDensity;
				inputInfos[INPUT_DENSITY]->name = fluctusModeTooltips[playbackMode].labelDensity + nebulaeCVSuffix;

				paramQuantities[PARAM_SIZE]->name = fluctusModeTooltips[playbackMode].labelSize;
				inputInfos[INPUT_SIZE]->name = fluctusModeTooltips[playbackMode].labelSize + nebulaeCVSuffix;

				paramQuantities[PARAM_TEXTURE]->name = fluctusModeTooltips[playbackMode].labelTexture;
				inputInfos[INPUT_TEXTURE]->name = fluctusModeTooltips[playbackMode].labelTexture + nebulaeCVSuffix;

				paramQuantities[PARAM_PITCH]->name = fluctusModeTooltips[playbackMode].labelPitch;
				inputInfos[INPUT_PITCH]->name = fluctusModeTooltips[playbackMode].labelPitch + nebulaeCVSuffix;

				inputInfos[INPUT_TRIGGER]->name = fluctusModeTooltips[playbackMode].labelTrigger;

				// Parasite
				paramQuantities[PARAM_BLEND]->name = fluctusModeTooltips[playbackMode].labelBlend;
				inputInfos[INPUT_BLEND]->name = fluctusModeTooltips[playbackMode].labelBlend + nebulaeCVSuffix;

				paramQuantities[PARAM_SPREAD]->name = fluctusModeTooltips[playbackMode].labelSpread;
				inputInfos[INPUT_SPREAD]->name = fluctusModeTooltips[playbackMode].labelSpread + nebulaeCVSuffix;

				paramQuantities[PARAM_FEEDBACK]->name = fluctusModeTooltips[playbackMode].labelFeeback;
				inputInfos[INPUT_FEEDBACK]->name = fluctusModeTooltips[playbackMode].labelFeeback + nebulaeCVSuffix;

				paramQuantities[PARAM_REVERB]->name = fluctusModeTooltips[playbackMode].labelReverb;
				inputInfos[INPUT_REVERB]->name = fluctusModeTooltips[playbackMode].labelReverb + nebulaeCVSuffix;

				lastPlaybackMode = playbackMode;
			}

			if (btLedsMode.process(params[PARAM_LEDS_MODE].getValue())) {
				ledMode = LedModes((ledMode + 1) % 3);
				lastLedMode = ledMode;
				displayTimeout = 0;
				lastBlend = params[PARAM_BLEND].getValue();
				lastSpread = params[PARAM_SPREAD].getValue();
				lastFeedback = params[PARAM_FEEDBACK].getValue();
				lastReverb = params[PARAM_REVERB].getValue();

				lastHiFi = params[PARAM_HI_FI].getValue();
				lastStereo = params[PARAM_STEREO].getValue();

				lastLEDPlaybackMode = playbackMode;

				paramQuantities[PARAM_LEDS_MODE]->name = nebulaeLedButtonPrefix + nebulaeButtonTexts[ledMode];

				if (bLastFrozen) {
					bDisplaySwitched = true;
				}
				else {
					bDisplaySwitched = false;
				}
			}

			switch (ledMode)
			{
			case LEDS_INPUT:
			case LEDS_OUTPUT: {
				lights[LIGHT_BLEND].setBrightness(vuMeter.getBrightness(-24.f, -18.f));
				lights[LIGHT_BLEND + 1].setBrightness(0.f);
				lights[LIGHT_SPREAD].setBrightness(vuMeter.getBrightness(-18.f, -12.f));
				lights[LIGHT_SPREAD + 1].setBrightness(0.f);
				lights[LIGHT_FEEDBACK].setBrightness(vuMeter.getBrightness(-12.f, -6.f));
				lights[LIGHT_FEEDBACK + 1].setBrightness(vuMeter.getBrightness(-12.f, -6.f));
				lights[LIGHT_REVERB].setBrightness(0.f);
				lights[LIGHT_REVERB + 1].setBrightness(vuMeter.getBrightness(-6.f, 0.f));
				break;
			}
			case LEDS_PARAMETERS:
			case LEDS_MOMENTARY: {
				float value;
				int currentLight;

				for (int i = 0; i < 4; i++) {
					value = params[PARAM_BLEND + i].getValue();
					currentLight = LIGHT_BLEND + i * 2;
					lights[currentLight + 0].setBrightness(value <= 0.66f ? math::rescale(value, 0.f, 0.66f, 0.f, 1.f) : math::rescale(value, 0.67f, 1.f, 1.f, 0.f));
					lights[currentLight + 1].setBrightness(value >= 0.33f ? math::rescale(value, 0.33f, 1.f, 0.f, 1.f) : math::rescale(value, 1.f, 0.34f, 1.f, 0.f));
				}
				break;
			}
			case LEDS_QUALITY_MOMENTARY: {
				lights[LIGHT_BLEND].setBrightness(0.f);
				lights[LIGHT_BLEND + 1].setBrightness((params[PARAM_HI_FI].getValue() > 0 && params[PARAM_STEREO].getValue() > 0) ? 1.f : 0.f);
				lights[LIGHT_SPREAD].setBrightness(0.f);
				lights[LIGHT_SPREAD + 1].setBrightness((params[PARAM_HI_FI].getValue() > 0 && params[PARAM_STEREO].getValue() < 1) ? 1.f : 0.f);
				lights[LIGHT_FEEDBACK].setBrightness(0.f);
				lights[LIGHT_FEEDBACK + 1].setBrightness((params[PARAM_HI_FI].getValue() < 1 && params[PARAM_STEREO].getValue() > 0) ? 1.f : 0.f);
				lights[LIGHT_REVERB].setBrightness(0.f);
				lights[LIGHT_REVERB + 1].setBrightness((params[PARAM_HI_FI].getValue() < 1 && params[PARAM_STEREO].getValue() < 1) ? 1.f : 0.f);
				break;
			}

			case LEDS_MODE_MOMENTARY: {
				lights[LIGHT_BLEND].setBrightness(playbackMode == 0 || playbackMode > 2 ? 1.f : 0.f);
				lights[LIGHT_BLEND + 1].setBrightness(playbackMode == 0 || playbackMode > 2 ? 1.f : 0.f);
				lights[LIGHT_SPREAD].setBrightness(playbackMode == 1 || playbackMode > 2 ? 1.f : 0.f);
				lights[LIGHT_SPREAD + 1].setBrightness(playbackMode == 1 || playbackMode > 2 ? 1.f : 0.f);
				lights[LIGHT_FEEDBACK].setBrightness(playbackMode >= 2 ? 1.f : 0.f);
				lights[LIGHT_FEEDBACK + 1].setBrightness(playbackMode >= 2 ? 1.f : 0.f);
				lights[LIGHT_REVERB].setBrightness(playbackMode == 4 ? 1.f : 0.f);
				lights[LIGHT_REVERB + 1].setBrightness(playbackMode == 4 ? 1.f : 0.f);

				break;
			}
			}

			float rescaledLight = math::rescale(inputs[INPUT_POSITION].getVoltage(), 0.f, 5.f, 0.f, 1.f);
			lights[LIGHT_POSITION_CV + 0].setBrightness(rescaledLight);
			lights[LIGHT_POSITION_CV + 1].setBrightness(-rescaledLight);

			rescaledLight = math::rescale(inputs[INPUT_DENSITY].getVoltage(), 0.f, 5.f, 0.f, 1.f);
			lights[LIGHT_DENSITY_CV + 0].setBrightness(rescaledLight);
			lights[LIGHT_DENSITY_CV + 1].setBrightness(-rescaledLight);

			rescaledLight = math::rescale(inputs[INPUT_SIZE].getVoltage(), 0.f, 5.f, 0.f, 1.f);
			lights[LIGHT_SIZE_CV + 0].setBrightness(rescaledLight);
			lights[LIGHT_SIZE_CV + 1].setBrightness(-rescaledLight);

			rescaledLight = math::rescale(inputs[INPUT_TEXTURE].getVoltage(), 0.f, 5.f, 0.f, 1.f);
			lights[LIGHT_TEXTURE_CV + 0].setBrightness(rescaledLight);
			lights[LIGHT_TEXTURE_CV + 1].setBrightness(-rescaledLight);

			const float sampleTime = args.sampleTime * kClockDivider;

			lights[LIGHT_HI_FI].setBrightnessSmooth(params[PARAM_HI_FI].getValue() ? 0.75f : 0.f, sampleTime);
			lights[LIGHT_STEREO].setBrightnessSmooth(params[PARAM_STEREO].getValue() ? 0.75f : 0.f, sampleTime);
		} // lightDivider
	}


	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		json_object_set_new(rootJ, "buffersize", json_integer(bufferSize));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* buffersizeJ = json_object_get(rootJ, "buffersize");
		if (buffersizeJ) {
			bufferSize = json_integer_value(buffersizeJ);
		}
	}

	int getModeParam() {
		return params[PARAM_MODE].getValue();
	}
	void setModeParam(int mode) {
		params[PARAM_MODE].setValue(mode);
	}
};

struct FluctusWidget : SanguineModuleWidget {
	FluctusWidget(Fluctus* module) {
		setModule(module);

		moduleName = "fluctus";
		panelSize = SIZE_27;
		backplateColor = PLATE_GREEN;

		makePanel();

		addScrews(SCREW_ALL);

		FramebufferWidget* etesiaFramebuffer = new FramebufferWidget();
		addChild(etesiaFramebuffer);

		Sanguine96x32OLEDDisplay* displayFreeze = new Sanguine96x32OLEDDisplay(module, 14.953, 16.419);
		etesiaFramebuffer->addChild(displayFreeze);
		displayFreeze->fallbackString = fluctusModeDisplays[0].labelFreeze;

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(7.677, 25.607), module, Fluctus::INPUT_FREEZE));
		CKD6* freezeButton = createParamCentered<CKD6>(millimetersToPixelsVec(21.529, 25.607), module, Fluctus::PARAM_FREEZE);
		freezeButton->latch = true;
		freezeButton->momentary = false;
		addParam(freezeButton);
		addChild(createLightCentered<CKD6Light<BlueLight>>(millimetersToPixelsVec(21.529, 25.607), module, Fluctus::LIGHT_FREEZE));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(79.173, 12.851), module, Fluctus::LIGHT_BLEND));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(85.911, 12.851), module, Fluctus::LIGHT_SPREAD));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(92.649, 12.851), module, Fluctus::LIGHT_FEEDBACK));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(99.386, 12.851), module, Fluctus::LIGHT_REVERB));

		addParam(createParamCentered<TL1105>(millimetersToPixelsVec(107.606, 12.851), module, Fluctus::PARAM_LEDS_MODE));

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 85.18, 25.227);
		etesiaFramebuffer->addChild(displayModel);
		displayModel->fallbackString = fluctusModeList[0].display;

		addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(128.505, 25.227), module, Fluctus::PARAM_MODE));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(11.763, 50.173),
			module, Fluctus::PARAM_POSITION, Fluctus::LIGHT_POSITION_CV));

		Sanguine96x32OLEDDisplay* displayPosition = new Sanguine96x32OLEDDisplay(module, 11.763, 68.166);
		etesiaFramebuffer->addChild(displayPosition);
		displayPosition->fallbackString = fluctusModeDisplays[0].labelPosition;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(11.763, 76.776), module, Fluctus::INPUT_POSITION));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(29.722, 50.173),
			module, Fluctus::PARAM_DENSITY, Fluctus::LIGHT_DENSITY_CV));

		Sanguine96x32OLEDDisplay* displayDensity = new Sanguine96x32OLEDDisplay(module, 29.722, 68.166);
		etesiaFramebuffer->addChild(displayDensity);
		displayDensity->fallbackString = fluctusModeDisplays[0].labelDensity;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(29.722, 76.776), module, Fluctus::INPUT_DENSITY));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(47.682, 50.173),
			module, Fluctus::PARAM_SIZE, Fluctus::LIGHT_SIZE_CV));

		Sanguine96x32OLEDDisplay* displaySize = new Sanguine96x32OLEDDisplay(module, 47.682, 68.166);
		etesiaFramebuffer->addChild(displaySize);
		displaySize->fallbackString = fluctusModeDisplays[0].labelSize;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(47.682, 76.776), module, Fluctus::INPUT_SIZE));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(65.644, 50.173),
			module, Fluctus::PARAM_TEXTURE, Fluctus::LIGHT_TEXTURE_CV));

		Sanguine96x32OLEDDisplay* displayTexture = new Sanguine96x32OLEDDisplay(module, 65.644, 68.166);
		etesiaFramebuffer->addChild(displayTexture);
		displayTexture->fallbackString = fluctusModeDisplays[0].labelTexture;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(65.644, 76.776), module, Fluctus::INPUT_TEXTURE));

		addParam(createParamCentered<Sanguine1PRed>(millimetersToPixelsVec(105.638, 41.169), module, Fluctus::PARAM_PITCH));

		Sanguine96x32OLEDDisplay* displayPitch = new Sanguine96x32OLEDDisplay(module, 105.638, 51.174);
		etesiaFramebuffer->addChild(displayPitch);
		displayPitch->fallbackString = fluctusModeDisplays[0].labelPitch;

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(105.638, 59.887), module, Fluctus::INPUT_PITCH));

		addParam(createParamCentered<Sanguine1PGreen>(millimetersToPixelsVec(86.118, 41.169), module, Fluctus::PARAM_BLEND));

		Sanguine96x32OLEDDisplay* displayBlend = new Sanguine96x32OLEDDisplay(module, 86.118, 51.174);
		etesiaFramebuffer->addChild(displayBlend);
		displayBlend->fallbackString = fluctusModeDisplays[0].labelBlend;

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(86.118, 59.887), module, Fluctus::INPUT_BLEND));

		Sanguine96x32OLEDDisplay* displayTrigger = new Sanguine96x32OLEDDisplay(module, 125.214, 51.174);
		etesiaFramebuffer->addChild(displayTrigger);
		displayTrigger->fallbackString = fluctusModeDisplays[0].labelTrigger;

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(125.214, 59.887), module, Fluctus::INPUT_TRIGGER));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(86.118, 78.013), module, Fluctus::INPUT_SPREAD));

		Sanguine96x32OLEDDisplay* displaySpread = new Sanguine96x32OLEDDisplay(module, 86.118, 86.709);
		etesiaFramebuffer->addChild(displaySpread);
		displaySpread->fallbackString = fluctusModeDisplays[0].labelSpread;

		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(86.118, 96.727), module, Fluctus::PARAM_SPREAD));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(105.638, 78.013), module, Fluctus::INPUT_FEEDBACK));

		Sanguine96x32OLEDDisplay* displayFeedback = new Sanguine96x32OLEDDisplay(module, 105.638, 86.709);
		etesiaFramebuffer->addChild(displayFeedback);
		displayFeedback->fallbackString = fluctusModeDisplays[0].labelFeeback;

		addParam(createParamCentered<Sanguine1PPurple>(millimetersToPixelsVec(105.638, 96.727), module, Fluctus::PARAM_FEEDBACK));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(125.214, 78.013), module, Fluctus::INPUT_REVERB));

		Sanguine96x32OLEDDisplay* displayReverb = new Sanguine96x32OLEDDisplay(module, 125.214, 86.709);
		etesiaFramebuffer->addChild(displayReverb);
		displayReverb->fallbackString = fluctusModeDisplays[0].labelReverb;

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(125.214, 96.727), module, Fluctus::PARAM_REVERB));

		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(14.603, 97.272), module, Fluctus::PARAM_IN_GAIN));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(31.438, 97.272),
			module, Fluctus::PARAM_HI_FI, Fluctus::LIGHT_HI_FI));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(56.63, 97.272),
			module, Fluctus::PARAM_STEREO, Fluctus::LIGHT_STEREO));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.677, 116.972), module, Fluctus::INPUT_LEFT));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(21.529, 116.972), module, Fluctus::INPUT_RIGHT));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 58.816, 110.16);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 71.817, 117.093);
		addChild(mutantsLogo);

		addOutput(createOutputCentered<BananutGreen>(millimetersToPixelsVec(115.161, 116.972), module, Fluctus::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutGreen>(millimetersToPixelsVec(129.013, 116.972), module, Fluctus::OUTPUT_RIGHT));

		if (module) {
			displayModel->values.displayText = &module->textMode;

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
		}
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Fluctus* module = dynamic_cast<Fluctus*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < static_cast<int>(fluctusModeList.size()); i++) {
			modelLabels.push_back(fluctusModeList[i].display + ": " + fluctusModeList[i].menuLabel);
		}
		menu->addChild(createIndexSubmenuItem("Mode", modelLabels,
			[=]() {return module->getModeParam(); },
			[=](int i) {module->setModeParam(i); }
		));
	}
};

Model* modelFluctus = createModel<Fluctus, FluctusWidget>("Sanguine-Fluctus");