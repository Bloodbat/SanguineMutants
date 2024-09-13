#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "clouds/dsp/granular_processor.h"
#include "cloudycommon.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

static const std::vector<NebulaeModeInfo> nebulaeModeList{
	{ "GRANULAR", "Granular mode" },
	{ "STRETCH", "Pitch shifter/time stretcher" },
	{ "LOOPING DLY", "Looping delay" },
	{ "SPECTRAL", "Spectral madness" }
};

struct NebulaeModeDisplay {
	std::string labelFreeze;
	std::string labelPosition;
	std::string labelDensity;
	std::string labelSize;
	std::string labelTexture;
	std::string labelPitch;
	std::string labelTrigger;
};

static const std::vector<NebulaeModeDisplay> nebulaeModeDisplays{
	{"Freeze",  "Position",     "Density",          "Size",             "Texture",          "Pitch",     "Trigger"},
	{"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP/HP",            "Pitch",     "Time"},
	{"Stutter", "Time / Start", "Diffusion",        "Overlap / Duratn", "LP/HP",            "Pitch",     "Time"},
	{"Freeze",  "Buffer",       "FFT Upd. / Merge", "Polynomial",       "Quantize / Parts", "Transpose", "Glitch"}
};

static const std::vector<NebulaeModeDisplay> nebulaeModeTooltips{
	{"Freeze",  "Position",     "Density",          "Size",               "Texture",          "Pitch",     "Trigger"},
	{"Stutter", "Scrub",        "Diffusion",        "Overlap",            "LP/HP",            "Pitch",     "Time"},
	{"Stutter", "Time / Start", "Diffusion",        "Overlap / Duration", "LP/HP",            "Pitch",     "Time"},
	{"Freeze",  "Buffer",       "FFT Upd. / Merge", "Polynomial",         "Quantize / Parts", "Transpose", "Glitch"}
};

struct Nebulae : Module {
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

	std::string textMode = nebulaeModeList[0].display;
	std::string textFreeze = nebulaeModeDisplays[0].labelFreeze;
	std::string textPosition = nebulaeModeDisplays[0].labelPosition;
	std::string textDensity = nebulaeModeDisplays[0].labelDensity;
	std::string textSize = nebulaeModeDisplays[0].labelSize;
	std::string textTexture = nebulaeModeDisplays[0].labelTexture;
	std::string textPitch = nebulaeModeDisplays[0].labelPitch;
	std::string textTrigger = nebulaeModeDisplays[0].labelTrigger;

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;
	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider lightDivider;
	dsp::BooleanTrigger btLedsMode;

	clouds::PlaybackMode playbackMode = clouds::PLAYBACK_MODE_GRANULAR;
	clouds::PlaybackMode lastPlaybackMode = clouds::PLAYBACK_MODE_GRANULAR;
	clouds::PlaybackMode lastLEDPlaybackMode = clouds::PLAYBACK_MODE_GRANULAR;

	float freezeLight = 0.0;

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

	clouds::GranularProcessor* cloudsProcessor;

	Nebulae() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configInput(INPUT_FREEZE, "Freeze");
		configParam(PARAM_FREEZE, 0.f, 1.f, 0.f, "Freeze");

		configButton(PARAM_LEDS_MODE, "LED display value: Input");

		configParam(PARAM_MODE, 0.f, 3.f, 0.f, "Mode", "", 0.f, 1.f, 1.f);
		paramQuantities[PARAM_MODE]->snapEnabled = true;

		configParam(PARAM_POSITION, 0.0, 1.0, 0.5, "Grain position", "%", 0.f, 100.f);
		configInput(INPUT_POSITION, "Grain position CV");

		configParam(PARAM_DENSITY, 0.0, 1.0, 0.5, "Grain density", "%", 0.f, 100.f);
		configInput(INPUT_DENSITY, "Grain density CV");

		configParam(PARAM_SIZE, 0.0, 1.0, 0.5, "Grain size", "%", 0.f, 100.f);
		configInput(INPUT_SIZE, "Grain size CV");

		configParam(PARAM_TEXTURE, 0.0, 1.0, 0.5, "Grain texture", "%", 0.f, 100.f);
		configInput(INPUT_TEXTURE, "Grain texture CV");

		configParam(PARAM_PITCH, -2.0, 2.0, 0.0, "Grain pitch");
		configInput(INPUT_PITCH, "Pitch (1V/oct)");

		configParam(PARAM_BLEND, 0.0, 1.0, 0.5, "Dry/wet", "%", 0.f, 100.f);
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
		lastFeedback = 0.5;
		lastReverb = 0.5;

