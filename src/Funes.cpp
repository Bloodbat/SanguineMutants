#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "plaits/dsp/voice.h"
#include "plaits/user_data.h"
#include "sanguinechannels.hpp"

#include "osdialog.h"
#include <thread>

#include <fstream>

#include "Funes.hpp"

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

	plaits::Voice voice[PORT_MAX_CHANNELS];
	plaits::Patch patch = {};
	plaits::UserData user_data;
	char shared_buffer[PORT_MAX_CHANNELS][16384] = {};

	float triPhase = 0.f;
	float lastLPGColor = 0.5f;
	float lastLPGDecay = 0.5f;
	float lastModelVoltage = 0.f;

	static const int kTextUpdateFrequency = 16;

	static const int kMaxUserDataSize = 4096;

	int frequencyMode = 10;
	int lastFrequencyMode = 10;
	int displayModelNum = 0;

	int displayChannel = 0;

	int channelCount = 0;
	int errorTimeOut = 0;

	uint32_t displayTimeout = 0;
	stmlib::HysteresisQuantizer2 octaveQuantizer;

	dsp::SampleRateConverter<PORT_MAX_CHANNELS * 2> outputSrc;
	dsp::DoubleRingBuffer<dsp::Frame<PORT_MAX_CHANNELS * 2>, 256> outputBuffer;

	bool bLowCpu = false;

	bool bDisplayModulatedModel = true;

	bool bLoading = false;

	bool bNotesModelSelection = false;

	FunesLEDModes ledsMode = LEDNormal;

	std::string displayText = "";

	dsp::ClockDivider clockDivider;

	FunesCustomDataStates customDataStates[plaits::kMaxEngines] = {};

	Funes() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configSwitch(PARAM_MODEL, 0.f, 23.f, 8.f, "Model", funesModelLabels);

		configParam(PARAM_FREQUENCY, -4.f, 4.f, 0.f, "Frequency", " semitones", 0.f, 12.f);
		configParam(PARAM_FREQUENCY_ROOT, -4.f, 4.f, 0.f, "Frequency Root", " semitones", 0.f, 12.f);
		configParam(PARAM_HARMONICS, 0.f, 1.f, 0.5f, "Harmonics", "%", 0.f, 100.f);
		configParam(PARAM_TIMBRE, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);
		configParam(PARAM_LPG_COLOR, 0.f, 1.f, 0.5f, "Lowpass gate response", "%", 0.f, 100.f);
		configParam(PARAM_MORPH, 0.f, 1.f, 0.5f, "Morph", "%", 0.f, 100.f);
		configParam(PARAM_LPG_DECAY, 0.f, 1.f, 0.5f, "Lowpass gate decay", "%", 0.f, 100.f);
		configParam(PARAM_TIMBRE_CV, -1.f, 1.f, 0.f, "Timbre CV");
		configParam(PARAM_FREQUENCY_CV, -1.f, 1.f, 0.f, "Frequency CV");
		configParam(PARAM_MORPH_CV, -1.f, 1.f, 0.f, "Morph CV");
		configSwitch(PARAM_FREQ_MODE, 0.f, 10.f, 10.f, "Frequency mode", funesFrequencyModes);
		configParam(PARAM_HARMONICS_CV, -1.f, 1.f, 0.f, "Harmonics CV");
		configParam(PARAM_LPG_COLOR_CV, -1.f, 1.f, 0.f, "Lowpass gate response CV");
		configParam(PARAM_LPG_DECAY_CV, -1.f, 1.f, 0.f, "Lowpass gate decay CV");

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
			stmlib::BufferAllocator allocator(shared_buffer[channel], sizeof(shared_buffer[channel]));
			voice[channel].Init(&allocator, &user_data);
		}

		octaveQuantizer.Init(9, 0.01f, false);

		clockDivider.setDivision(kTextUpdateFrequency);

		resetCustomDataStates();

		onReset();
	}

	void process(const ProcessArgs& args) override {
		channelCount = std::max(std::max(inputs[INPUT_NOTE].getChannels(), inputs[INPUT_TRIGGER].getChannels()), 1);

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
						displayModelNum = patch.engine;
					}
				}
			} else if (params[PARAM_MODEL].getValue() != patch.engine) {
				ledsMode = LEDNormal;
				displayTimeout = 0;
				lastLPGColor = params[PARAM_LPG_COLOR].getValue();
				lastLPGDecay = params[PARAM_LPG_DECAY].getValue();
				lastFrequencyMode = params[PARAM_FREQ_MODE].getValue();
				patch.engine = params[PARAM_MODEL].getValue();
				displayModelNum = patch.engine;
			}

			// Calculate pitch for low cpu mode, if needed.
			float pitch = params[PARAM_FREQUENCY].getValue();
			if (bLowCpu) {
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
				ledsMode = LEDLPG;
				--displayTimeout;
			}

			bool activeLights[PORT_MAX_CHANNELS] = {};

			bool pulse = false;

			// Render output buffer for each voice.
			dsp::Frame<PORT_MAX_CHANNELS * 2> outputFrames[blockSize];
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
				plaits::Voice::Frame output[blockSize];
				voice[channel].Render(patch, modulations, output, blockSize);

				// Convert output to frames
				for (int blockNum = 0; blockNum < blockSize; ++blockNum) {
					outputFrames[blockNum].samples[channel * 2 + 0] = output[blockNum].out / 32768.f;
					outputFrames[blockNum].samples[channel * 2 + 1] = output[blockNum].aux / 32768.f;
				}

				if (displayChannel == channel) {
					displayModelNum = voice[channel].active_engine();
				}

				if (ledsMode == LEDNormal) {
					// Model lights
					// Get the active engines for current channel.
					int currentLight;
					int clampedEngine;
					int activeEngine = voice[channel].active_engine();
					clampedEngine = (activeEngine % 8) * 2;

					bool noiseModels = activeEngine & 0x10;
					bool pitchedModels = activeEngine & 0x08;

					if (noiseModels) {
						currentLight = clampedEngine + 1;
						activeLights[currentLight] = true;
					} else if (pitchedModels) {
						currentLight = clampedEngine;
						activeLights[currentLight] = true;
					} else
					{
						currentLight = clampedEngine;
						activeLights[currentLight] = true;
						currentLight = clampedEngine + 1;
						activeLights[currentLight] = true;
					}

					// Pulse the light if at least one voice is using a different engine.
					pulse = activeEngine != patch.engine;
				}
			}

			// Update model text, custom data lights and frequency mode every 16 samples only.
			if (clockDivider.process()) {
				if (displayChannel >= channelCount) {
					displayChannel = channelCount - 1;
				}

				if (bDisplayModulatedModel) {
					displayText = funesDisplayLabels[displayModelNum];
				}

				frequencyMode = params[PARAM_FREQ_MODE].getValue();

				if (frequencyMode != lastFrequencyMode) {
					ledsMode = LEDOctave;
					--displayTimeout;
				}

				lights[LIGHT_FACTORY_DATA].setBrightness(customDataStates[patch.engine] == DataFactory &&
					errorTimeOut == 0 ? kSanguineButtonLightValue : 0.f);
				lights[LIGHT_CUSTOM_DATA + 0].setBrightness(customDataStates[patch.engine] == DataCustom &&
					errorTimeOut == 0 ? kSanguineButtonLightValue : 0.f);
				lights[LIGHT_CUSTOM_DATA + 1].setBrightness(customDataStates[patch.engine] == DataCustom ||
					errorTimeOut > 0 ? kSanguineButtonLightValue : 0.f);

				if (errorTimeOut != 0) {
					--errorTimeOut;

					if (errorTimeOut <= 0) {
						errorTimeOut = 0;
					}
				}
			}

			// Convert output.
			if (!bLowCpu) {
				outputSrc.setRates(48000, static_cast<int>(args.sampleRate));
				int inLen = blockSize;
				int outLen = outputBuffer.capacity();
				outputSrc.setChannels(channelCount * 2);
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			} else {
				int len = std::min(static_cast<int>(outputBuffer.capacity()), blockSize);
				std::memcpy(outputBuffer.endData(), outputFrames, len * sizeof(outputFrames[0]));
				outputBuffer.endIncr(len);
			}

			// Pulse light at 2 Hz.
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
				for (int led = 0; led < 8; ++led) {
					int currentLight = led * 2;
					float brightnessRed = static_cast<float>(activeLights[currentLight + 1]);
					float brightnessGreen = static_cast<float>(activeLights[currentLight]);

					if (pulse && clampedEngine == led) {
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
			case LEDOctave: {
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

		if (ledsMode != LEDNormal) {
			++displayTimeout;
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
			dsp::Frame<PORT_MAX_CHANNELS * 2> outputFrame = outputBuffer.shift();
			for (int channel = 0; channel < channelCount; ++channel) {
				// Inverting op-amp on outputs
				outputs[OUTPUT_OUT].setVoltage(-outputFrame.samples[channel * 2 + 0] * 5.f, channel);
				outputs[OUTPUT_AUX].setVoltage(-outputFrame.samples[channel * 2 + 1] * 5.f, channel);
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

		json_object_set_new(rootJ, "lowCpu", json_boolean(bLowCpu));
		json_object_set_new(rootJ, "displayModulatedModel", json_boolean(bDisplayModulatedModel));
		json_object_set_new(rootJ, "frequencyMode", json_integer(frequencyMode));
		json_object_set_new(rootJ, "notesModelSelection", json_boolean(bNotesModelSelection));
		json_object_set_new(rootJ, "displayChannel", json_integer(displayChannel));

		const uint8_t* userDataBuffer = user_data.getBuffer();
		if (userDataBuffer != nullptr) {
			std::string userDataString = rack::string::toBase64(userDataBuffer, plaits::UserData::MAX_USER_DATA_SIZE);
			json_object_set_new(rootJ, "userData", json_string(userDataString.c_str()));
		}

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		SanguineModule::dataFromJson(rootJ);

		json_t* lowCpuJ = json_object_get(rootJ, "lowCpu");
		if (lowCpuJ) {
			bLowCpu = json_boolean_value(lowCpuJ);
		}

		json_t* displayModulatedModelJ = json_object_get(rootJ, "displayModulatedModel");
		if (displayModulatedModelJ) {
			bDisplayModulatedModel = json_boolean_value(displayModulatedModelJ);
		}

		json_t* notesModelSelectionJ = json_object_get(rootJ, "notesModelSelection");
		if (notesModelSelectionJ) {
			bNotesModelSelection = json_boolean_value(notesModelSelectionJ);
		}

		json_t* frequencyModeJ = json_object_get(rootJ, "frequencyMode");
		if (frequencyModeJ) {
			setFrequencyMode(json_integer_value(frequencyModeJ));
		}

		json_t* displayChannelJ = json_object_get(rootJ, "displayChannel");
		if (displayChannelJ) {
			displayChannel = json_integer_value(displayChannelJ);
		}

		json_t* userDataJ = json_object_get(rootJ, "userData");
		if (userDataJ) {
			std::string userDataString = json_string_value(userDataJ);
			const std::vector<uint8_t> userDataVector = rack::string::fromBase64(userDataString);
			if (userDataVector.size() > 0) {
				const uint8_t* userDataBuffer = &userDataVector[0];
				user_data.setBuffer(userDataBuffer);
				if (userDataBuffer[kMaxUserDataSize - 2] == 'U') {
					resetCustomDataStates();
					customDataStates[userDataBuffer[kMaxUserDataSize - 1] - ' '] = DataCustom;
				}
			}
		}
	}

	void resetCustomData() {
		bool success = user_data.Save(nullptr, patch.engine);
		if (success) {
			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				voice[channel].ReloadUserData();
			}
			resetCustomDataStates();
		}
	}

	void loadCustomData(const std::string& filePath) {
		bLoading = true;
		DEFER({ bLoading = false; });
		// HACK: Sleep 100us so DSP thread is likely to finish processing before we resize the vector.
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));

		std::string fileExtension = string::lowercase(system::getExtension(filePath));

		if (fileExtension == ".bin") {
			std::ifstream input(filePath, std::ios::binary);
			std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(input), {});
			uint8_t* dataBuffer = buffer.data();
			bool success = user_data.Save(dataBuffer, patch.engine);
			if (success) {
				for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
					voice[channel].ReloadUserData();
				}
				// Only 1 engine can use custom data at a time.
				resetCustomDataStates();
				customDataStates[patch.engine] = DataCustom;
			} else {
				errorTimeOut = 4;
			}
		}
	}

	void showCustomDataLoadDialog() {
#ifndef USING_CARDINAL_NOT_RACK
		osdialog_filters* filters = osdialog_filters_parse(WAVE_FILTERS);
		char* dialogFilePath = osdialog_file(OSDIALOG_OPEN, waveDir.empty() ? NULL : waveDir.c_str(), NULL, filters);

		if (!dialogFilePath) {
			errorTimeOut = 4;
			return;
		}

		std::string filePath = dialogFilePath;
		std::free(dialogFilePath);

		waveDir = system::getDirectory(filePath);
		loadCustomData(filePath);
#else
		async_dialog_filebrowser(false, nullptr, waveDir.empty() ? nullptr : waveDir.c_str(), "Load custom data .bin file", [this](char* dialogFilePath) {

			if (dialogFilePath == nullptr) {
				errorTimeOut = 4;
				return;
			}

			const std::string filePath = dialogFilePath;
			std::free(dialogFilePath);

			waveDir = system::getDirectory(filePath);
			loadCustomData(filePath);
			});
#endif
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
		std::this_thread::sleep_for(std::chrono::duration<double>(100e-6));
	}

	void setFrequencyMode(int freqModeNum) {
		frequencyMode = freqModeNum;
		params[PARAM_FREQ_MODE].setValue(freqModeNum);
	}

	void resetCustomDataStates() {
		customDataStates[2] = DataFactory;
		customDataStates[3] = DataFactory;
		customDataStates[4] = DataFactory;
		customDataStates[5] = DataFactory;
		customDataStates[13] = DataFactory;
	}
};

