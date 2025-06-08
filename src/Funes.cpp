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
		LIGHTS_COUNT
	};

	plaits::Voice voices[PORT_MAX_CHANNELS];
	plaits::Patch patch = {};
	plaits::UserData userData;
	char sharedBuffers[PORT_MAX_CHANNELS][16384] = {};

	float triPhase = 0.f;
	float lastLPGColor = 0.5f;
	float lastLPGDecay = 0.5f;
	float lastModelVoltage = 0.f;

	static const int kLightsUpdateFrequency = 16;

	static const int kMaxUserDataSize = 4096;

	int frequencyMode = 10;
	int lastFrequencyMode = 10;
	int displayModelNum = 0;

	int displayChannel = 0;

	int channelCount = 0;
	int errorTimeOut = 0;

	uint32_t displayTimeout = 0;
	stmlib::HysteresisQuantizer2 octaveQuantizer;

	dsp::SampleRateConverter<PORT_MAX_CHANNELS * 2> srcOutputs;
	dsp::DoubleRingBuffer<dsp::Frame<PORT_MAX_CHANNELS * 2>, 256> drbOutputBuffers;

	bool bWantLowCpu = false;

	bool bDisplayModulatedModel = true;

	bool bIsLoading = false;

	bool bNotesModelSelection = false;

	funes::LEDModes ledsMode = funes::LEDNormal;

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
		configSwitch(PARAM_FREQ_MODE, 0.f, 10.f, 10.f, "Frequency mode", funes::frequencyModes);
		configParam(PARAM_HARMONICS_CV, -1.f, 1.f, 0.f, "Harmonics CV", "%", 0.f, 100.f);
		configParam(PARAM_LPG_COLOR_CV, -1.f, 1.f, 0.f, "Lowpass gate response CV", "%", 0.f, 100.f);
		configParam(PARAM_LPG_DECAY_CV, -1.f, 1.f, 0.f, "Lowpass gate decay CV", "%", 0.f, 100.f);

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

		configOutput(OUTPUT_OUT, "Main");
		configOutput(OUTPUT_AUX, "Auxiliary");

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			stmlib::BufferAllocator allocator(sharedBuffers[channel], sizeof(sharedBuffers[channel]));
			voices[channel].Init(&allocator, &userData);
		}

		octaveQuantizer.Init(9, 0.01f, false);

		lightsDivider.setDivision(kLightsUpdateFrequency);

		resetCustomDataStates();

		onReset();
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(std::max(inputs[INPUT_NOTE].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

		if (drbOutputBuffers.empty()) {
			const int kBlockSize = 12;

			// Switch models
			if (bNotesModelSelection && inputs[INPUT_ENGINE].isConnected()) {
				float currentModelVoltage = inputs[INPUT_ENGINE].getVoltage();
				if (currentModelVoltage != lastModelVoltage) {
					lastModelVoltage = currentModelVoltage;
					int updatedModel = std::round((lastModelVoltage + 4.f) * 12.f);
					if (updatedModel >= 0 && updatedModel < 24) {
						patch.engine = updatedModel;
						displayModelNum = patch.engine;
					}
				}
			} else if (params[PARAM_MODEL].getValue() != patch.engine) {
				ledsMode = funes::LEDNormal;
				displayTimeout = 0;
				lastLPGColor = params[PARAM_LPG_COLOR].getValue();
				lastLPGDecay = params[PARAM_LPG_DECAY].getValue();
				lastFrequencyMode = params[PARAM_FREQ_MODE].getValue();
				patch.engine = params[PARAM_MODEL].getValue();
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
			if (frequencyMode == 0) {
				patch.note = -48.37f + pitch * 15.f;
			} else if (frequencyMode == 9) {
				float fineTune = params[PARAM_FREQUENCY_ROOT].getValue() / 4.f;
				patch.note = 53.f + fineTune * 14.f + 12.f * static_cast<float>(octaveQuantizer.Process(0.5f * pitch / 4.f + 0.5f) - 4.f);
			} else if (frequencyMode == 10) {
				patch.note = 60.f + pitch * 12.f;
			} else {
				patch.note = static_cast<float>(frequencyMode) * 12.f + pitch * 7.f / 4.f;
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

			if (params[PARAM_LPG_COLOR].getValue() != lastLPGColor || params[PARAM_LPG_DECAY].getValue() != lastLPGDecay) {
				ledsMode = funes::LEDLPG;
				--displayTimeout;
			}

			bool activeLights[PORT_MAX_CHANNELS] = {};

			bool bPulseLight = false;

			// Render output buffer for each voice.
			dsp::Frame<PORT_MAX_CHANNELS * 2> outputFrames[kBlockSize];
			for (int channel = 0; channel < channelCount; ++channel) {
				// Construct modulations.
				plaits::Modulations modulations;
				if (!bNotesModelSelection) {
					modulations.engine = inputs[INPUT_ENGINE].getPolyVoltage(channel) / 5.f;
				}

				modulations.note = inputs[INPUT_NOTE].getVoltage(channel) * 12.f;
				modulations.frequency_patched = inputs[INPUT_FREQUENCY].isConnected();
				modulations.frequency = inputs[INPUT_FREQUENCY].getPolyVoltage(channel) * 6.f;
				modulations.harmonics = (inputs[INPUT_HARMONICS].getPolyVoltage(channel) / 5.f) *
					params[PARAM_HARMONICS_CV].getValue();
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
				plaits::Voice::Frame output[kBlockSize];
				voices[channel].Render(patch, modulations, output, kBlockSize);

				// Convert output to frames
				for (int blockNum = 0; blockNum < kBlockSize; ++blockNum) {
					outputFrames[blockNum].samples[channel * 2 + 0] = output[blockNum].out / 32768.f;
					outputFrames[blockNum].samples[channel * 2 + 1] = output[blockNum].aux / 32768.f;
				}

				if (displayChannel == channel) {
					displayModelNum = voices[channel].active_engine();
				}

				if (ledsMode == funes::LEDNormal) {
					// Model lights
					// Get the active engines for current channel.
					int currentLight;
					int activeEngine = voices[channel].active_engine();
					int clampedEngine = (activeEngine % 8) * 2;

					bool bIsNoiseModel = activeEngine & 0x10;
					bool bIsPitchedModel = activeEngine & 0x08;

					if (bIsNoiseModel) {
						currentLight = clampedEngine + 1;
						activeLights[currentLight] = true;
					} else if (bIsPitchedModel) {
						currentLight = clampedEngine;
						activeLights[currentLight] = true;
					} else {
						currentLight = clampedEngine;
						activeLights[currentLight] = true;
						currentLight = clampedEngine + 1;
						activeLights[currentLight] = true;
					}

					// Pulse the light if at least one voice is using a different engine.
					bPulseLight = activeEngine != patch.engine;
				}
			}

			// Update model text, custom data lights and frequency mode every 16 samples only.
			if (lightsDivider.process()) {
				if (displayChannel >= channelCount) {
					displayChannel = channelCount - 1;
				}

				if (bDisplayModulatedModel) {
					displayText = funes::displayLabels[displayModelNum];
				}

				frequencyMode = params[PARAM_FREQ_MODE].getValue();

				if (frequencyMode != lastFrequencyMode) {
					ledsMode = funes::LEDOctave;
					--displayTimeout;
				}

				lights[LIGHT_FACTORY_DATA].setBrightness(customDataStates[patch.engine] == funes::DataFactory &&
					errorTimeOut == 0 ? kSanguineButtonLightValue : 0.f);
				lights[LIGHT_CUSTOM_DATA + 0].setBrightness(customDataStates[patch.engine] == funes::DataCustom &&
					errorTimeOut == 0 ? kSanguineButtonLightValue : 0.f);
				lights[LIGHT_CUSTOM_DATA + 1].setBrightness(customDataStates[patch.engine] == funes::DataCustom ||
					errorTimeOut > 0 ? kSanguineButtonLightValue : 0.f);

				if (errorTimeOut != 0) {
					--errorTimeOut;

					if (errorTimeOut <= 0) {
						errorTimeOut = 0;
					}
				}
			}

			// Convert output.
			if (!bWantLowCpu) {
				srcOutputs.setRates(48000, static_cast<int>(args.sampleRate));
				int inLen = kBlockSize;
				int outLen = drbOutputBuffers.capacity();
				srcOutputs.setChannels(channelCount * 2);
				srcOutputs.process(outputFrames, &inLen, drbOutputBuffers.endData(), &outLen);
				drbOutputBuffers.endIncr(outLen);
			} else {
				int len = std::min(static_cast<int>(drbOutputBuffers.capacity()), kBlockSize);
				std::memcpy(drbOutputBuffers.endData(), outputFrames, len * sizeof(outputFrames[0]));
				drbOutputBuffers.endIncr(len);
			}

			// Pulse light at 2 Hz.
			triPhase += 2.f * args.sampleTime * kBlockSize;
			if (triPhase >= 1.f) {
				triPhase -= 1.f;
			}
			float tri = (triPhase < 0.5f) ? triPhase * 2.f : (1.f - triPhase) * 2.f;

			switch (ledsMode) {
			case funes::LEDNormal: {
				// Set model lights.
				int clampedEngine = patch.engine % 8;
				for (int led = 0; led < 8; ++led) {
					int currentLight = led * 2;
					float brightnessRed = static_cast<float>(activeLights[currentLight + 1]);
					float brightnessGreen = static_cast<float>(activeLights[currentLight]);

					if (bPulseLight && clampedEngine == led) {
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
			case funes::LEDLPG: {
				for (int parameter = 0; parameter < 2; ++parameter) {
					float value;
					int startLight;
					// nextLight should be a multiple of 2: LEDs are RedGreen lights and each color is a separate "light".
					int nextLight;
					if (parameter == 0) {
						value = params[PARAM_LPG_COLOR].getValue();
						startLight = LIGHT_MODEL + 3 * 2;
						nextLight = -2;
					} else {
						value = params[PARAM_LPG_DECAY].getValue();
						startLight = LIGHT_MODEL + 4 * 2;
						nextLight = 2;
					}

					float lightValue = value > 0.f ? math::rescale(value, 0.f, 0.25f, 0.f, 1.f) : 0.f;
					lights[startLight + 0].setBrightness(lightValue);
					lights[startLight + 1].setBrightness(lightValue);
					startLight += nextLight;
					lightValue = value >= 0.251f ? math::rescale(value, 0.251f, 0.50f, 0.f, 1.f) : 0.f;
					lights[startLight + 0].setBrightness(lightValue);
					lights[startLight + 1].setBrightness(lightValue);
					startLight += nextLight;
					lightValue = value >= 0.501f ? math::rescale(value, 0.501f, 0.75f, 0.f, 1.f) : 0.f;
					lights[startLight + 0].setBrightness(lightValue);
					lights[startLight + 1].setBrightness(lightValue);
					startLight += nextLight;
					lightValue = value >= 0.751f ? math::rescale(value, 0.751f, 1.f, 0.f, 1.f) : 0.f;
					lights[startLight + 0].setBrightness(lightValue);
					lights[startLight + 1].setBrightness(lightValue);
				}
				break;
			}
			case funes::LEDOctave: {
				int octave = params[PARAM_FREQ_MODE].getValue();
				for (int led = 0; led < 8; ++led) {
					float ledValue = 0.f;
					int triangle = tri;

					if (octave == 0) {
						ledValue = led == (triangle >> 1) ? 0.f : 1.f;
					} else if (octave == 10) {
						ledValue = 1.f;
					} else if (octave == 9) {
						ledValue = (led & 1) == ((triangle >> 3) & 1) ? 0.f : 1.f;
					} else {
						ledValue = (octave - 1) == led ? 1.f : 0.f;
					}
					lights[(LIGHT_MODEL + 7 * 2) - led * 2 + 0].setBrightness(ledValue);
					lights[(LIGHT_MODEL + 7 * 2) - led * 2 + 1].setBrightness(ledValue);
				}
				break;
			}
			}
		}

		if (ledsMode != funes::LEDNormal) {
			++displayTimeout;
		}
		if (displayTimeout > args.sampleRate * 3) {
			ledsMode = funes::LEDNormal;
			displayTimeout = 0;
			lastLPGColor = params[PARAM_LPG_COLOR].getValue();
			lastLPGDecay = params[PARAM_LPG_DECAY].getValue();
			lastFrequencyMode = params[PARAM_FREQ_MODE].getValue();
		}

		// Set output
		if (!drbOutputBuffers.empty()) {
			dsp::Frame<PORT_MAX_CHANNELS * 2> outputFrames = drbOutputBuffers.shift();
			for (int channel = 0; channel < channelCount; ++channel) {
				// Inverting op-amp on outputs
				outputs[OUTPUT_OUT].setVoltage(-outputFrames.samples[channel * 2 + 0] * 5.f, channel);
				outputs[OUTPUT_AUX].setVoltage(-outputFrames.samples[channel * 2 + 1] * 5.f, channel);
			}
		}
		outputs[OUTPUT_OUT].setChannels(channelCount);
		outputs[OUTPUT_AUX].setChannels(channelCount);
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
				if (userDataBuffer[kMaxUserDataSize - 2] == 'U') {
					for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
						voices[channel].ReloadUserData();
					}

					resetCustomDataStates();
					customDataStates[userDataBuffer[kMaxUserDataSize - 1] - ' '] = funes::DataCustom;
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
			uint8_t* dataBuffer = buffer.data();
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

	void resetCustomDataStates() {
		customDataStates[2] = funes::DataFactory;
		customDataStates[3] = funes::DataFactory;
		customDataStates[4] = funes::DataFactory;
		customDataStates[5] = funes::DataFactory;
		customDataStates[13] = funes::DataFactory;
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
		addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(126.4772, 28.3256), module, Funes::INPUT_ENGINE));

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
		addParam(createParam<Sanguine1PSRed>(millimetersToPixelsVec(32.618, 88.606), module, Funes::PARAM_TIMBRE));

		addParam(createParam<Sanguine1PSPurple>(millimetersToPixelsVec(61.807, 88.606), module, Funes::PARAM_FREQ_MODE));

		addParam(createParam<Sanguine1PSGreen>(millimetersToPixelsVec(91.329, 88.606), module, Funes::PARAM_MORPH));
		addParam(createParam<Trimpot>(millimetersToPixelsVec(113.456, 92.296), module, Funes::PARAM_MORPH_CV));
		addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(126.477, 91.296), module, Funes::INPUT_MORPH));

		addInput(createInput<BananutGreenPoly>(millimetersToPixelsVec(3.737, 112.984), module, Funes::INPUT_TRIGGER));
		addInput(createInput<BananutGreenPoly>(millimetersToPixelsVec(17.214, 112.984), module, Funes::INPUT_LEVEL));
		addInput(createInput<BananutGreenPoly>(millimetersToPixelsVec(30.690, 112.984), module, Funes::INPUT_NOTE));

		addOutput(createOutput<BananutRedPoly>(millimetersToPixelsVec(111.028, 112.984), module, Funes::OUTPUT_OUT));
		addOutput(createOutput<BananutRedPoly>(millimetersToPixelsVec(124.880, 112.984), module, Funes::OUTPUT_AUX));

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

		menu->addChild(createIndexSubmenuItem("Frequency mode", funes::frequencyModes,
			[=]() {return module->frequencyMode; },
			[=](int i) {module->setFrequencyMode(i); }
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