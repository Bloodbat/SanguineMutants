#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

#pragma GCC diagnostic push

#ifndef __clang__
#pragma GCC diagnostic ignored "-Wsuggest-override"
#endif

#include "plaits/dsp/voice.h"
#include "plaits/user_data.h"

#pragma GCC diagnostic pop

#include <osdialog.h>
#include <thread>

#include <fstream>

static const char WAVE_FILTERS[] = "BIN (*.bin):bin, BIN";
static std::string waveDir;

static const std::string funesDisplayLabels[24] = {
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

static const std::vector<std::string> frequencyModes = {
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
	"C0 to C8"
};

static const std::vector<std::string> modelLabels = {
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
	"Analog hi-hat"
};

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
		PARAM_FREQ_MODE,
		PARAMS_COUNT
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
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_OUT,
		OUTPUT_AUX,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHT_MODEL, 8 * 2),
		LIGHTS_COUNT
	};

	enum LEDsMode {
		LEDNormal,
		LEDLPG,
		LEDOctave
	};

	plaits::Voice voice[16];
	plaits::Patch patch = {};
	plaits::UserData user_data;
	char shared_buffer[16][16384] = {};

	float triPhase = 0.f;
	float lastLPGColor = 0.5f;
	float lastLPGDecay = 0.5f;

	int frequencyMode = 10;
	int lastFrequencyMode = 10;
	int modelNum = 0;

	uint32_t displayTimeout = 0;
	stmlib::HysteresisQuantizer2 octaveQuantizer;

	dsp::SampleRateConverter<16 * 2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<16 * 2>, 256> outputBuffer;

	bool bLowCpu = false;

	bool bDisplayModulatedModel = true;

	bool bLoading = false;

	bool bNotesModelSelection = false;

	LEDsMode ledsMode = LEDNormal;

	std::string displayText = "";

	float lastModelVoltage = 0.f;

	static const int textUpdateFrequency = 16;
	dsp::ClockDivider clockDivider;

	Funes() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODEL, 0.0f, 23.0f, 8.0f, "Model", modelLabels);
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
		configSwitch(PARAM_FREQ_MODE, 0.f, 10.f, 10.f, "Frequency mode", frequencyModes);
		paramQuantities[PARAM_FREQ_MODE]->snapEnabled = true;

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

		clockDivider.setDivision(textUpdateFrequency);

		onReset();
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[INPUT_NOTE].getChannels(), 1);

		if (outputBuffer.empty()) {
			const int blockSize = 12;

			// Switch models
			if (bNotesModelSelection && inputs[INPUT_ENGINE].isConnected()) {
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
				ledsMode = LEDNormal;
				displayTimeout = 0;
				lastLPGColor = params[PARAM_LPG_COLOR].getValue();
				lastLPGDecay = params[PARAM_LPG_DECAY].getValue();
				lastFrequencyMode = params[PARAM_FREQ_MODE].getValue();
				patch.engine = params[PARAM_MODEL].getValue();
				modelNum = patch.engine;
			}

			// Check if engine for first poly channel is different than "base" engine.
			int activeEngine = voice[0].active_engine();
			if (bDisplayModulatedModel && (activeEngine != modelNum && activeEngine >= 0))
				modelNum = activeEngine;

			// Update model text and frequency mode every 16 samples only.
			if (clockDivider.process()) {
				displayText = funesDisplayLabels[modelNum];

				frequencyMode = params[PARAM_FREQ_MODE].getValue();

				if (frequencyMode != lastFrequencyMode) {
					ledsMode = LEDOctave;
					displayTimeout--;
				}
			}

			// Calculate pitch for low cpu mode if needed
			float pitch = params[PARAM_FREQUENCY].getValue();
			if (bLowCpu)
				pitch += std::log2(48000.f * args.sampleTime);

			// Update patch

			// Similar implementation to original Plaits ui.cc code.
			// TODO: check with low cpu mode.
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

			if (params[PARAM_LPG_COLOR].getValue() != lastLPGColor || params[PARAM_LPG_DECAY].getValue() != lastLPGDecay) {
				ledsMode = LEDLPG;
				displayTimeout--;
			}

			bool activeLights[16] = {};

			bool pulse = false;

			// Render output buffer for each voice
			dsp::Frame<16 * 2> outputFrames[blockSize];
			for (int channel = 0; channel < channels; channel++) {
				// Construct modulations
				plaits::Modulations modulations;
				if (!bNotesModelSelection) {
					modulations.engine = inputs[INPUT_ENGINE].getPolyVoltage(channel) / 5.f;
				}
				modulations.note = inputs[INPUT_NOTE].getVoltage(channel) * 12.f;
				modulations.frequency_patched = inputs[INPUT_FREQUENCY].isConnected();
				modulations.frequency = inputs[INPUT_FREQUENCY].getPolyVoltage(channel) * 6.f;
				modulations.harmonics = inputs[INPUT_HARMONICS].getPolyVoltage(channel) / 5.f;
				modulations.timbre_patched = inputs[INPUT_TIMBRE].isConnected();
				modulations.timbre = inputs[INPUT_TIMBRE].getPolyVoltage(channel) / 8.f;
				modulations.morph_patched = inputs[INPUT_MORPH].isConnected();
				modulations.morph = inputs[INPUT_MORPH].getPolyVoltage(channel) / 8.f;
				modulations.trigger_patched = inputs[INPUT_TRIGGER].isConnected();
				// Triggers at around 0.7 V
				modulations.trigger = inputs[INPUT_TRIGGER].getPolyVoltage(channel) / 3.f;
				modulations.level_patched = inputs[INPUT_LEVEL].isConnected();
				modulations.level = inputs[INPUT_LEVEL].getPolyVoltage(channel) / 8.f;

				// Render frames
				plaits::Voice::Frame output[blockSize];
				voice[channel].Render(patch, modulations, output, blockSize);

				// Convert output to frames
				for (int i = 0; i < blockSize; i++) {
					outputFrames[i].samples[channel * 2 + 0] = output[i].out / 32768.f;
					outputFrames[i].samples[channel * 2 + 1] = output[i].aux / 32768.f;
				}

				if (ledsMode == LEDNormal) {
					// Model lights	
					// Get the active engines for currnet channel.
					int currentLight;
					int clampedEngine;
					int activeEngine = voice[channel].active_engine();
					clampedEngine = (activeEngine % 8) * 2;

					bool noiseModels = activeEngine & 0x10;
					bool pitchedModels = activeEngine & 0x08;

					if (noiseModels) {
						currentLight = clampedEngine + 1;
						activeLights[currentLight] = true;
					}
					else if (pitchedModels) {
						currentLight = clampedEngine;
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
			}

			// Convert output
			if (bLowCpu) {
				int len = std::min((int)outputBuffer.capacity(), blockSize);
				std::memcpy(outputBuffer.endData(), outputFrames, len * sizeof(outputFrames[0]));
				outputBuffer.endIncr(len);
			}
			else {
				outputSrc.setRates(48000, int(args.sampleRate));
				int inLen = blockSize;
				int outLen = outputBuffer.capacity();
				outputSrc.setChannels(channels * 2);
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}

			// Pulse light at 2 Hz
			triPhase += 2.f * args.sampleTime * blockSize;
			if (triPhase >= 1.f) {
				triPhase -= 1.f;
			}
			float tri = (triPhase < 0.5f) ? triPhase * 2.f : (1.f - triPhase) * 2.f;

			switch (ledsMode)
			{
			case LEDNormal: {
				// Set model lights.
				int clampedEngine = patch.engine % 8;
				for (int i = 0; i < 8; i++) {
					int currentLight = i * 2;
					float brightnessRed = activeLights[currentLight + 1];
					float brightnessGreen = activeLights[currentLight];

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
					// Lights are GreenRed and need a signal on each pin.
					lights[LIGHT_MODEL + currentLight].setBrightness(brightnessGreen);
					lights[LIGHT_MODEL + currentLight + 1].setBrightness(brightnessRed);
				}
				break;
			}
			case LEDLPG: {
				for (int parameter = 0; parameter < 2; parameter++) {
					float value = parameter == 0 ? params[PARAM_LPG_COLOR].getValue() : params[PARAM_LPG_DECAY].getValue();
					value *= 100;
					int startLight = (parameter * 4 + 3) * 2;
					startLight = LIGHT_MODEL + startLight;

					float rescaledLight;
					rescaledLight = math::rescale(value, 0.f, 25.f, 0.f, 1.f);
					lights[startLight + 0].setBrightness(value <= 25.f || value >= 25.f ? rescaledLight : 0.f);
					lights[startLight + 1].setBrightness(value <= 25.f || value >= 25.f ? rescaledLight : 0.f);
					startLight -= 2;
					rescaledLight = math::rescale(value, 25.1f, 50.f, 0.f, 1.f);
					lights[startLight + 0].setBrightness(value >= 25.1f ? rescaledLight : 0.f);
					lights[startLight + 1].setBrightness(value >= 25.1f ? rescaledLight : 0.f);
					startLight -= 2;
					rescaledLight = math::rescale(value, 50.1f, 75.f, 0.f, 1.f);
					lights[startLight + 0].setBrightness(value >= 75.1f ? rescaledLight : 0.f);
					lights[startLight + 1].setBrightness(value >= 75.1f ? rescaledLight : 0.f);
					startLight -= 2;
					rescaledLight = math::rescale(value, 75.1f, 100.f, 0.f, 1.f);
					lights[startLight + 0].setBrightness(value >= 75.1f ? rescaledLight : 0.f);
					lights[startLight + 1].setBrightness(value >= 75.1f ? rescaledLight : 0.f);
				}
				break;
			}
			case LEDOctave: {
				int octave = params[PARAM_FREQ_MODE].getValue();
				for (int i = 0; i < 8; i++) {
					float ledValue = 0.f;
					int triangle = tri;

					if (octave == 0) {
						ledValue = i == (triangle >> 1) ? 0.f : 1.f;
					}
					else if (octave == 10) {
						ledValue = 1.f;
					}
					else if (octave == 9) {
						ledValue = (i & 1) == ((triangle >> 3) & 1) ? 0.f : 1.f;
					}
					else {
						ledValue = (octave - 1) == i ? 1.f : 0.f;
					}
					lights[(LIGHT_MODEL + 7 * 2) - i * 2 + 0].setBrightness(ledValue);
					lights[(LIGHT_MODEL + 7 * 2) - i * 2 + 1].setBrightness(ledValue);
				}
				break;
			}
			}
		}

		if (ledsMode != LEDNormal) {
			displayTimeout++;
		}
		if (displayTimeout > args.sampleRate * 3) {
			ledsMode = LEDNormal;
			displayTimeout = 0;
			lastLPGColor = params[PARAM_LPG_COLOR].getValue();
			lastLPGDecay = params[PARAM_LPG_DECAY].getValue();
			lastFrequencyMode = params[PARAM_FREQ_MODE].getValue();
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

	void onReset() override {
		setEngine(8);
		patch.lpg_colour = 0.5f;
		patch.decay = 0.5f;
	}

	void onRandomize() override {
		int newEngine;
		newEngine = random::u32() % 24;
		setEngine(newEngine);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "lowCpu", json_boolean(bLowCpu));
		json_object_set_new(rootJ, "displayModulatedModel", json_boolean(bDisplayModulatedModel));
		json_object_set_new(rootJ, "frequencyMode", json_integer(frequencyMode));
		json_object_set_new(rootJ, "notesModelSelection", json_boolean(bNotesModelSelection));

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
			bLowCpu = json_boolean_value(lowCpuJ);

		json_t* displayModulatedModelJ = json_object_get(rootJ, "displayModulatedModel");
		if (displayModulatedModelJ)
			bDisplayModulatedModel = json_boolean_value(displayModulatedModelJ);

		json_t* notesModelSelectionJ = json_object_get(rootJ, "notesModelSelection");
		if (notesModelSelectionJ)
			bNotesModelSelection = json_boolean_value(notesModelSelectionJ);

		json_t* frequencyModeJ = json_object_get(rootJ, "frequencyMode");
		if (frequencyModeJ)
			setFrequencyMode(json_integer_value(frequencyModeJ));

		json_t* userDataJ = json_object_get(rootJ, "userData");
		if (userDataJ) {
			std::string userDataString = json_string_value(userDataJ);
			const std::vector<uint8_t> userDataVector = rack::string::fromBase64(userDataString);
			if (userDataVector.size() > 0) {
				const uint8_t* userDataBuffer = &userDataVector[0];
				user_data.setBuffer(userDataBuffer);
			}
		}
	}

	void customDataReset() {
		bool success = user_data.Save(nullptr, patch.engine);
		if (success) {
			for (int c = 0; c < 16; c++) {
				voice[c].ReloadUserData();
			}
		}
	}

	void customDataLoad(const std::string& path) {
		bLoading = true;
		DEFER({ bLoading = false; });
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

	void customDataShowLoadDialog() {
		osdialog_filters* filters = osdialog_filters_parse(WAVE_FILTERS);
		char* pathC = osdialog_file(OSDIALOG_OPEN, waveDir.empty() ? NULL : waveDir.c_str(), NULL, filters);
		if (!pathC) {
			// Fail silently			
			return;
		}
		const std::string path = pathC;
		std::free(pathC);

		waveDir = system::getDirectory(path);
		customDataLoad(path);
	}

	void setEngine(int newModelNum) {
		params[PARAM_MODEL].setValue(newModelNum);
		modelNum = newModelNum;
		patch.engine = newModelNum;
	}

	void toggleModulatedDisplay() {
		bDisplayModulatedModel = !bDisplayModulatedModel;
		if (!bDisplayModulatedModel)
			this->modelNum = params[PARAM_MODEL].getValue();
	}

	void toggleNotesModelSelection() {
		bNotesModelSelection = !bNotesModelSelection;
		if (bNotesModelSelection)
			inputs[INPUT_ENGINE].setChannels(0);
		// Try to wait for DSP to finish.
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));
	}

	void setFrequencyMode(int freqModeNum) {
		frequencyMode = freqModeNum;
		params[PARAM_FREQ_MODE].setValue(freqModeNum);
	}
};