		lastHiFi = 1;
		lastStereo = 1;

		const int memLen = 118784;
		const int ccmLen = 65536 - 128;
		bufferLarge = new uint8_t[memLen]();
		bufferSmall = new uint8_t[ccmLen]();
		cloudsProcessor = new clouds::GranularProcessor();
		memset(cloudsProcessor, 0, sizeof(*cloudsProcessor));

		lightDivider.setDivision(kClockDivider);

		cloudsProcessor->Init(bufferLarge, memLen, bufferSmall, ccmLen);
	}

	~Nebulae() {
		delete cloudsProcessor;
		delete[] bufferLarge;
		delete[] bufferSmall;
	}

	void process(const ProcessArgs& args) override {
		dsp::Frame<2> inputFrame;
		dsp::Frame<2> outputFrame = {};

		// Get input
		if (!inputBuffer.full()) {
			inputFrame.samples[0] = inputs[INPUT_LEFT].getVoltage() * params[PARAM_IN_GAIN].getValue() / 5.0;
			inputFrame.samples[1] = inputs[INPUT_RIGHT].isConnected() ? inputs[INPUT_RIGHT].getVoltage() * params[PARAM_IN_GAIN].getValue() / 5.0 : inputFrame.samples[0];
			inputBuffer.push(inputFrame);
		}

		// Trigger
		bTriggered = inputs[INPUT_TRIGGER].getVoltage() >= 1.0;

		clouds::Parameters* cloudsParameters = cloudsProcessor->mutable_parameters();

		// Render frames
		if (outputBuffer.empty()) {
			clouds::ShortFrame input[32] = {};
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
				// Re-init cloudsProcessor with new size.
				delete cloudsProcessor;
				delete[] bufferLarge;
				int memLen = 118784 * bufferSize;
				const int ccmLen = 65536 - 128;
				bufferLarge = new uint8_t[memLen]();
				cloudsProcessor = new clouds::GranularProcessor();
				memset(cloudsProcessor, 0, sizeof(*cloudsProcessor));
				cloudsProcessor->Init(bufferLarge, memLen, bufferSmall, ccmLen);
				currentBufferSize = bufferSize;
			}

			// Set up Clouds processor			
			cloudsProcessor->set_playback_mode(playbackMode);
			cloudsProcessor->set_num_channels(bool(params[PARAM_STEREO].getValue()) ? 2 : 1);
			cloudsProcessor->set_low_fidelity(!bool(params[PARAM_HI_FI].getValue()));
			cloudsProcessor->Prepare();

			bool frozen = params[PARAM_FREEZE].getValue();

			cloudsParameters->trigger = bTriggered;
			cloudsParameters->gate = bTriggered;
			cloudsParameters->freeze = (inputs[INPUT_FREEZE].getVoltage() >= 1.0 || frozen);
			cloudsParameters->position = clamp(params[PARAM_POSITION].getValue() + inputs[INPUT_POSITION].getVoltage() / 5.0, 0.0f, 1.0f);
			cloudsParameters->size = clamp(params[PARAM_SIZE].getValue() + inputs[INPUT_SIZE].getVoltage() / 5.0, 0.0f, 1.0f);
			cloudsParameters->pitch = clamp((params[PARAM_PITCH].getValue() + inputs[INPUT_PITCH].getVoltage()) * 12.0, -48.0f, 48.0f);
			cloudsParameters->density = clamp(params[PARAM_DENSITY].getValue() + inputs[INPUT_DENSITY].getVoltage() / 5.0, 0.0f, 1.0f);
			cloudsParameters->texture = clamp(params[PARAM_TEXTURE].getValue() + inputs[INPUT_TEXTURE].getVoltage() / 5.0, 0.0f, 1.0f);
			float blend = clamp(params[PARAM_BLEND].getValue() + inputs[INPUT_BLEND].getVoltage() / 5.0, 0.0f, 1.0f);
			cloudsParameters->dry_wet = blend;
			cloudsParameters->stereo_spread = clamp(params[PARAM_SPREAD].getValue() + inputs[INPUT_SPREAD].getVoltage() / 5.0, 0.0f, 1.0f);
			cloudsParameters->feedback = clamp(params[PARAM_FEEDBACK].getValue() + inputs[INPUT_FEEDBACK].getVoltage() / 5.0, 0.0f, 1.0f);
			cloudsParameters->reverb = clamp(params[PARAM_REVERB].getValue() + inputs[INPUT_REVERB].getVoltage() / 5.0, 0.0f, 1.0f);

			clouds::ShortFrame output[32];
			cloudsProcessor->Process(input, output, 32);

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

			lights[LIGHT_FREEZE].setBrightnessSmooth(cloudsParameters->freeze ? 1.0 : 0.0, args.sampleTime);

			// Convert output buffer
			{
				dsp::Frame<2> outputFrames[32];
				for (int i = 0; i < 32; i++) {
					outputFrames[i].samples[0] = output[i].l / 32768.0;
					outputFrames[i].samples[1] = output[i].r / 32768.0;
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
				outputs[OUTPUT_LEFT].setVoltage(5.0 * outputFrame.samples[0]);
			}
			if (outputs[OUTPUT_RIGHT].isConnected()) {
				outputs[OUTPUT_RIGHT].setVoltage(5.0 * outputFrame.samples[1]);
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

		lights[LIGHT_FREEZE].setBrightness(cloudsParameters->freeze ? 0.75f : 0.0f);

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
			playbackMode = clouds::PlaybackMode(params[PARAM_MODE].getValue());

			if (playbackMode != lastPlaybackMode) {
				textMode = nebulaeModeList[playbackMode].display;

				textFreeze = nebulaeModeDisplays[playbackMode].labelFreeze;
				textPosition = nebulaeModeDisplays[playbackMode].labelPosition;
				textDensity = nebulaeModeDisplays[playbackMode].labelDensity;
				textSize = nebulaeModeDisplays[playbackMode].labelSize;
				textTexture = nebulaeModeDisplays[playbackMode].labelTexture;
				textPitch = nebulaeModeDisplays[playbackMode].labelPitch;
				textTrigger = nebulaeModeDisplays[playbackMode].labelTrigger;

				paramQuantities[PARAM_FREEZE]->name = nebulaeModeTooltips[playbackMode].labelFreeze;
				inputInfos[INPUT_FREEZE]->name = nebulaeModeTooltips[playbackMode].labelFreeze + nebulaeCVSuffix;

				paramQuantities[PARAM_POSITION]->name = nebulaeModeTooltips[playbackMode].labelPosition;
				inputInfos[INPUT_POSITION]->name = nebulaeModeTooltips[playbackMode].labelPosition + nebulaeCVSuffix;

				paramQuantities[PARAM_DENSITY]->name = nebulaeModeTooltips[playbackMode].labelDensity;
				inputInfos[INPUT_DENSITY]->name = nebulaeModeTooltips[playbackMode].labelDensity + nebulaeCVSuffix;

				paramQuantities[PARAM_SIZE]->name = nebulaeModeTooltips[playbackMode].labelSize;
				inputInfos[INPUT_SIZE]->name = nebulaeModeTooltips[playbackMode].labelSize + nebulaeCVSuffix;

				paramQuantities[PARAM_TEXTURE]->name = nebulaeModeTooltips[playbackMode].labelTexture;
				inputInfos[INPUT_TEXTURE]->name = nebulaeModeTooltips[playbackMode].labelTexture + nebulaeCVSuffix;

				paramQuantities[PARAM_PITCH]->name = nebulaeModeTooltips[playbackMode].labelPitch;
				inputInfos[INPUT_PITCH]->name = nebulaeModeTooltips[playbackMode].labelPitch + nebulaeCVSuffix;

				inputInfos[INPUT_TRIGGER]->name = nebulaeModeTooltips[playbackMode].labelTrigger;

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
				int currentLight;
				for (int i = 0; i < clouds::PLAYBACK_MODE_LAST; i++) {
					currentLight = LIGHT_BLEND + i * 2;

					lights[currentLight + 0].setBrightness(playbackMode == i ? 1.f : 0.f);
					lights[currentLight + 1].setBrightness(playbackMode == i ? 1.f : 0.f);
				}
				break;
			}
			}

			lights[LIGHT_POSITION_CV + 0].setBrightness(math::rescale(inputs[INPUT_POSITION].getVoltage(), 0.f, 5.f, 0.f, 1.f));
			lights[LIGHT_POSITION_CV + 1].setBrightness(math::rescale(-inputs[INPUT_POSITION].getVoltage(), 0.f, 5.f, 0.f, 1.f));

			lights[LIGHT_DENSITY_CV + 0].setBrightness(math::rescale(inputs[INPUT_DENSITY].getVoltage(), 0.f, 5.f, 0.f, 1.f));
			lights[LIGHT_DENSITY_CV + 1].setBrightness(math::rescale(-inputs[INPUT_DENSITY].getVoltage(), 0.f, 5.f, 0.f, 1.f));

			lights[LIGHT_SIZE_CV + 0].setBrightness(math::rescale(inputs[INPUT_SIZE].getVoltage(), 0.f, 5.f, 0.f, 1.f));
			lights[LIGHT_SIZE_CV + 1].setBrightness(math::rescale(-inputs[INPUT_SIZE].getVoltage(), 0.f, 5.f, 0.f, 1.f));

			lights[LIGHT_TEXTURE_CV + 0].setBrightness(math::rescale(inputs[INPUT_TEXTURE].getVoltage(), 0.f, 5.f, 0.f, 1.f));
			lights[LIGHT_TEXTURE_CV + 1].setBrightness(math::rescale(-inputs[INPUT_TEXTURE].getVoltage(), 0.f, 5.f, 0.f, 1.f));

			const float sampleTime = args.sampleTime * kClockDivider;

			lights[LIGHT_HI_FI].setBrightnessSmooth(params[PARAM_HI_FI].getValue() ? 1.f : 0.f, sampleTime);
			lights[LIGHT_STEREO].setBrightnessSmooth(params[PARAM_STEREO].getValue() ? 1.f : 0.f, sampleTime);
		} // lightDivider
	}


	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "buffersize", json_integer(bufferSize));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
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

