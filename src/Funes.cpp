#include "plugin.hpp"
#include "sanguinecomponents.hpp"

#pragma GCC diagnostic push

#ifndef __clang__
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif

#include "plaits/dsp/voice.h"
//#include "plaits/user_data_receiver.h"
#include "plaits/user_data.h"

#pragma GCC diagnostic pop

#include <osdialog.h>
#include <thread>

#include <fstream>

static const char WAVE_FILTERS[] = "BIN (*.bin):bin, BIN";
static std::string waveDir;

struct Funes : Module {
	enum ParamIds {
		PARAM_MODEL,
		PARAM_FREQUENCY,
		PARAM_HARMONICS,
		PARAM_TIMBRE,
		PARAM_MORPH,
		PARAM_TIMBRE_CV,
		PARAM_FREQUENCY_CV,
		PARAM_MORPH_CV,
		PARAM_LPG_COLOR,
		PARAM_LPG_DECAY,
		PARAM_FREQUENCY_ROOT,
		NUM_PARAMS
	};

	enum InputIds {
		INPUT_ENGINE,
		INPUT_TIMBRE,
		INPUT_FREQUENCY,
		INPUT_MORPH,
		INPUT_HARMONICS,
		INPUT_TRIGGER,
		INPUT_LEVEL,
		INPUT_NOTE,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTPUT_OUT,
		OUTPUT_AUX,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(LIGHT_MODEL, 8 * 3),
		NUM_LIGHTS
	};

	plaits::Voice voice[16];
	plaits::Patch patch = {};
	plaits::UserData user_data;
	char shared_buffer[16][16384] = {};
	float triPhase = 0.f;
	int frequencyMode = 10;
	stmlib::HysteresisQuantizer2 octaveQuantizer;

	dsp::SampleRateConverter<16 * 2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<16 * 2>, 256> outputBuffer;
	bool lowCpu = false;

	bool displayModulatedModel = true;

	bool loading = false;

	bool notesModelSelection = false;

	int modelNum = 0;

	float lastModelVoltage = 0.f;

	Funes() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(PARAM_MODEL, 0.0f, 23.0f, 8.0f, "Model", "", 0.0f, 1.0f, 1.0f);
		paramQuantities[PARAM_MODEL]->snapEnabled = true;

		configParam(PARAM_FREQUENCY, -4.0, 4.0, 0.0, "Frequency", " semitones", 0.f, 12.f);
		configParam(PARAM_FREQUENCY_ROOT, -4.0, 4.0, 0.0, "Frequency Root", " semitones", 0.f, 12.f);
		configParam(PARAM_HARMONICS, 0.0, 1.0, 0.5, "Harmonics", "%", 0.f, 100.f);
		configParam(PARAM_TIMBRE, 0.0, 1.0, 0.5, "Timbre", "%", 0.f, 100.f);
		configParam(PARAM_LPG_COLOR, 0.0, 1.0, 0.5, "Lowpass gate response", "%", 0.f, 100.f);
		configParam(PARAM_MORPH, 0.0, 1.0, 0.5, "Morph", "%", 0.f, 100.f);
		configParam(PARAM_LPG_DECAY, 0.0, 1.0, 0.5, "Lowpass gate decay", "%", 0.f, 100.f);
		configParam(PARAM_TIMBRE_CV, -1.0, 1.0, 0.0, "Timbre CV");
		configParam(PARAM_FREQUENCY_CV, -1.0, 1.0, 0.0, "Frequency CV");
		configParam(PARAM_MORPH_CV, -1.0, 1.0, 0.0, "Morph CV");

