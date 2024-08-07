#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "clouds/dsp/granular_processor.h"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct ModeInfo {
	std::string display;
	std::string menuLabel;
};

static const std::vector<ModeInfo> modeList{
	{ "GRANULAR", "Granular mode" },
	{ "STRETCH", "Pitch shifter/time stretcher" },
	{ "LOOPING DLY", "Looping delay" },
	{ "SPECTRAL", "Spectral madness" }
};

static const std::string cvSuffix = " CV";

static const std::string ledButtonPrefix = "LED display value: ";

struct ModeDisplay {
	std::string labelFreeze;
	std::string labelPosition;
	std::string labelDensity;
	std::string labelSize;
	std::string labelTexture;
	std::string labelPitch;
	std::string labelTrigger;
};

static const std::vector<ModeDisplay> modeDisplays{
	{"Freeze",  "Position",     "Density",          "Size",             "Texture",          "Pitch",     "Trigger"},
	{"Stutter", "Scrub",        "Diffusion",        "Overlap",          "LP/HP",            "Pitch",     "Time"},
	{"Stutter", "Time / Start", "Diffusion",        "Overlap / Duratn", "LP/HP",            "Pitch",     "Time"},
	{"Freeze",  "Buffer",       "FFT Upd. / Merge", "Polynomial",       "Quantize / Parts", "Transpose", "Glitch"}
};

static const std::vector<ModeDisplay> modeTooltips{
	{"Freeze",  "Position",     "Density",          "Size",               "Texture",          "Pitch",     "Trigger"},
	{"Stutter", "Scrub",        "Diffusion",        "Overlap",            "LP/HP",            "Pitch",     "Time"},
	{"Stutter", "Time / Start", "Diffusion",        "Overlap / Duration", "LP/HP",            "Pitch",     "Time"},
	{"Freeze",  "Buffer",       "FFT Upd. / Merge", "Polynomial",         "Quantize / Parts", "Transpose", "Glitch"}
};

