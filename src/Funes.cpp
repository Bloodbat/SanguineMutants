#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinechannels.hpp"
#include "sanguinejson.hpp"

#ifndef METAMODULE
#include "osdialog.h"
#include <fstream>
#include <thread>
#else
#include "async_filebrowser.hh"
#endif

#include "plaits/dsp/voice.h"
#include "plaits/user_data.h"

#include "Funes.hpp"

using namespace sanguineCommonCode;

struct Funes : SanguineModule {
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
		PARAM_HARMONICS_CV,
		PARAM_LPG_COLOR_CV,
		PARAM_LPG_DECAY_CV,
		PARAM_CHORD_BANK,
		PARAM_AUX_CROSSFADE,
		PARAM_AUX_SUBOSCILLATOR,
		PARAM_HOLD_MODULATIONS,
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
		INPUT_LGP_COLOR,
		INPUT_LPG_DECAY,
		INPUT_AUX_CROSSFADE,
		INPUTS_COUNT
	};

	enum OutputIds {
		OUTPUT_OUT,
		OUTPUT_AUX,
		OUTPUTS_COUNT
	};

	enum LightIds {
		ENUMS(LIGHT_MODEL, 8 * 2),
		LIGHT_FACTORY_DATA,
		ENUMS(LIGHT_CUSTOM_DATA, 2),
		ENUMS(LIGHT_CHORD_BANK, 2),
		ENUMS(LIGHT_AUX_SUBOSCILLATOR, 3),
		LIGHT_HOLD_MODULATIONS,
		LIGHTS_COUNT
	};

	plaits::Voice voices[PORT_MAX_CHANNELS];
	plaits::Patch patch = {};
	plaits::Modulations modulations[PORT_MAX_CHANNELS];
	plaits::UserData userData;
	// Hardware uses 16384; but new chords crash Rack on mode change if left as such.
	char sharedBuffers[PORT_MAX_CHANNELS][50176] = {};

	float triPhase = 0.f;
	float lastModelVoltage = 0.f;

	static const int kLightsFrequency = 16;

	static const int kBlockSize = 12;

	static const int kModelLightsCount = 8;

	int frequencyMode = funes::FM_FULL;
	int displayModelNum = 0;

	int displayChannel = 0;

	int channelCount = 0;
	int errorTimeOut = 0;

	uint8_t chordBank = 0;

	funes::SuboscillatorModes suboscillatorMode = funes::SUBOSCILLATOR_OFF;

	stmlib::HysteresisQuantizer2 octaveQuantizer;

	dsp::SampleRateConverter<PORT_MAX_CHANNELS * 2> srcOutputs;
	dsp::DoubleRingBuffer<dsp::Frame<PORT_MAX_CHANNELS * 2>, 256> drbOutputBuffers;

	bool bWantLowCpu = false;

	bool bDisplayModulatedModel = true;

	bool bIsLoading = false;

	bool bNotesModelSelection = false;

	bool bWantHoldModulations = false;

	bool bHaveInputEngine = false;
	bool bHaveInputFrequency = false;
	bool bHaveInputLevel = false;
	bool bHaveInputTimbre = false;
	bool bHaveInputMorph = false;
	bool bHaveInputTrigger = false;

	std::string displayText = "";

	dsp::ClockDivider lightsDivider;

	funes::CustomDataStates customDataStates[plaits::kMaxEngines] = {};

	Funes() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODEL, 0.f, 23.f, 8.f, "Model", funes::modelLabels);

		configParam(PARAM_FREQUENCY, -4.f, 4.f, 0.f, "Frequency", " semitones", 0.f, 12.f);
		configParam(PARAM_FREQUENCY_ROOT, -4.f, 4.f, 0.f, "Frequency Root", " semitones", 0.f, 12.f);
		configParam(PARAM_HARMONICS, 0.f, 1.f, 0.5f, "Harmonics", "%", 0.f, 100.f);
		configParam(PARAM_TIMBRE, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);
		configParam(PARAM_LPG_COLOR, 0.f, 1.f, 0.5f, "Lowpass gate response", "%", 0.f, 100.f);
		configParam(PARAM_MORPH, 0.f, 1.f, 0.5f, "Morph", "%", 0.f, 100.f);
		configParam(PARAM_LPG_DECAY, 0.f, 1.f, 0.5f, "Lowpass gate decay", "%", 0.f, 100.f);
		configParam(PARAM_TIMBRE_CV, -1.f, 1.f, 0.f, "Timbre CV", "%", 0.f, 100.f);
		configParam(PARAM_FREQUENCY_CV, -1.f, 1.f, 0.f, "Frequency CV", "%", 0.f, 100.f);
		configParam(PARAM_MORPH_CV, -1.f, 1.f, 0.f, "Morph CV", "%", 0.f, 100.f);
		configSwitch(PARAM_FREQ_MODE, 0.f, 10.f, 10.f, "Frequency mode", funes::frequencyModeLabels);
		configParam(PARAM_HARMONICS_CV, -1.f, 1.f, 0.f, "Harmonics CV", "%", 0.f, 100.f);
		configParam(PARAM_LPG_COLOR_CV, -1.f, 1.f, 0.f, "Lowpass gate response CV", "%", 0.f, 100.f);
		configParam(PARAM_LPG_DECAY_CV, -1.f, 1.f, 0.f, "Lowpass gate decay CV", "%", 0.f, 100.f);

		configSwitch(PARAM_HOLD_MODULATIONS, 0.f, 1.f, 0.f, "Hold modulation voltages on trigger");

		configSwitch(PARAM_CHORD_BANK, 0.f, 2.f, 0.f, "Chord bank", { funes::chordBankLabels });

		configParam(PARAM_AUX_CROSSFADE, 0.f, 1.f, 0.f, "Main Out → Aux crossfader", "%", 0.f, 100.f);

		configSwitch(PARAM_AUX_SUBOSCILLATOR, 0.f, 4.f, 0.f, "Aux sub-oscillator", { funes::suboscillatorLabels });

		configInput(INPUT_ENGINE, "Model");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_FREQUENCY, "FM");
		configInput(INPUT_MORPH, "Morph");
		configInput(INPUT_HARMONICS, "Harmonics");
		configInput(INPUT_TRIGGER, "Trigger");
		configInput(INPUT_LEVEL, "Level");
		configInput(INPUT_NOTE, "Pitch (1V/oct)");
		configInput(INPUT_LGP_COLOR, "Lowpass gate response");
		configInput(INPUT_LPG_DECAY, "Lowpass gate decay");

		configInput(INPUT_AUX_CROSSFADE, "Main Out → Aux crossfader");

		configOutput(OUTPUT_OUT, "Main");
		configOutput(OUTPUT_AUX, "Auxiliary");

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			stmlib::BufferAllocator allocator(sharedBuffers[channel], sizeof(sharedBuffers[channel]));
			voices[channel].Init(&allocator, &userData);
		}

		octaveQuantizer.Init(9, 0.01f, false);

		lightsDivider.setDivision(kLightsFrequency);

		resetCustomDataStates();

		init();
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		channelCount = std::max(std::max(inputs[INPUT_NOTE].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

		chordBank = params[PARAM_CHORD_BANK].getValue();

		bWantHoldModulations = static_cast<bool>(params[PARAM_HOLD_MODULATIONS].getValue());

		if (drbOutputBuffers.empty()) {
			int knobModel = static_cast<int>(params[PARAM_MODEL].getValue());

			// Switch models
			if (bHaveInputEngine && bNotesModelSelection) {
				float currentModelVoltage = inputs[INPUT_ENGINE].getVoltage();
				if (currentModelVoltage != lastModelVoltage) {
					lastModelVoltage = currentModelVoltage;
					int updatedModel = std::round((lastModelVoltage + 4.f) * 12.f);
					if (updatedModel >= 0 && updatedModel < 24) {
						patch.engine = updatedModel;
						displayModelNum = patch.engine;
					}
				}
			} else if (knobModel != patch.engine) {
				patch.engine = knobModel;
				displayModelNum = patch.engine;
			}

			// Calculate pitch for low cpu mode, if needed.
			float pitch = params[PARAM_FREQUENCY].getValue();
			if (bWantLowCpu) {
				pitch += std::log2(48000.f * args.sampleTime);
			}

			// Update patch

			// Similar implementation to original Plaits ui.cc code.
			// TODO: Check with low cpu mode.
			switch (frequencyMode) {
			case funes::FM_LFO:
				patch.note = -48.37f + pitch * 15.f;
				break;
			case funes::FM_OCTAVES: {
				float fineTune = params[PARAM_FREQUENCY_ROOT].getValue() / 4.f;
				patch.note = 53.f + fineTune * 14.f + 12.f * static_cast<float>(octaveQuantizer.Process(0.5f * pitch / 4.f + 0.5f) - 4.f);
				break;
			}
			case funes::FM_FULL:
				patch.note = 60.f + pitch * 12.f;
				break;
			default:
				patch.note = static_cast<float>(frequencyMode) * 12.f + pitch * 7.f / 4.f;
				break;
			}

			patch.harmonics = params[PARAM_HARMONICS].getValue();
			patch.timbre = params[PARAM_TIMBRE].getValue();
			patch.morph = params[PARAM_MORPH].getValue();
			patch.lpg_colour = clamp(params[PARAM_LPG_COLOR].getValue() + ((inputs[INPUT_LGP_COLOR].getVoltage() / 5.f) *
				params[PARAM_LPG_COLOR_CV].getValue()), 0.f, 1.f);
			patch.decay = clamp(params[PARAM_LPG_DECAY].getValue() + ((inputs[INPUT_LPG_DECAY].getVoltage() / 5.f) *
				params[PARAM_LPG_DECAY_CV].getValue()), 0.f, 1.f);
			patch.frequency_modulation_amount = params[PARAM_FREQUENCY_CV].getValue();
			patch.timbre_modulation_amount = params[PARAM_TIMBRE_CV].getValue();
			patch.morph_modulation_amount = params[PARAM_MORPH_CV].getValue();
			patch.chordBank = chordBank;
			patch.auxCrossfade = params[PARAM_AUX_CROSSFADE].getValue();
			suboscillatorMode = static_cast<funes::SuboscillatorModes>(params[PARAM_AUX_SUBOSCILLATOR].getValue());
			bool bIsSubOscillatorOctave = suboscillatorMode > funes::SUBOSCILLATOR_SINE;
			patch.auxSuboscillatorWave = bIsSubOscillatorOctave ? funes::SUBOSCILLATOR_SINE : suboscillatorMode;
			patch.auxSuboscillatorOctave = (suboscillatorMode - 2) * bIsSubOscillatorOctave;

			patch.wantHoldModulations = bWantHoldModulations;

			bool activeLights[kModelLightsCount * 2] = {};

			bool bPulseLight = false;

			// Render output buffer for each voice.
			float attenHarmonics = params[PARAM_HARMONICS_CV].getValue();
			dsp::Frame<PORT_MAX_CHANNELS * 2> renderFrames[kBlockSize];
			for (int channel = 0; channel < channelCount; ++channel) {
				float_4 inputVoltages;

				inputVoltages[0] = inputs[INPUT_ENGINE].getVoltage(channel);
				inputVoltages[1] = inputs[INPUT_HARMONICS].getVoltage(channel);
				inputVoltages[2] = inputs[INPUT_AUX_CROSSFADE].getVoltage(channel);

				inputVoltages /= 5.f;

				// Construct modulations.
				if (!bNotesModelSelection) {
					modulations[channel].engine = inputVoltages[0];
				}
				modulations[channel].harmonics = inputVoltages[1] * attenHarmonics;
				modulations[channel].auxCrossfade = inputVoltages[2];

				inputVoltages[0] = inputs[INPUT_TIMBRE].getVoltage(channel);
				inputVoltages[1] = inputs[INPUT_MORPH].getVoltage(channel);
				inputVoltages[2] = inputs[INPUT_LEVEL].getVoltage(channel);

				inputVoltages /= 8.f;

				modulations[channel].timbre = inputVoltages[0];
				modulations[channel].morph = inputVoltages[1];
				modulations[channel].level = inputVoltages[2];

				modulations[channel].note = inputs[INPUT_NOTE].getVoltage(channel) * 12.f;
				modulations[channel].frequency = inputs[INPUT_FREQUENCY].getVoltage(channel) * 6.f;

				// Triggers at around 0.7 V
				modulations[channel].trigger = inputs[INPUT_TRIGGER].getVoltage(channel) / 3.f;

				modulations[channel].frequency_patched = bHaveInputFrequency;
				modulations[channel].level_patched = bHaveInputLevel;
				modulations[channel].timbre_patched = bHaveInputTimbre;
				modulations[channel].morph_patched = bHaveInputMorph;
				modulations[channel].trigger_patched = bHaveInputTrigger;

				// Render frames
				plaits::Voice::Frame output[kBlockSize];
				voices[channel].Render(patch, modulations[channel], output, kBlockSize);

				// Convert output to frames
				const int channelFrame = channel * 2;
				for (int blockNum = 0; blockNum < kBlockSize; ++blockNum) {
					renderFrames[blockNum].samples[channelFrame] = output[blockNum].out / 32768.f;
					renderFrames[blockNum].samples[channelFrame + 1] = output[blockNum].aux / 32768.f;
				}

				int activeEngine = voices[channel].active_engine();

				if (displayChannel == channel) {
					displayModelNum = activeEngine;
				}

				// Model lights
				// Get the active engine for current channel.
				int clampedEngine = (activeEngine % 8) * 2;

				activeLights[clampedEngine] = activeEngine < 16;
				activeLights[clampedEngine + 1] = (activeEngine & 0x10) | (activeEngine < 8);

				// Pulse the light if at least one voice is using a different engine.
				bPulseLight = activeEngine != patch.engine;
			}

			// Convert output.
			if (!bWantLowCpu) {
				int inLen = kBlockSize;
				int outLen = drbOutputBuffers.capacity();
				srcOutputs.setChannels(channelCount * 2);
				srcOutputs.process(renderFrames, &inLen, drbOutputBuffers.endData(), &outLen);
				drbOutputBuffers.endIncr(outLen);
			} else {
				int len = std::min(static_cast<int>(drbOutputBuffers.capacity()), kBlockSize);
				std::memcpy(drbOutputBuffers.endData(), renderFrames, len * sizeof(renderFrames[0]));
				drbOutputBuffers.endIncr(len);
			}

			// Pulse light at 2 Hz.
			triPhase += 2.f * args.sampleTime * kBlockSize;
			if (triPhase >= 1.f) {
				triPhase -= 1.f;
			}
			const float tri = (triPhase < 0.5f) ? triPhase * 2.f : (1.f - triPhase) * 2.f;

			// Set model lights.
			const int baseEngine = patch.engine;
			const int clampedEngine = baseEngine % 8;
			for (int led = 0; led < kModelLightsCount; ++led) {
				const int currentLight = led * 2;
				float brightnessRed = static_cast<float>(activeLights[currentLight + 1]);
				float brightnessGreen = static_cast<float>(activeLights[currentLight]);

				if (bPulseLight && clampedEngine == led) {
					brightnessRed = ((baseEngine < 8) | (baseEngine & 0x10)) * tri;
					brightnessGreen = ((baseEngine < 16)) * tri;
				}
				// Lights are GreenRed and need a signal on each pin.
				lights[LIGHT_MODEL + currentLight].setBrightness(brightnessGreen);
				lights[LIGHT_MODEL + currentLight + 1].setBrightness(brightnessRed);
			}
		}

		// Set output
		if (!drbOutputBuffers.empty()) {
			dsp::Frame<PORT_MAX_CHANNELS * 2> outputFrames = drbOutputBuffers.shift();
			int currentSample;
			for (int channel = 0; channel < channelCount; ++channel) {
				currentSample = channel * 2;
				// Inverting op-amp on outputs
				outputs[OUTPUT_OUT].setVoltage(-outputFrames.samples[currentSample] * 5.f, channel);
				outputs[OUTPUT_AUX].setVoltage(-outputFrames.samples[currentSample + 1] * 5.f, channel);
			}
		}
		outputs[OUTPUT_OUT].setChannels(channelCount);
		outputs[OUTPUT_AUX].setChannels(channelCount);

		// Update model text, custom data lights, chord bank light and frequency mode every 16 samples only.
		if (lightsDivider.process()) {
			if (displayChannel >= channelCount) {
				displayChannel = channelCount - 1;
			}

			if (bDisplayModulatedModel) {
				displayText = funes::displayLabels[displayModelNum];
			}

			frequencyMode = params[PARAM_FREQ_MODE].getValue();

			lights[LIGHT_FACTORY_DATA].setBrightness(((customDataStates[patch.engine] == funes::DataFactory) &
				(errorTimeOut == 0)) * kSanguineButtonLightValue);
			lights[LIGHT_CUSTOM_DATA].setBrightness(((customDataStates[patch.engine] == funes::DataCustom) &
				(errorTimeOut == 0)) * kSanguineButtonLightValue);
			lights[LIGHT_CUSTOM_DATA + 1].setBrightness(((customDataStates[patch.engine] == funes::DataCustom) |
				(errorTimeOut > 0)) * kSanguineButtonLightValue);

			if (errorTimeOut != 0) {
				--errorTimeOut;

				if (errorTimeOut <= 0) {
					errorTimeOut = 0;
				}
			}

			bool bEngineHasChords = (patch.engine == 6) | (patch.engine == 7) | (patch.engine == 14);

			lights[LIGHT_CHORD_BANK].setBrightness((bEngineHasChords & ((chordBank == 0) | (chordBank == 2))) *
				kSanguineButtonLightValue);
			lights[LIGHT_CHORD_BANK + 1].setBrightness((bEngineHasChords & (chordBank > 0)) * kSanguineButtonLightValue);

			lights[LIGHT_AUX_SUBOSCILLATOR].setBrightness((suboscillatorMode > funes::SUBOSCILLATOR_SQUARE) *
				kSanguineButtonLightValue);

			lights[LIGHT_AUX_SUBOSCILLATOR + 1].setBrightness(((suboscillatorMode == funes::SUBOSCILLATOR_SQUARE) |
				(suboscillatorMode == funes::SUBOSCILLATOR_SINE_MINUS_ONE)) * kSanguineButtonLightValue);

			lights[LIGHT_AUX_SUBOSCILLATOR + 2].setBrightness((suboscillatorMode == funes::SUBOSCILLATOR_SINE_MINUS_TWO) *
				kSanguineButtonLightValue);

			lights[LIGHT_HOLD_MODULATIONS].setBrightness(bWantHoldModulations * kSanguineButtonLightValue);
		}
	}

	void init() {
		setEngine(8);
		patch.lpg_colour = 0.5f;
		patch.decay = 0.5f;

		params[PARAM_LPG_COLOR].setValue(0.5f);
		params[PARAM_LPG_DECAY].setValue(0.5f);
	}

	void onReset(const ResetEvent& e) override {
		init();
	}

	void onRandomize(const RandomizeEvent& e) override {
		int newEngine;
		newEngine = random::u32() % 24;
		setEngine(newEngine);
	}

	void onPortChange(const PortChangeEvent& e) override {
		if (e.type == Port::INPUT) {
			switch (e.portId) {
			case INPUT_ENGINE:
				bHaveInputEngine = e.connecting;
				break;
			case INPUT_FREQUENCY:
				bHaveInputFrequency = e.connecting;
				break;
			case INPUT_LEVEL:
				bHaveInputLevel = e.connecting;
				break;
			case INPUT_TIMBRE:
				bHaveInputTimbre = e.connecting;
				break;
			case INPUT_MORPH:
				bHaveInputMorph = e.connecting;
				break;
			case INPUT_TRIGGER:
				bHaveInputTrigger = e.connecting;
				break;
			default:
				break;
			}
		}
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		srcOutputs.setRates(48000, static_cast<int>(e.sampleRate));
	}

	json_t* dataToJson() override {
		json_t* rootJ = SanguineModule::dataToJson();

		setJsonBoolean(rootJ, "lowCpu", bWantLowCpu);
		setJsonBoolean(rootJ, "displayModulatedModel", bDisplayModulatedModel);
		setJsonInt(rootJ, "frequencyMode", frequencyMode);
		setJsonBoolean(rootJ, "notesModelSelection", bNotesModelSelection);
		setJsonInt(rootJ, "displayChannel", displayChannel);

		const uint8_t* userDataBuffer = userData.getBuffer();
		if (userDataBuffer != nullptr) {
			std::string userDataString = rack::string::toBase64(userDataBuffer, plaits::UserData::MAX_USER_DATA_SIZE);
			setJsonString(rootJ, "userData", userDataString.c_str());
		}

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_int_t intValue = 0;

		getJsonBoolean(rootJ, "lowCpu", bWantLowCpu);
		getJsonBoolean(rootJ, "displayModulatedModel", bDisplayModulatedModel);
		getJsonBoolean(rootJ, "notesModelSelection", bNotesModelSelection);

		if (getJsonInt(rootJ, "frequencyMode", intValue)) {
			setFrequencyMode(intValue);
		}
		if (getJsonInt(rootJ, "displayChannel", intValue)) {
			displayChannel = intValue;
		}

		std::string userDataString;

		if (getJsonString(rootJ, "userData", userDataString)) {
			const std::vector<uint8_t> userDataVector = rack::string::fromBase64(userDataString);
			if (userDataVector.size() > 0) {
				const uint8_t* userDataBuffer = &userDataVector[0];
				userData.setBuffer(userDataBuffer);
				if (userDataBuffer[plaits::UserData::MAX_USER_DATA_SIZE - 2] == 'U') {
					for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
						voices[channel].ReloadUserData();
					}

					resetCustomDataStates();
					customDataStates[userDataBuffer[plaits::UserData::MAX_USER_DATA_SIZE - 1] - ' '] = funes::DataCustom;
				}
			}
		}
	}

	void resetCustomData() {
		bool success = userData.Save(nullptr, patch.engine);
		if (success) {
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				voices[channel].ReloadUserData();
			}
			resetCustomDataStates();
		}
	}

	void loadCustomData(const std::string& filePath) {
		bIsLoading = true;
		DEFER({ bIsLoading = false; });
		// HACK: Sleep 100us so DSP thread is likely to finish processing before we resize the vector.
#ifndef METAMODULE
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));
#else
		// delay_ms(1);