struct FunesWidget : ModuleWidget {

	FunesWidget(Funes* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_34hp_purple.svg", "res/funes_faceplate.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan2SGray>(millimetersToPixelsVec(133.8, 32.306), module, Funes::PARAM_MODEL));
		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(19.083, 67.293), module, Funes::PARAM_FREQUENCY));
		addParam(createParamCentered<Sanguine3PSGreen>(millimetersToPixelsVec(86.86, 67.293), module, Funes::PARAM_HARMONICS));
		addParam(createParamCentered<Sanguine1PSRed>(millimetersToPixelsVec(120.305, 55.118), module, Funes::PARAM_TIMBRE));
		addParam(createParamCentered<Sanguine1PSGreen>(millimetersToPixelsVec(120.305, 95.975), module, Funes::PARAM_MORPH));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(142.556, 55.106), module, Funes::PARAM_TIMBRE_CV));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(142.556, 74.878), module, Funes::PARAM_FREQUENCY_CV));
		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(142.556, 95.964), module, Funes::PARAM_MORPH_CV));

		addParam(createParamCentered<Sanguine1PSPurple>(millimetersToPixelsVec(19.083, 89.884), module, Funes::PARAM_FREQ_MODE));
		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(52.962, 89.884), module, Funes::PARAM_LPG_COLOR));
		addParam(createParamCentered<Sanguine1PSBlue>(millimetersToPixelsVec(86.86, 89.884), module, Funes::PARAM_LPG_DECAY));
		addParam(createParamCentered<Sanguine3PSRed>(millimetersToPixelsVec(52.962, 67.293), module, Funes::PARAM_FREQUENCY_ROOT));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(161.831, 32.306), module, Funes::INPUT_ENGINE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(161.831, 55.118), module, Funes::INPUT_TIMBRE));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(161.831, 74.89), module, Funes::INPUT_FREQUENCY));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(161.831, 95.976), module, Funes::INPUT_MORPH));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(120.305, 74.878), module, Funes::INPUT_HARMONICS));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(14.378, 116.972), module, Funes::INPUT_TRIGGER));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(27.855, 116.972), module, Funes::INPUT_LEVEL));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(41.331, 116.972), module, Funes::INPUT_NOTE));

		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(147.979, 116.972), module, Funes::OUTPUT_OUT));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(161.831, 116.972), module, Funes::OUTPUT_AUX));

		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(41.489, 14.41), module, Funes::LIGHT_MODEL + 0 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(46.489, 14.41), module, Funes::LIGHT_MODEL + 1 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(51.489, 14.41), module, Funes::LIGHT_MODEL + 2 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(56.489, 14.41), module, Funes::LIGHT_MODEL + 3 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(61.489, 14.41), module, Funes::LIGHT_MODEL + 4 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(66.489, 14.41), module, Funes::LIGHT_MODEL + 5 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(71.489, 14.41), module, Funes::LIGHT_MODEL + 6 * 2));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(millimetersToPixelsVec(76.489, 14.41), module, Funes::LIGHT_MODEL + 7 * 2));

		FramebufferWidget* funesFrambuffer = new FramebufferWidget();
		addChild(funesFrambuffer);

		SanguineAlphaDisplay* alphaDisplay = new SanguineAlphaDisplay(8, module, 59.074, 32.314);
		funesFrambuffer->addChild(alphaDisplay);
		alphaDisplay->fallbackString = funesDisplayLabels[8];

		if (module) {
			alphaDisplay->values.displayText = &module->displayText;
		}

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 76.596, 112.027);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 89.597, 118.96);
		addChild(mutantsLogo);
	}

	void appendContextMenu(Menu* menu) override {
		Funes* module = dynamic_cast<Funes*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createCheckMenuItem("Low CPU (disable resampling)", "",
			[=]() {return module->bLowCpu; },
			[=]() {module->bLowCpu = !module->bLowCpu; }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Frequency mode", "", [=](Menu* menu) {
			for (int i = 0; i < 11; i++) {
				menu->addChild(createCheckMenuItem(frequencyModes[i], "",
					[=]() {return module->frequencyMode == i; },
					[=]() {module->setFrequencyMode(i); }
				));
			}
			}));

		menu->addChild(new MenuSeparator);

		menu->addChild(createMenuItem("Reset custom data for current engine", "",
			[=]() {module->customDataReset(); }
		));

		menu->addChild(createMenuItem("Load custom data for current engine", "",
			[=]() {module->customDataShowLoadDialog(); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Pitched models", "", [=](Menu* menu) {
			for (int i = 8; i < 16; i++) {
				menu->addChild(createCheckMenuItem(modelLabels[i], "",
					[=]() { return module->patch.engine == i; },
					[=]() {	module->setEngine(i); }
				));
			}
			}));

		menu->addChild(createSubmenuItem("Noise/percussive models", "", [=](Menu* menu) {
			for (int i = 16; i < 24; i++) {
				menu->addChild(createCheckMenuItem(modelLabels[i], "",
					[=]() { return module->patch.engine == i; },
					[=]() { module->setEngine(i); }
				));
			}
			}));

		menu->addChild(createSubmenuItem("New synthesis models", "", [=](Menu* menu) {
			for (int i = 0; i < 8; i++) {
				menu->addChild(createCheckMenuItem(modelLabels[i], "",
					[=]() { return module->patch.engine == i; },
					[=]() {	module->setEngine(i); }
				));
			}
			}));
		menu->addChild(new MenuSeparator);

		menu->addChild(createCheckMenuItem("Display follows modulated Model", "",
			[=]() {return module->bDisplayModulatedModel; },
			[=]() {module->toggleModulatedDisplay(); }));

		menu->addChild(createCheckMenuItem("C0 model modulation (monophonic)", "",
			[=]() {return module->bNotesModelSelection; },
			[=]() {module->toggleNotesModelSelection(); }));
	}
};

Model* modelFunes = createModel<Funes, FunesWidget>("Funes");