		configInput(INPUT_ENGINE, "Model");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_FREQUENCY, "FM");
		configInput(INPUT_MORPH, "Morph");
		configInput(INPUT_HARMONICS, "Harmonics");
		configInput(INPUT_TRIGGER, "Trigger");
		configInput(INPUT_LEVEL, "Level");
		configInput(INPUT_NOTE, "Pitch (1V/oct)");

		configOutput(OUTPUT_OUT, "Main");
		configOutput(OUTPUT_AUX, "Auxiliary");

		for (int i = 0; i < 16; i++) {
			stmlib::BufferAllocator allocator(shared_buffer[i], sizeof(shared_buffer[i]));
			voice[i].Init(&allocator, &user_data);
		}

		octaveQuantizer.Init(9, 0.01f, false);

		onReset();
	}

	void onReset() override {
		patch.engine = 8;
		setEngine(8);
		patch.lpg_colour = 0.5f;
		patch.decay = 0.5f;
	}

	void onRandomize() override {
		int newEngine;
		newEngine = random::u32() % 24;
		patch.engine = newEngine;
		setEngine(newEngine);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "lowCpu", json_boolean(lowCpu));
		json_object_set_new(rootJ, "displayModulatedModel", json_boolean(displayModulatedModel));
		json_object_set_new(rootJ, "model", json_integer(patch.engine));
		json_object_set_new(rootJ, "frequencyMode", json_integer(frequencyMode));
		json_object_set_new(rootJ, "notesModelSelection", json_boolean(notesModelSelection));

		const uint8_t* userDataBuffer = user_data.getBuffer();
		if (userDataBuffer != nullptr) {
			std::string userDataString = rack::string::toBase64(userDataBuffer, plaits::UserData::MAX_USER_DATA_SIZE);
			json_object_set_new(rootJ, "userData", json_string(userDataString.c_str()));
		}

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* lowCpuJ = json_object_get(rootJ, "lowCpu");
		if (lowCpuJ)
			lowCpu = json_boolean_value(lowCpuJ);

		json_t* displayModulatedModelJ = json_object_get(rootJ, "displayModulatedModel");
		if (displayModulatedModelJ)
			displayModulatedModel = json_boolean_value(displayModulatedModelJ);

		json_t* modelJ = json_object_get(rootJ, "model");
		if (modelJ) {
			patch.engine = json_integer_value(modelJ);
			modelNum = patch.engine;
		}

		json_t* notesModelSelectionJ = json_object_get(rootJ, "notesModelSelection");
		if (notesModelSelectionJ)
			notesModelSelection = json_boolean_value(notesModelSelectionJ);

		json_t* frequencyModeJ = json_object_get(rootJ, "frequencyMode");
		if (frequencyModeJ)
			frequencyMode = json_integer_value(frequencyModeJ);

		json_t* userDataJ = json_object_get(rootJ, "userData");
		if (userDataJ) {
			std::string userDataString = json_string_value(userDataJ);
			const std::vector<uint8_t> userDataVector = rack::string::fromBase64(userDataString);
			if (userDataVector.size() > 0) {
				const uint8_t* userDataBuffer = &userDataVector[0];
				user_data.setBuffer(userDataBuffer);
			}
		}

		// Legacy <=1.0.2
		json_t* lpgColorJ = json_object_get(rootJ, "lpgColor");
		if (lpgColorJ)
			params[PARAM_LPG_COLOR].setValue(json_number_value(lpgColorJ));

		// Legacy <=1.0.2
		json_t* decayJ = json_object_get(rootJ, "decay");
		if (decayJ)
			params[PARAM_LPG_DECAY].setValue(json_number_value(decayJ));
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[INPUT_NOTE].getChannels(), 1);

		if (outputBuffer.empty()) {
			const int blockSize = 12;

			// Switch models
			if (notesModelSelection && inputs[INPUT_ENGINE].isConnected()) {
				float currentModelVoltage = inputs[INPUT_ENGINE].getVoltage();
				if (currentModelVoltage != lastModelVoltage) {
					lastModelVoltage = currentModelVoltage;
					int updatedModel = std::round((lastModelVoltage + 4.f) * 12.f);
					if (updatedModel >= 0 && updatedModel < 24) {
						patch.engine = updatedModel;
						modelNum = patch.engine;
					}
				}
			}
			else if (params[PARAM_MODEL].getValue() != patch.engine) {
				patch.engine = params[PARAM_MODEL].getValue();
				modelNum = patch.engine;
			}

			// Check if engine for first poly channel is different than "base" engine.
			int activeEngine = voice[0].active_engine();
			if (displayModulatedModel && (activeEngine != modelNum && activeEngine >= 0))
				modelNum = activeEngine;

			// Model lights
			// Pulse light at 2 Hz
			triPhase += 2.f * args.sampleTime * blockSize;
			if (triPhase >= 1.f)
				triPhase -= 1.f;
			float tri = (triPhase < 0.5f) ? triPhase * 2.f : (1.f - triPhase) * 2.f;

			// Get the active engines of all voice channels.
			bool activeLights[24] = {};
			bool pulse = false;
			int currentLight;
			int clampedEngine;
			for (int c = 0; c < channels; c++) {
				int activeEngine = voice[c].active_engine();
				clampedEngine = (activeEngine % 8) * 3;

				bool noiseModels = activeEngine & 0x10;
				bool pitchedModels = activeEngine & 0x08;

				if (noiseModels) {
					currentLight = clampedEngine;
					activeLights[currentLight] = true;
				}
				else if (pitchedModels) {
					currentLight = clampedEngine + 1;
					activeLights[currentLight] = true;
				}
				else
				{
					currentLight = clampedEngine;
					activeLights[currentLight] = true;
					currentLight = clampedEngine + 1;
					activeLights[currentLight] = true;
				}

				// Pulse the light if at least one voice is using a different engine.
				if (activeEngine != patch.engine)
					pulse = true;
			}

			// Set model lights.
			clampedEngine = patch.engine % 8;
			for (int i = 0; i < 8; i++) {
				int currentLight = i * 3;
				float brightnessRed = activeLights[currentLight];
				float brightnessGreen = activeLights[currentLight + 1];
				float brightnessBlue = activeLights[currentLight + 2];

				if (pulse && clampedEngine == i) {
					switch (patch.engine) {
					case 0:
					case 1:
					case 2:
					case 3:
					case 4:
					case 5:
					case 6:
					case 7:
						brightnessRed = tri;
						brightnessGreen = tri;
						break;
					case 8:
					case 9:
					case 10:
					case 11:
					case 12:
					case 13:
					case 14:
					case 15:
						brightnessGreen = tri;
						break;
					default:
						brightnessRed = tri;
					}
				}
				// Lights are RGB and need a signal on every pin.
				lights[LIGHT_MODEL + currentLight].setBrightness(brightnessRed);
				lights[LIGHT_MODEL + currentLight + 1].setBrightness(brightnessGreen);
				lights[LIGHT_MODEL + currentLight + 2].setBrightness(brightnessBlue);
			}

			// Calculate pitch for lowCpu mode if needed
			float pitch = params[PARAM_FREQUENCY].getValue();
			if (lowCpu)
				pitch += std::log2(48000.f * args.sampleTime);
			// Update patch

			// Similar implementation to original Plaits ui.cc code.
			// TODO: check with lowCpu mode.
			if (frequencyMode == 0) {
				patch.note = -48.37f + pitch * 15.f;
			}
			else if (frequencyMode == 9) {
				float fineTune = params[PARAM_FREQUENCY_ROOT].getValue() / 4.f;
				patch.note = 53.f + fineTune * 14.f + 12.f * static_cast<float>(octaveQuantizer.Process(0.5f * pitch / 4.f + 0.5f) - 4.f);
			}
			else if (frequencyMode == 10) {
				patch.note = 60.f + pitch * 12.f;
			}
			else {
				patch.note = static_cast<float>(frequencyMode) * 12.f + pitch * 7.f / 4.f;
			}

			patch.harmonics = params[PARAM_HARMONICS].getValue();
			patch.timbre = params[PARAM_TIMBRE].getValue();
			patch.morph = params[PARAM_MORPH].getValue();
			patch.lpg_colour = params[PARAM_LPG_COLOR].getValue();
			patch.decay = params[PARAM_LPG_DECAY].getValue();
			patch.frequency_modulation_amount = params[PARAM_FREQUENCY_CV].getValue();
			patch.timbre_modulation_amount = params[PARAM_TIMBRE_CV].getValue();
			patch.morph_modulation_amount = params[PARAM_MORPH_CV].getValue();

			// Render output buffer for each voice
			dsp::Frame<16 * 2> outputFrames[blockSize];
			for (int c = 0; c < channels; c++) {
				// Construct modulations
				plaits::Modulations modulations;
				if (!notesModelSelection)
					modulations.engine = inputs[INPUT_ENGINE].getPolyVoltage(c) / 5.f;
				modulations.note = inputs[INPUT_NOTE].getVoltage(c) * 12.f;
				modulations.frequency = inputs[INPUT_FREQUENCY].getPolyVoltage(c) * 6.f;
				modulations.harmonics = inputs[INPUT_HARMONICS].getPolyVoltage(c) / 5.f;
				modulations.timbre = inputs[INPUT_TIMBRE].getPolyVoltage(c) / 8.f;
				modulations.morph = inputs[INPUT_MORPH].getPolyVoltage(c) / 8.f;
				// Triggers at around 0.7 V
				modulations.trigger = inputs[INPUT_TRIGGER].getPolyVoltage(c) / 3.f;
				modulations.level = inputs[INPUT_LEVEL].getPolyVoltage(c) / 8.f;

				modulations.frequency_patched = inputs[INPUT_FREQUENCY].isConnected();
				modulations.timbre_patched = inputs[INPUT_TIMBRE].isConnected();
				modulations.morph_patched = inputs[INPUT_MORPH].isConnected();
				modulations.trigger_patched = inputs[INPUT_TRIGGER].isConnected();
				modulations.level_patched = inputs[INPUT_LEVEL].isConnected();

				// Render frames
				plaits::Voice::Frame output[blockSize];
				voice[c].Render(patch, modulations, output, blockSize);

				// Convert output to frames
				for (int i = 0; i < blockSize; i++) {
					outputFrames[i].samples[c * 2 + 0] = output[i].out / 32768.f;
					outputFrames[i].samples[c * 2 + 1] = output[i].aux / 32768.f;
				}
			}

			// Convert output
			if (lowCpu) {
				int len = std::min((int)outputBuffer.capacity(), blockSize);
				std::memcpy(outputBuffer.endData(), outputFrames, len * sizeof(outputFrames[0]));
				outputBuffer.endIncr(len);
			}
			else {
				outputSrc.setRates(48000, (int)args.sampleRate);
				int inLen = blockSize;
				int outLen = outputBuffer.capacity();
				outputSrc.setChannels(channels * 2);
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}
		}

		// Set output
		if (!outputBuffer.empty()) {
			dsp::Frame<16 * 2> outputFrame = outputBuffer.shift();
			for (int c = 0; c < channels; c++) {
				// Inverting op-amp on outputs
				outputs[OUTPUT_OUT].setVoltage(-outputFrame.samples[c * 2 + 0] * 5.f, c);
				outputs[OUTPUT_AUX].setVoltage(-outputFrame.samples[c * 2 + 1] * 5.f, c);
			}
		}
		outputs[OUTPUT_OUT].setChannels(channels);
		outputs[OUTPUT_AUX].setChannels(channels);
	}

	void reset() {
		bool success = user_data.Save(nullptr, patch.engine);
		if (success) {
			for (int c = 0; c < 16; c++) {
				voice[c].ReloadUserData();
			}
		}
	}

	void load(const std::string& path) {
		loading = true;
		DEFER({ loading = false; });
		// HACK Sleep 100us so DSP thread is likely to finish processing before we resize the vector
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));

		std::string ext = string::lowercase(system::getExtension(path));

		if (ext == ".bin") {
			std::ifstream input(path, std::ios::binary);
			std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
			uint8_t* rx_buffer = buffer.data();
			bool success = user_data.Save(rx_buffer, patch.engine);
			if (success) {
				for (int c = 0; c < 16; c++) {
					voice[c].ReloadUserData();
				}
			}
		}
	}

	void loadDialog() {
		osdialog_filters* filters = osdialog_filters_parse(WAVE_FILTERS);
		char* pathC = osdialog_file(OSDIALOG_OPEN, waveDir.empty() ? NULL : waveDir.c_str(), NULL, filters);
		if (!pathC) {
			// Fail silently			
			return;
		}
		const std::string path = pathC;
		std::free(pathC);

		waveDir = system::getDirectory(path);
		load(path);
	}

	void setEngine(int modelNum) {
		params[PARAM_MODEL].setValue(modelNum);
		this->modelNum = modelNum;
	}

	void toggleModulatedDisplay() {
		displayModulatedModel = !displayModulatedModel;
		if (!displayModulatedModel)
			this->modelNum = params[PARAM_MODEL].getValue();
	}

	void toggleNotesModelSelection() {
		notesModelSelection = !notesModelSelection;
		if (notesModelSelection)
			inputs[INPUT_ENGINE].setChannels(0);
		// Try to wait for DSP to finish.
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));
	}
};