#endif
		std::string fileExtension = string::lowercase(system::getExtension(filePath));

		if (fileExtension == ".bin") {
			std::vector<uint8_t> buffer = system::readFile(filePath);
			const uint8_t* dataBuffer = buffer.data();
			bool success = userData.Save(dataBuffer, patch.engine);
			if (success) {
				for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
					voices[channel].ReloadUserData();
				}
				// Only 1 engine can use custom data at a time.
				resetCustomDataStates();
				customDataStates[patch.engine] = funes::DataCustom;
			} else {
				errorTimeOut = 4;
			}
		}
	}

	void resetCustomDataStates() {
		customDataStates[2] = funes::DataFactory;
		customDataStates[3] = funes::DataFactory;
		customDataStates[4] = funes::DataFactory;
		customDataStates[5] = funes::DataFactory;
		customDataStates[13] = funes::DataFactory;
	}

	void showCustomDataLoadDialog() {
#ifndef METAMODULE
#ifndef USING_CARDINAL_NOT_RACK
		osdialog_filters* filters = osdialog_filters_parse(funes::CUSTOM_DATA_FILENAME_FILTERS);

		DEFER({ osdialog_filters_free(filters); });

		char* dialogFilePath = osdialog_file(OSDIALOG_OPEN, funes::customDataDir.empty() ? NULL : funes::customDataDir.c_str(), NULL, filters);

		if (!dialogFilePath) {
			errorTimeOut = 4;
			return;
		}

		const std::string filePath = dialogFilePath;
		std::free(dialogFilePath);

		funes::customDataDir = system::getDirectory(filePath);
		loadCustomData(filePath);
#else // USING_CARDINAL_NOT_RACK
		async_dialog_filebrowser(false, nullptr, funes::customDataDir.empty() ?
			nullptr : funes::customDataDir.c_str(), "Load custom data .bin file", [this](char* dialogFilePath) {
				if (dialogFilePath == nullptr) {
					errorTimeOut = 4;
					return;
				}

				const std::string filePath = dialogFilePath;
				std::free(dialogFilePath);

				funes::customDataDir = system::getDirectory(filePath);
				loadCustomData(filePath);
			});
#endif
#else
		// TODO: this needs testing!
		osdialog_filters* filters = osdialog_filters_parse(funes::CUSTOM_DATA_FILENAME_FILTERS);

		DEFER({ osdialog_filters_free(filters); });

		async_osdialog_file(OSDIALOG_OPEN, funes::customDataDir.empty() ? NULL : funes::customDataDir.c_str(), NULL, filters, [this](char* dialogFilePath) {
			if (!dialogFilePath) {
				errorTimeOut = 4;
				return;
			}
			const std::string filePath = dialogFilePath;
			std::free(dialogFilePath);

			funes::customDataDir = system::getDirectory(filePath);
			loadCustomData(filePath);
			});
#endif // METAMODULE
	}

	void setEngine(int newModelNum) {
		params[PARAM_MODEL].setValue(newModelNum);
		displayModelNum = newModelNum;
		patch.engine = newModelNum;
	}

	void toggleModulatedDisplay() {
		bDisplayModulatedModel = !bDisplayModulatedModel;
		if (!bDisplayModulatedModel) {
			this->displayModelNum = params[PARAM_MODEL].getValue();
		}
	}

	void toggleNotesModelSelection() {
		bNotesModelSelection = !bNotesModelSelection;
		if (bNotesModelSelection) {
			inputs[INPUT_ENGINE].setChannels(0);
		}
		// Try to wait for DSP to finish.
#ifndef METAMODULE
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));
#else
		// delay_ms(1);