static const std::vector<std::string> buttonTexts{
	"Input",
	"Output",
	"Blends",
	"Momentary"
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
		LEDS_MOMENTARY
	} ledMode = LEDS_INPUT;

	LedModes lastLedMode = LEDS_INPUT;

	std::string textMode = modeList[0].display;
	std::string textFreeze = modeDisplays[0].labelFreeze;
	std::string textPosition = modeDisplays[0].labelPosition;
	std::string textDensity = modeDisplays[0].labelDensity;
	std::string textSize = modeDisplays[0].labelSize;
	std::string textTexture = modeDisplays[0].labelTexture;
	std::string textPitch = modeDisplays[0].labelPitch;
	std::string textTrigger = modeDisplays[0].labelTrigger;

	dsp::SampleRateConverter<2> inputSrc;
	dsp::SampleRateConverter<2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 256> outputBuffer;
	dsp::VuMeter2 vuMeter;
	dsp::ClockDivider lightDivider;
	dsp::BooleanTrigger btLedsMode;

	clouds::PlaybackMode playbackMode = clouds::PLAYBACK_MODE_GRANULAR;
	clouds::PlaybackMode lastPlaybackMode = clouds::PLAYBACK_MODE_GRANULAR;

	float freezeLight = 0.0;

	float lastBlend;
	float lastSpread;
	float lastFeedback;
	float lastReverb;

	const int kClockDivider = 512;
	int buffersize = 1;
	int currentbuffersize = 1;

	uint32_t displayTimeout = 0;

	bool lastFrozen = false;
	bool displaySwitched = false;
	bool triggered = false;

	uint8_t* block_mem;
	uint8_t* block_ccm;

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

		const int memLen = 118784;
		const int ccmLen = 65536 - 128;
		block_mem = new uint8_t[memLen]();
		block_ccm = new uint8_t[ccmLen]();
		cloudsProcessor = new clouds::GranularProcessor();
		memset(cloudsProcessor, 0, sizeof(*cloudsProcessor));

		lightDivider.setDivision(kClockDivider);

		cloudsProcessor->Init(block_mem, memLen, block_ccm, ccmLen);
	}

	~Nebulae() {
		delete cloudsProcessor;
		delete[] block_mem;
		delete[] block_ccm;
	}

	void process(const ProcessArgs& args) override {
		dsp::Frame<2> inputFrame;
		dsp::Frame<2> outputFrame;

		// Get input
		if (!inputBuffer.full()) {
			inputFrame.samples[0] = inputs[INPUT_LEFT].getVoltage() * params[PARAM_IN_GAIN].getValue() / 5.0;
			inputFrame.samples[1] = inputs[INPUT_RIGHT].isConnected() ? inputs[INPUT_RIGHT].getVoltage() * params[PARAM_IN_GAIN].getValue() / 5.0 : inputFrame.samples[0];
			inputBuffer.push(inputFrame);
		}

		// Trigger
		triggered = inputs[INPUT_TRIGGER].getVoltage() >= 1.0;

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
			if (currentbuffersize != buffersize) {
				// Re-init cloudsProcessor with new size.
				delete cloudsProcessor;
				delete[] block_mem;
				int memLen = 118784 * buffersize;
				const int ccmLen = 65536 - 128;
				block_mem = new uint8_t[memLen]();
				cloudsProcessor = new clouds::GranularProcessor();
				memset(cloudsProcessor, 0, sizeof(*cloudsProcessor));
				cloudsProcessor->Init(block_mem, memLen, block_ccm, ccmLen);
				currentbuffersize = buffersize;
			}

			// Set up cloudsProcessor
			cloudsProcessor->set_num_channels(params[PARAM_STEREO].getValue() ? 2 : 1);
			cloudsProcessor->set_low_fidelity(!(params[PARAM_HI_FI].getValue()));
			cloudsProcessor->set_playback_mode(playbackMode);
			cloudsProcessor->Prepare();

			bool frozen = params[PARAM_FREEZE].getValue();

			clouds::Parameters* cloudsParameters = cloudsProcessor->mutable_parameters();
			cloudsParameters->trigger = triggered;
			cloudsParameters->gate = triggered;
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

			if (frozen && !lastFrozen) {
				lastFrozen = true;
				if (!displaySwitched) {
					ledMode = LEDS_OUTPUT;
					lastLedMode = LEDS_OUTPUT;
				}
			}
			else if (!frozen && lastFrozen) {
				lastFrozen = false;
				if (!displaySwitched) {
					ledMode = LEDS_INPUT;
					lastLedMode = LEDS_INPUT;
				}
				else {
					displaySwitched = false;
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

			triggered = false;
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
		clouds::Parameters* cloudsParameters = cloudsProcessor->mutable_parameters();

		dsp::Frame<2> lightFrame;

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
			ledMode = LEDS_MOMENTARY;
			displayTimeout++;
		}

		if (displayTimeout > args.sampleRate) {
			ledMode = lastLedMode;
			displayTimeout = 0;
			lastBlend = params[PARAM_BLEND].getValue();
			lastSpread = params[PARAM_SPREAD].getValue();
			lastFeedback = params[PARAM_FEEDBACK].getValue();
			lastReverb = params[PARAM_REVERB].getValue();
		}

		if (lightDivider.process()) { // Expensive, so call this infrequently
			playbackMode = clouds::PlaybackMode(params[PARAM_MODE].getValue());

			if (playbackMode != lastPlaybackMode) {
				textMode = modeList[playbackMode].display;

				textFreeze = modeDisplays[playbackMode].labelFreeze;
				textPosition = modeDisplays[playbackMode].labelPosition;
				textDensity = modeDisplays[playbackMode].labelDensity;
				textSize = modeDisplays[playbackMode].labelSize;
				textTexture = modeDisplays[playbackMode].labelTexture;
				textPitch = modeDisplays[playbackMode].labelPitch;
				textTrigger = modeDisplays[playbackMode].labelTrigger;

				paramQuantities[PARAM_FREEZE]->name = modeTooltips[playbackMode].labelFreeze;
				inputInfos[INPUT_FREEZE]->name = modeTooltips[playbackMode].labelFreeze + cvSuffix;

				paramQuantities[PARAM_POSITION]->name = modeTooltips[playbackMode].labelPosition;
				inputInfos[INPUT_POSITION]->name = modeTooltips[playbackMode].labelPosition + cvSuffix;

				paramQuantities[PARAM_DENSITY]->name = modeTooltips[playbackMode].labelDensity;
				inputInfos[INPUT_DENSITY]->name = modeTooltips[playbackMode].labelDensity + cvSuffix;

				paramQuantities[PARAM_SIZE]->name = modeTooltips[playbackMode].labelSize;
				inputInfos[INPUT_SIZE]->name = modeTooltips[playbackMode].labelSize + cvSuffix;

				paramQuantities[PARAM_TEXTURE]->name = modeTooltips[playbackMode].labelTexture;
				inputInfos[INPUT_TEXTURE]->name = modeTooltips[playbackMode].labelTexture + cvSuffix;

				paramQuantities[PARAM_PITCH]->name = modeTooltips[playbackMode].labelPitch;
				inputInfos[INPUT_PITCH]->name = modeTooltips[playbackMode].labelPitch + cvSuffix;

				inputInfos[INPUT_TRIGGER]->name = modeTooltips[playbackMode].labelTrigger;

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

				paramQuantities[PARAM_LEDS_MODE]->name = ledButtonPrefix + buttonTexts[ledMode];

				if (lastFrozen) {
					displaySwitched = true;
				}
				else {
					displaySwitched = false;
				}
			}

			// These could probably do with base colored lights: they are never colorfully changed.
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
		json_object_set_new(rootJ, "buffersize", json_integer(buffersize));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* buffersizeJ = json_object_get(rootJ, "buffersize");
		if (buffersizeJ) {
			buffersize = json_integer_value(buffersizeJ);
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
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/nebulae_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		FramebufferWidget* nebulaeFramebuffer = new FramebufferWidget();
		addChild(nebulaeFramebuffer);

		Sanguine96x32OLEDDisplay* displayFreeze = new Sanguine96x32OLEDDisplay;
		displayFreeze->box.pos = mm2px(Vec(6.804, 13.711));
		displayFreeze->module = module;
		nebulaeFramebuffer->addChild(displayFreeze);

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(7.677, 25.607)), module, Nebulae::INPUT_FREEZE));
		CKD6* freezeButton = createParamCentered<CKD6>(mm2px(Vec(21.529, 25.607)), module, Nebulae::PARAM_FREEZE);
		freezeButton->latch = true;
		freezeButton->momentary = false;
		addParam(freezeButton);
		addChild(createLightCentered<CKD6Light<BlueLight>>(mm2px(Vec(21.529, 25.607)), module, Nebulae::LIGHT_FREEZE));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(79.173, 14.97)), module, Nebulae::LIGHT_BLEND));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(85.911, 14.97)), module, Nebulae::LIGHT_SPREAD));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(92.649, 14.97)), module, Nebulae::LIGHT_FEEDBACK));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(mm2px(Vec(99.386, 14.97)), module, Nebulae::LIGHT_REVERB));

		addParam(createParamCentered<TL1105>(mm2px(Vec(107.606, 14.97)), module, Nebulae::PARAM_LEDS_MODE));

		SanguineMatrixDisplay* displayModel = new SanguineMatrixDisplay(12);
		displayModel->box.pos = mm2px(Vec(50.963, 20.147));
		displayModel->module = module;
		nebulaeFramebuffer->addChild(displayModel);

		addParam(createParamCentered<Sanguine1PGrayCap>(mm2px(Vec(129.805, 25.227)), module, Nebulae::PARAM_MODE));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(mm2px(Vec(11.763, 50.173)), module, Nebulae::PARAM_POSITION, Nebulae::LIGHT_POSITION_CV));

		Sanguine96x32OLEDDisplay* displayPosition = new Sanguine96x32OLEDDisplay;
		displayPosition->box.pos = mm2px(Vec(3.614, 65.457));
		displayPosition->module = module;
		nebulaeFramebuffer->addChild(displayPosition);

		addInput(createInputCentered<BananutBlack>(mm2px(Vec(11.763, 76.776)), module, Nebulae::INPUT_POSITION));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(mm2px(Vec(29.722, 50.173)), module, Nebulae::PARAM_DENSITY, Nebulae::LIGHT_DENSITY_CV));

		Sanguine96x32OLEDDisplay* displayDensity = new Sanguine96x32OLEDDisplay;
		displayDensity->box.pos = mm2px(Vec(21.573, 65.457));
		displayDensity->module = module;
		nebulaeFramebuffer->addChild(displayDensity);

		addInput(createInputCentered<BananutBlack>(mm2px(Vec(29.722, 76.776)), module, Nebulae::INPUT_DENSITY));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(mm2px(Vec(47.682, 50.173)), module, Nebulae::PARAM_SIZE, Nebulae::LIGHT_SIZE_CV));

		Sanguine96x32OLEDDisplay* displaySize = new Sanguine96x32OLEDDisplay;
		displaySize->box.pos = mm2px(Vec(39.533, 65.457));
		displaySize->module = module;
		nebulaeFramebuffer->addChild(displaySize);

		addInput(createInputCentered<BananutBlack>(mm2px(Vec(47.682, 76.776)), module, Nebulae::INPUT_SIZE));

		addParam(createLightParamCentered<VCVLightSlider<GreenRedLight>>(mm2px(Vec(65.644, 50.173)), module, Nebulae::PARAM_TEXTURE, Nebulae::LIGHT_TEXTURE_CV));

		Sanguine96x32OLEDDisplay* displayTexture = new Sanguine96x32OLEDDisplay;
		displayTexture->box.pos = mm2px(Vec(57.495, 65.457));
		displayTexture->module = module;
		nebulaeFramebuffer->addChild(displayTexture);

		addInput(createInputCentered<BananutBlack>(mm2px(Vec(65.644, 76.776)), module, Nebulae::INPUT_TEXTURE));

		addParam(createParamCentered<Sanguine1PRed>(mm2px(Vec(105.638, 41.169)), module, Nebulae::PARAM_PITCH));

		Sanguine96x32OLEDDisplay* displayPitch = new Sanguine96x32OLEDDisplay;
		displayPitch->box.pos = mm2px(Vec(97.489, 48.465));
		displayPitch->module = module;
		nebulaeFramebuffer->addChild(displayPitch);

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(105.638, 59.887)), module, Nebulae::INPUT_PITCH));

		addParam(createParamCentered<Sanguine1PGreen>(mm2px(Vec(86.118, 41.169)), module, Nebulae::PARAM_BLEND));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(86.118, 59.887)), module, Nebulae::INPUT_BLEND));

		Sanguine96x32OLEDDisplay* displayTrigger = new Sanguine96x32OLEDDisplay;
		displayTrigger->box.pos = mm2px(Vec(117.065, 48.465));
		displayTrigger->module = module;
		nebulaeFramebuffer->addChild(displayTrigger);

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(125.214, 59.887)), module, Nebulae::INPUT_TRIGGER));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(86.118, 78.013)), module, Nebulae::INPUT_SPREAD));

		addParam(createParamCentered<Sanguine1PBlue>(mm2px(Vec(86.118, 96.727)), module, Nebulae::PARAM_SPREAD));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(105.638, 78.013)), module, Nebulae::INPUT_FEEDBACK));

		addParam(createParamCentered<Sanguine1PPurple>(mm2px(Vec(105.638, 96.727)), module, Nebulae::PARAM_FEEDBACK));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(125.214, 78.013)), module, Nebulae::INPUT_REVERB));

		addParam(createParamCentered<Sanguine1PYellow>(mm2px(Vec(125.214, 96.727)), module, Nebulae::PARAM_REVERB));

		addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(14.603, 97.272)), module, Nebulae::PARAM_IN_GAIN));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenLight>>>(mm2px(Vec(31.438, 97.272)), module, Nebulae::PARAM_HI_FI, Nebulae::LIGHT_HI_FI));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(56.63, 97.272)), module, Nebulae::PARAM_STEREO, Nebulae::LIGHT_STEREO));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(7.677, 116.972)), module, Nebulae::INPUT_LEFT));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(21.529, 116.972)), module, Nebulae::INPUT_RIGHT));

		SanguineShapedLight* bloodLogo = new SanguineShapedLight();
		bloodLogo->box.pos = mm2px(Vec(56.919, 106.223));
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		addChild(bloodLogo);

		SanguineShapedLight* mutantsLogo = new SanguineShapedLight();
		mutantsLogo->box.pos = mm2px(Vec(65.71, 114.592));
		mutantsLogo->module = module;
		mutantsLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/mutants_glowy.svg")));
		addChild(mutantsLogo);

		addOutput(createOutputCentered<BananutGreen>(mm2px(Vec(115.161, 116.972)), module, Nebulae::OUTPUT_LEFT));
		addOutput(createOutputCentered<BananutGreen>(mm2px(Vec(129.013, 116.972)), module, Nebulae::OUTPUT_RIGHT));

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
		for (int i = 0; i < int(modeList.size()); i++) {
			modelLabels.push_back(modeList[i].display + ": " + modeList[i].menuLabel);
		}
		menu->addChild(createIndexSubmenuItem("Mode", modelLabels,
			[=]() {return module->getModeParam(); },
			[=](int i) {module->setModeParam(i); }
		));
	}
};

Model* modelNebulae = createModel<Nebulae, NebulaeWidget>("Sanguine-Nebulae");