static const std::string modelLabels[24] = {
	"Classic waveshapes with filter",
	"Phase distortion",
	"6-operator FM 1",
	"6-operator FM 2",
	"6-operator FM 3",
	"Wave terrain synthesis",
	"String machine",
	"Chiptune",
	"Pair of classic waveforms",
	"Waveshaping oscillator",
	"Two operator FM",
	"Granular formant oscillator",
	"Harmonic oscillator",
	"Wavetable oscillator",
	"Chords",
	"Vowel and speech synthesis",
	"Granular cloud",
	"Filtered noise",
	"Particle noise",
	"Inharmonic string modeling",
	"Modal resonator",
	"Analog bass drum",
	"Analog snare drum",
	"Analog hi-hat",
};

static std::vector<std::string> modelsList{
	"FLTRWAVE",
	"PHASDIST",
	"6 OP.FM1",
	"6 OP.FM2",
	"6 OP.FM3",
	"WAVETRRN",
	"STRGMACH",
	"CHIPTUNE",
	"DUALWAVE",
	"WAVESHAP",
	"2 OP.FM",
	"GRANFORM",
	"HARMONIC",
	"WAVETABL",
	"CHORDS",
	"VOWLSPCH",
	"GR.CLOUD",
	"FLT.NOIS",
	"PRT.NOIS",
	"STG.MODL",
	"MODALRES",
	"BASSDRUM",
	"SNARDRUM",
	"HI-HAT",
};