#endif
	}

	void setFrequencyMode(int freqModeNum) {
		frequencyMode = freqModeNum;
		params[PARAM_FREQ_MODE].setValue(freqModeNum);
	}

	void setChordBank(int chordBankNum) {
		chordBank = chordBankNum;
		params[PARAM_CHORD_BANK].setValue(chordBankNum);
	}

	void setSuboscillatorMode(int suboscillatorModeNum) {
		suboscillatorMode = static_cast<funes::SuboscillatorModes>(suboscillatorModeNum);
		params[PARAM_AUX_SUBOSCILLATOR].setValue(suboscillatorModeNum);
	}

	void toggleHoldVoltagesOnTrigger() {
		bWantHoldModulations = !bWantHoldModulations;
		params[PARAM_HOLD_MODULATIONS].setValue(static_cast<float>(bWantHoldModulations));
	}
};

struct FunesWidget : SanguineModuleWidget {
	explicit FunesWidget(Funes* module) {
		setModule(module);

		moduleName = "funes";
		panelSize = sanguineThemes::SIZE_27;
		backplateColor = sanguineThemes::PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		const float baseX = 33.9955f;

		const float lightOffsetX = 5.f;

		float currentX = baseX;

		for (int light = 0; light < 8; ++light) {
			addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(currentX, 12.1653f), module,
				Funes::LIGHT_MODEL + light * 2));
			currentX += lightOffsetX;
		}

		addChild(createLight<MediumLight<GreenLight>>(millimetersToPixelsVec(89.0271, 12.1653), module, Funes::LIGHT_FACTORY_DATA));
		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(98.7319, 12.1653), module, Funes::LIGHT_CUSTOM_DATA));

		FramebufferWidget* funesFramebuffer = new FramebufferWidget();
		addChild(funesFramebuffer);

		SanguineAlphaDisplay* alphaDisplay = new SanguineAlphaDisplay(8, module, 53.122, 32.314);
		funesFramebuffer->addChild(alphaDisplay);
		alphaDisplay->fallbackString = funes::displayLabels[8];

		if (module) {
			alphaDisplay->values.displayText = &module->displayText;
		}

		addParam(createParamCentered<Rogan2SGray>(millimetersToPixelsVec(115.088, 32.314), module, Funes::PARAM_MODEL));
		addInput(createInput<BananutBlackPoly>(millimetersToPixelsVec(126.4772, 28.3256), module, Funes::INPUT_ENGINE));

		addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(2.546, 50.836), module, Funes::INPUT_FREQUENCY));
		addParam(createParam<Trimpot>(millimetersToPixelsVec(17.552, 51.836), module, Funes::PARAM_FREQUENCY_CV));
		addParam(createParam<Sanguine3PSRed>(millimetersToPixelsVec(30.558, 46.086), module, Funes::PARAM_FREQUENCY));

		addParam(createParam<Sanguine1PSRed>(millimetersToPixelsVec(61.807, 48.100), module, Funes::PARAM_FREQUENCY_ROOT));

		addParam(createParam<Sanguine3PSGreen>(millimetersToPixelsVec(88.935, 46.086), module, Funes::PARAM_HARMONICS));
		addParam(createParam<Trimpot>(millimetersToPixelsVec(113.456, 51.825), module, Funes::PARAM_HARMONICS_CV));
		addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(126.477, 50.836), module, Funes::INPUT_HARMONICS));

		addInput(createInput<BananutPurple>(millimetersToPixelsVec(16.552, 71.066), module, Funes::INPUT_LGP_COLOR));
		addParam(createParam<Trimpot>(millimetersToPixelsVec(36.308, 72.066), module, Funes::PARAM_LPG_COLOR_CV));
		addParam(createParam<Sanguine1PSBlue>(millimetersToPixelsVec(49.243, 68.376), module, Funes::PARAM_LPG_COLOR));

		addParam(createParam<Sanguine1PSBlue>(millimetersToPixelsVec(74.371, 68.376), module, Funes::PARAM_LPG_DECAY));
		addParam(createParam<Trimpot>(millimetersToPixelsVec(94.685, 72.054), module, Funes::PARAM_LPG_DECAY_CV));
		addInput(createInput<BananutPurple>(millimetersToPixelsVec(112.456, 71.066), module, Funes::INPUT_LPG_DECAY));

		addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(2.546, 91.296), module, Funes::INPUT_TIMBRE));
		addParam(createParam<Trimpot>(millimetersToPixelsVec(17.552, 92.296), module, Funes::PARAM_TIMBRE_CV));
		addParam(createParam<Sanguine1PSOrange>(millimetersToPixelsVec(32.618, 88.606), module, Funes::PARAM_TIMBRE));

		addParam(createParam<Sanguine1PSRed>(millimetersToPixelsVec(61.807, 88.606), module, Funes::PARAM_FREQ_MODE));

		addParam(createParam<Sanguine1PSPurple>(millimetersToPixelsVec(91.329, 88.606), module, Funes::PARAM_MORPH));
		addParam(createParam<Trimpot>(millimetersToPixelsVec(113.456, 92.296), module, Funes::PARAM_MORPH_CV));
		addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(126.477, 91.296), module, Funes::INPUT_MORPH));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.737, 116.972), module, Funes::INPUT_TRIGGER));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(21.213, 116.972), module, Funes::INPUT_LEVEL));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(34.69, 116.972), module, Funes::INPUT_NOTE));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(48.603, 116.972), module,
			Funes::PARAM_HOLD_MODULATIONS, Funes::LIGHT_HOLD_MODULATIONS));

		addChild(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(millimetersToPixelsVec(89.435, 116.972), module,
			Funes::PARAM_AUX_SUBOSCILLATOR, Funes::LIGHT_AUX_SUBOSCILLATOR));

		addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(98.669, 116.972), module,
			Funes::INPUT_AUX_CROSSFADE));

		addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(119.534, 112.576), module, Funes::PARAM_AUX_CROSSFADE));

		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(110.177, 116.972), module, Funes::OUTPUT_OUT));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(128.88, 116.972), module, Funes::OUTPUT_AUX));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(116.456, 65.606), module,
			Funes::PARAM_CHORD_BANK, Funes::LIGHT_CHORD_BANK));