struct NebulaeWidget : ModuleWidget {
	NebulaeWidget(Nebulae* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_27hp_purple.svg", "res/nebulae_faceplate.svg");
		panel->addLayer("res/nebulae_common.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* nebulaeFramebuffer = new FramebufferWidget();
		addChild(nebulaeFramebuffer);

		Sanguine96x32OLEDDisplay* displayFreeze = new Sanguine96x32OLEDDisplay(module, 14.953, 16.419);
		nebulaeFramebuffer->addChild(displayFreeze);
		displayFreeze->fallbackString = nebulaeModeDisplays[0].labelFreeze;

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(7.677, 25.607), module, Nebulae::INPUT_FREEZE));
		CKD6* freezeButton = createParamCentered<CKD6>(millimetersToPixelsVec(21.529, 25.607), module, Nebulae::PARAM_FREEZE);
		freezeButton->latch = true;
		freezeButton->momentary = false;
		addParam(freezeButton);
		addChild(createLightCentered<CKD6Light<BlueLight>>(millimetersToPixelsVec(21.529, 25.607), module, Nebulae::LIGHT_FREEZE));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(79.173, 12.851), module, Nebulae::LIGHT_BLEND));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(85.911, 12.851), module, Nebulae::LIGHT_SPREAD));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(92.649, 12.851), module, Nebulae::LIGHT_FEEDBACK));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(99.386, 12.851), module, Nebulae::LIGHT_REVERB));

		addParam(createParamCentered<TL1105>(millimetersToPixelsVec(107.606, 12.851), module, Nebulae::PARAM_LEDS_MODE));

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12, module, 85.18, 25.227);
		nebulaeFramebuffer->addChild(displayModel);
		displayModel->fallbackString = nebulaeModeList[0].display;

		addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(128.505, 25.227), module, Nebulae::PARAM_MODE));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(11.763, 50.173),
			module, Nebulae::PARAM_POSITION, Nebulae::LIGHT_POSITION_CV));

		Sanguine96x32OLEDDisplay* displayPosition = new Sanguine96x32OLEDDisplay(module, 11.763, 68.166);
		nebulaeFramebuffer->addChild(displayPosition);
		displayPosition->fallbackString = nebulaeModeDisplays[0].labelPosition;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(11.763, 76.776), module, Nebulae::INPUT_POSITION));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(29.722, 50.173),
			module, Nebulae::PARAM_DENSITY, Nebulae::LIGHT_DENSITY_CV));

		Sanguine96x32OLEDDisplay* displayDensity = new Sanguine96x32OLEDDisplay(module, 29.722, 68.166);
		nebulaeFramebuffer->addChild(displayDensity);
		displayDensity->fallbackString = nebulaeModeDisplays[0].labelDensity;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(29.722, 76.776), module, Nebulae::INPUT_DENSITY));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(47.682, 50.173),
			module, Nebulae::PARAM_SIZE, Nebulae::LIGHT_SIZE_CV));

		Sanguine96x32OLEDDisplay* displaySize = new Sanguine96x32OLEDDisplay(module, 47.682, 68.166);
		nebulaeFramebuffer->addChild(displaySize);
		displaySize->fallbackString = nebulaeModeDisplays[0].labelSize;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(47.682, 76.776), module, Nebulae::INPUT_SIZE));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(millimetersToPixelsVec(65.644, 50.173),
			module, Nebulae::PARAM_TEXTURE, Nebulae::LIGHT_TEXTURE_CV));

		Sanguine96x32OLEDDisplay* displayTexture = new Sanguine96x32OLEDDisplay(module, 65.644, 68.166);
		nebulaeFramebuffer->addChild(displayTexture);
		displayTexture->fallbackString = nebulaeModeDisplays[0].labelTexture;

		addInput(createInputCentered<BananutBlack>(millimetersToPixelsVec(65.644, 76.776), module, Nebulae::INPUT_TEXTURE));

		addParam(createParamCentered<Sanguine1PRed>(millimetersToPixelsVec(105.638, 41.169), module, Nebulae::PARAM_PITCH));

		Sanguine96x32OLEDDisplay* displayPitch = new Sanguine96x32OLEDDisplay(module, 105.638, 51.174);
		nebulaeFramebuffer->addChild(displayPitch);
		displayPitch->fallbackString = nebulaeModeDisplays[0].labelPitch;

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(105.638, 59.887), module, Nebulae::INPUT_PITCH));

		addParam(createParamCentered<Sanguine1PGreen>(millimetersToPixelsVec(86.118, 41.169), module, Nebulae::PARAM_BLEND));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(86.118, 59.887), module, Nebulae::INPUT_BLEND));

		Sanguine96x32OLEDDisplay* displayTrigger = new Sanguine96x32OLEDDisplay(module, 125.214, 51.174);
		nebulaeFramebuffer->addChild(displayTrigger);
		displayTrigger->fallbackString = nebulaeModeDisplays[0].labelTrigger;

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(125.214, 59.887), module, Nebulae::INPUT_TRIGGER));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(86.118, 78.013), module, Nebulae::INPUT_SPREAD));

		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(86.118, 96.727), module, Nebulae::PARAM_SPREAD));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(105.638, 78.013), module, Nebulae::INPUT_FEEDBACK));

		addParam(createParamCentered<Sanguine1PPurple>(millimetersToPixelsVec(105.638, 96.727), module, Nebulae::PARAM_FEEDBACK));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(125.214, 78.013), module, Nebulae::INPUT_REVERB));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(125.214, 96.727), module, Nebulae::PARAM_REVERB));

		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(14.603, 97.272), module, Nebulae::PARAM_IN_GAIN));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(millimetersToPixelsVec(31.438, 97.272),
			module, Nebulae::PARAM_HI_FI, Nebulae::LIGHT_HI_FI));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(56.63, 97.272),
			module, Nebulae::PARAM_STEREO, Nebulae::LIGHT_STEREO));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.677, 116.972), module, Nebulae::INPUT_LEFT));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(21.529, 116.972), module, Nebulae::INPUT_RIGHT));

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 58.816, 110.16);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 71.817, 117.093);
		addChild(mutantsLogo);

		addOutput(createOutputCentered<BananutGreen>(millimetersToPixelsVec(115.161, 116.972), module, Nebulae::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutGreen>(millimetersToPixelsVec(129.013, 116.972), module, Nebulae::OUTPUT_RIGHT));

		if (module) {
			displayModel->values.displayText = &module->textMode;

			displayFreeze->oledText = &module->textFreeze;
			displayPosition->oledText = &module->textPosition;
			displayDensity->oledText = &module->textDensity;
			displaySize->oledText = &module->textSize;
			displayTexture->oledText = &module->textTexture;
			displayPitch->oledText = &module->textPitch;
			displayTrigger->oledText = &module->textTrigger;
		}
	}

	void appendContextMenu(Menu* menu) override {
		Nebulae* module = dynamic_cast<Nebulae*>(this->module);

		menu->addChild(new MenuSeparator);

		std::vector<std::string> modelLabels;
		for (int i = 0; i < int(nebulaeModeList.size()); i++) {
			modelLabels.push_back(nebulaeModeList[i].display + ": " + nebulaeModeList[i].menuLabel);
		}
		menu->addChild(createIndexSubmenuItem("Mode", modelLabels,
			[=]() {return module->getModeParam(); },
			[=](int i) {module->setModeParam(i); }
		));
	}
};

Model* modelNebulae = createModel<Nebulae, NebulaeWidget>("Sanguine-Nebulae");