static const std::string frequencyModes[11] = {
	"LFO mode",
	"C0 +/- 7 semitones",
	"C1 +/- 7 semitones",
	"C2 +/- 7 semitones",
	"C3 +/- 7 semitones",
	"C4 +/- 7 semitones",
	"C5 +/- 7 semitones",
	"C6 +/- 7 semitones",
	"C7 +/- 7 semitones",
	"Octaves",
	"C0 to C8",
};

struct FunesWidget : ModuleWidget {

	FunesWidget(Funes* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/funes_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan2SGray>(mm2px(Vec(133.8, 32.29)), module, Funes::PARAM_MODEL));
		addParam(createParamCentered<Rogan3PSRed>(mm2px(Vec(19.083, 62.502)), module, Funes::PARAM_FREQUENCY));
		addParam(createParamCentered<Rogan3PSGreen>(mm2px(Vec(86.86, 62.502)), module, Funes::PARAM_HARMONICS));
		addParam(createParamCentered<Rogan1PSRed>(mm2px(Vec(120.305, 55.102)), module, Funes::PARAM_TIMBRE));
		addParam(createParamCentered<Rogan1PSGreen>(mm2px(Vec(120.305, 95.968)), module, Funes::PARAM_MORPH));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(142.556, 55.102)), module, Funes::PARAM_TIMBRE_CV));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(142.556, 74.874)), module, Funes::PARAM_FREQUENCY_CV));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(142.556, 95.96)), module, Funes::PARAM_MORPH_CV));

		addParam(createParamCentered<Rogan1PSBlue>(mm2px(Vec(35.8, 89.868)), module, Funes::PARAM_LPG_COLOR));
		addParam(createParamCentered<Rogan1PSBlue>(mm2px(Vec(69.552, 89.868)), module, Funes::PARAM_LPG_DECAY));
		addParam(createParamCentered<Rogan3PSRed>(mm2px(Vec(52.962, 62.502)), module, Funes::PARAM_FREQUENCY_ROOT));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(161.831, 32.29)), module, Funes::INPUT_ENGINE));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(161.831, 55.102)), module, Funes::INPUT_TIMBRE));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(161.831, 74.874)), module, Funes::INPUT_FREQUENCY));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(161.831, 95.968)), module, Funes::INPUT_MORPH));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(97.154, 84.527)), module, Funes::INPUT_HARMONICS));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(14.378, 116.956)), module, Funes::INPUT_TRIGGER));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(27.855, 116.956)), module, Funes::INPUT_LEVEL));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(41.331, 116.956)), module, Funes::INPUT_NOTE));

		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(147.979, 116.956)), module, Funes::OUTPUT_OUT));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(161.831, 116.956)), module, Funes::OUTPUT_AUX));

		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(41.489, 15.85)), module, Funes::LIGHT_MODEL + 0 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(46.489, 15.85)), module, Funes::LIGHT_MODEL + 1 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(51.489, 15.85)), module, Funes::LIGHT_MODEL + 2 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(56.489, 15.85)), module, Funes::LIGHT_MODEL + 3 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(61.489, 15.85)), module, Funes::LIGHT_MODEL + 4 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(66.489, 15.85)), module, Funes::LIGHT_MODEL + 5 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(71.489, 15.85)), module, Funes::LIGHT_MODEL + 6 * 3));
		addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(mm2px(Vec(76.489, 15.85)), module, Funes::LIGHT_MODEL + 7 * 3));

		FramebufferWidget* funesFrambuffer = new FramebufferWidget();
		addChild(funesFrambuffer);

		SanguineAlphaDisplay* alphaDisplay = new SanguineAlphaDisplay();
		alphaDisplay->box.pos = Vec(25, 68);
		alphaDisplay->box.size = Vec(296, 55);
		alphaDisplay->module = module;
		alphaDisplay->itemList = &modelsList;
		alphaDisplay->selectedItem = &module->modelNum;
		funesFrambuffer->addChild(alphaDisplay);

		SanguineShapedLight* mutantsLogo = new SanguineShapedLight();
		mutantsLogo->box.pos = Vec(246.53, 344.31);
		mutantsLogo->box.size = Vec(36.06, 14.79);
		mutantsLogo->module = module;
		mutantsLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/mutants_glowy.svg")));
		funesFrambuffer->addChild(mutantsLogo);

		SanguineShapedLight* bloodLogo = new SanguineShapedLight();
		bloodLogo->box.pos = Vec(220.57, 319.57);
		bloodLogo->box.size = Vec(11.2, 23.27);
		bloodLogo->module = module;
		bloodLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy.svg")));
		funesFrambuffer->addChild(bloodLogo);
	}

	void appendContextMenu(Menu* menu) override {
		Funes* module = dynamic_cast<Funes*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", "", &module->lowCpu));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Frequency mode", "", [=](Menu* menu) {
			for (int i = 0; i < 11; i++) {
				menu->addChild(createCheckMenuItem(frequencyModes[i], "",
					[=]() {return module->frequencyMode == i; },
					[=]() {module->frequencyMode = i; }
				));
			}
			}));

		menu->addChild(new MenuSeparator);

		menu->addChild(createMenuItem("Reset custom data for current engine", "",
			[=]() {module->reset(); }
		));

		menu->addChild(createMenuItem("Load custom data for current engine", "",
			[=]() {module->loadDialog(); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Pitched models", "", [=](Menu* menu) {
			for (int i = 8; i < 16; i++) {
				menu->addChild(createCheckMenuItem(modelLabels[i], "",
					[=]() {return module->patch.engine == i; },
					[=]() {module->patch.engine = i;
				module->setEngine(i); }
				));
			}
			}));

		menu->addChild(createSubmenuItem("Noise/percussive models", "", [=](Menu* menu) {
			for (int i = 16; i < 24; i++) {
				menu->addChild(createCheckMenuItem(modelLabels[i], "",
					[=]() {return module->patch.engine == i; },
					[=]() {module->patch.engine = i;
				module->setEngine(i); }
				));
			}
			}));

		menu->addChild(createSubmenuItem("New synthesis models", "", [=](Menu* menu) {
			for (int i = 0; i < 8; i++) {
				menu->addChild(createCheckMenuItem(modelLabels[i], "",
					[=]() {return module->patch.engine == i; },
					[=]() {module->patch.engine = i;
				module->setEngine(i); }
				));
			}
			}));
		menu->addChild(new MenuSeparator);

		menu->addChild(createCheckMenuItem("Display follows modulated Model", "",
			[=]() {return module->displayModulatedModel; },
			[=]() {module->toggleModulatedDisplay(); }));

		menu->addChild(createCheckMenuItem("C0 model modulation (monophonic)", "",
			[=]() {return module->notesModelSelection; },
			[=]() {module->toggleNotesModelSelection(); }));
	}
};

Model* modelFunes = createModel<Funes, FunesWidget>("Funes");