#ifndef METAMODULE
		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 58.733, 113.895);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 71.734, 120.828);
		addChild(mutantsLogo);
#endif
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Funes* module = dynamic_cast<Funes*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Synthesis model", "", [=](Menu* menu) {
			menu->addChild(createSubmenuItem("New", "", [=](Menu* menu) {
				for (int i = 0; i < 8; ++i) {
					menu->addChild(createCheckMenuItem(funes::modelLabels[i], "",
						[=]() { return module->patch.engine == i; },
						[=]() {	module->setEngine(i); }
					));
				}
				}));

			menu->addChild(createSubmenuItem("Pitched", "", [=](Menu* menu) {
				for (int i = 8; i < 16; ++i) {
					menu->addChild(createCheckMenuItem(funes::modelLabels[i], "",
						[=]() { return module->patch.engine == i; },
						[=]() {	module->setEngine(i); }
					));
				}
				}));

			menu->addChild(createSubmenuItem("Noise/percussive", "", [=](Menu* menu) {
				for (int i = 16; i < 24; ++i) {
					menu->addChild(createCheckMenuItem(funes::modelLabels[i], "",
						[=]() { return module->patch.engine == i; },
						[=]() { module->setEngine(i); }
					));
				}
				}));
			}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Frequency mode", funes::frequencyModeLabels,
			[=]() {return module->frequencyMode; },
			[=](int i) {module->setFrequencyMode(i); }
		));

		menu->addChild(createIndexSubmenuItem("Chord bank", funes::chordBankLabels,
			[=]() {return module->chordBank; },
			[=](int i) {module->setChordBank(i); }
		));

		menu->addChild(createCheckMenuItem("Hold modulation voltages on trigger", "",
			[=]() {return module->bWantHoldModulations; },
			[=]() {module->toggleHoldVoltagesOnTrigger(); }
		));

		menu->addChild(createIndexSubmenuItem("Aux sub-oscillator", funes::suboscillatorLabels,
			[=]() {return module->suboscillatorMode; },
			[=](int i) {module->setSuboscillatorMode(i); }
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Options", "",
			[=](Menu* menu) {
				menu->addChild(createCheckMenuItem("Display follows modulated Model", "",
					[=]() {return module->bDisplayModulatedModel; },
					[=]() {module->toggleModulatedDisplay(); }));

				std::vector<std::string> availableChannels;
				for (int i = 0; i < module->channelCount; ++i) {
					availableChannels.push_back(channelNumbers[i]);
				}
				menu->addChild(createIndexSubmenuItem("Display channel", availableChannels,
					[=]() {return module->displayChannel; },
					[=](int i) {module->displayChannel = i; }
				));

				menu->addChild(new MenuSeparator);

				menu->addChild(createCheckMenuItem("C0 model modulation (monophonic)", "",
					[=]() {return module->bNotesModelSelection; },
					[=]() {module->toggleNotesModelSelection(); }));

				menu->addChild(new MenuSeparator);

				menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", "", &module->bWantLowCpu));
			}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Custom data", "",
			[=](Menu* menu) {

				int engineNum = module->patch.engine;
				if (engineNum == 2 || engineNum == 3 || engineNum == 4 || engineNum == 5 || engineNum == 13) {
					menu->addChild(createMenuItem("Load...", "", [=]() {
						module->showCustomDataLoadDialog();
						}));

					menu->addChild(createMenuItem("Reset to factory default", "", [=]() {
						module->resetCustomData();
						}));
				} else {
					menu->addChild(createMenuLabel("6 OP-FMx, WAVETRRN & WAVETABL only"));
				}

				menu->addChild(new MenuSeparator);

				menu->addChild(createMenuItem("Open editors in web browser...", "", [=]() {
					system::openBrowser("https://bloodbat.github.io/Funes-Editors/");
					}));
			}
		));
	}
};

Model* modelFunes = createModel<Funes, FunesWidget>("Funes");