struct FunesWidget : SanguineModuleWidget {
	explicit FunesWidget(Funes* module) {
		setModule(module);

		moduleName = "funes";
		panelSize = SIZE_27;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		const float baseX = 33.9955f;

		const float lightOffsetX = 5.f;

		float currentX = baseX;

		for (int light = 0; light < 8; ++light) {
			addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(currentX, 12.1653f), module, Funes::LIGHT_MODEL + light * 2));
			currentX += lightOffsetX;
		}

		addChild(createLight<MediumLight<GreenLight>>(millimetersToPixelsVec(89.0271, 12.1653), module, Funes::LIGHT_FACTORY_DATA));
		addChild(createLight<MediumLight<GreenRedLight>>(millimetersToPixelsVec(98.7319, 12.1653), module, Funes::LIGHT_CUSTOM_DATA));

		FramebufferWidget* funesFrambuffer = new FramebufferWidget();
		addChild(funesFrambuffer);

		SanguineAlphaDisplay* alphaDisplay = new SanguineAlphaDisplay(8, module, 53.122, 32.314);
		funesFrambuffer->addChild(alphaDisplay);
		alphaDisplay->fallbackString = funesDisplayLabels[8];

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

		SanguineBloodLogoLight* bloodLogo = new SanguineBloodLogoLight(module, 58.733, 113.895);
		addChild(bloodLogo);

		SanguineMutantsLogoLight* mutantsLogo = new SanguineMutantsLogoLight(module, 71.734, 120.828);
		addChild(mutantsLogo);
	}

	void appendContextMenu(Menu* menu) override {
		SanguineModuleWidget::appendContextMenu(menu);

		Funes* module = dynamic_cast<Funes*>(this->module);

		menu->addChild(new MenuSeparator);

		menu->addChild(createSubmenuItem("Synthesis model", "",
			[=](Menu* menu) {
				menu->addChild(createSubmenuItem("New", "", [=](Menu* menu) {
					for (int i = 0; i < 8; ++i) {
						menu->addChild(createCheckMenuItem(funesModelLabels[i], "",
							[=]() { return module->patch.engine == i; },
							[=]() {	module->setEngine(i); }
						));
					}
					}));

				menu->addChild(createSubmenuItem("Pitched", "", [=](Menu* menu) {
					for (int i = 8; i < 16; ++i) {
						menu->addChild(createCheckMenuItem(funesModelLabels[i], "",
							[=]() { return module->patch.engine == i; },
							[=]() {	module->setEngine(i); }
						));
					}
					}));

				menu->addChild(createSubmenuItem("Noise/percussive", "", [=](Menu* menu) {
					for (int i = 16; i < 24; ++i) {
						menu->addChild(createCheckMenuItem(funesModelLabels[i], "",
							[=]() { return module->patch.engine == i; },
							[=]() { module->setEngine(i); }
						));
					}
					}));
			}
		));

		menu->addChild(new MenuSeparator);

		menu->addChild(createIndexSubmenuItem("Frequency mode", funesFrequencyModes,
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

				menu->addChild(createBoolPtrMenuItem("Low CPU (disable resampling)", "", &module->bLowCpu));
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