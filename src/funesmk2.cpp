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
#include "funesmk2.hpp"

struct FunesMk2 : SanguineModule {
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
        INPUT_MODEL,
        INPUT_TIMBRE,
        INPUT_FREQUENCY,
        INPUT_MORPH,
        INPUT_HARMONICS,
        INPUT_TRIGGER,
        INPUT_LEVEL,
        INPUT_NOTE,
        INPUT_LPG_COLOR,
        INPUT_LPG_DECAY,
        INPUT_AUX_CROSSFADE,
        INPUT_CHORD_BANK,
        INPUT_AUX_SUBOSCILLATOR,
        INPUT_HOLD_MODULATIONS,
        INPUTS_COUNT
    };

    enum OutputIds {
        OUTPUT_OUT,
        OUTPUT_AUX,
        OUTPUTS_COUNT
    };

    enum LightIds {
        ENUMS(LIGHT_CHANNEL, PORT_MAX_CHANNELS * 3),
        LIGHT_FACTORY_DATA,
        ENUMS(LIGHT_CUSTOM_DATA, 2),
        ENUMS(LIGHT_CHORD_BANK, 2),
        ENUMS(LIGHT_AUX_SUBOSCILLATOR, 3),
        LIGHT_HOLD_MODULATIONS,
        LIGHTS_COUNT
    };

    plaits::Voice voices[PORT_MAX_CHANNELS];
    plaits::Patch patches[PORT_MAX_CHANNELS] = {};
    plaits::Modulations modulations[PORT_MAX_CHANNELS] = {};
    // TODO: Are we still crashing??
    plaits::UserData userDatas[PORT_MAX_CHANNELS];
    // Hardware uses 16384; but new chords crash Rack on mode change if left as such.
    uint8_t sharedBuffers[PORT_MAX_CHANNELS][50176] = {};

    static const int kLightsFrequency = 16;

    int jitteredLightsFrequency;

    int frequencyMode = funes::FM_FULL;

    int displayChannel = 0;

    int channelCount = 0;
    int errorTimeOut = 0;

    int selectedChordBank = 0;

    int knobModel = 8;

    uint8_t chordBanks[PORT_MAX_CHANNELS] = {};

    funes::SuboscillatorModes suboscillatorModes[PORT_MAX_CHANNELS] = {};

    stmlib::HysteresisQuantizer2 octaveQuantizers[PORT_MAX_CHANNELS];

    dsp::SampleRateConverter<PORT_MAX_CHANNELS * 2> srcOutputs;
    dsp::DoubleRingBuffer<dsp::Frame<PORT_MAX_CHANNELS * 2>, 256> drbOutputBuffers;

    bool bWantLowCpu = false;

    bool bDisplayModulatedModel = true;

    bool bIsLoading = false;

    bool bNotesModelSelection = false;

    bool bWantHoldModulations = false;

    bool bHaveInputModel = false;
    bool bHaveInputTimbre = false;
    bool bHaveInputFrequency = false;
    bool bHaveInputMorph = false;
    bool bHaveInputTrigger = false;
    bool bHaveInputLevel = false;
    bool bHaveInputLpgColor = false;
    bool bHaveInputLpgDecay = false;
    bool bHaveInputChordBank = false;
    bool bHaveInputAuxSuboscillator = false;
    bool bHaveInputHoldModulations = false;

    bool holdModulations[PORT_MAX_CHANNELS] = {};

    std::string displayText = "";

    dsp::ClockDivider lightsDivider;

    int customDataStates[PORT_MAX_CHANNELS] = {};

    float selectedSubOscillator = 0.f;

    FunesMk2() {
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

        configInput(INPUT_MODEL, "Model");
        configInput(INPUT_TIMBRE, "Timbre");
        configInput(INPUT_FREQUENCY, "FM");
        configInput(INPUT_MORPH, "Morph");
        configInput(INPUT_HARMONICS, "Harmonics");
        configInput(INPUT_TRIGGER, "Trigger");
        configInput(INPUT_LEVEL, "Level");
        configInput(INPUT_NOTE, "Pitch (1V/oct)");
        configInput(INPUT_LPG_COLOR, "Lowpass gate response");
        configInput(INPUT_LPG_DECAY, "Lowpass gate decay");

        configInput(INPUT_AUX_CROSSFADE, "Main Out → Aux crossfader");

        configOutput(OUTPUT_OUT, "Main");
        configOutput(OUTPUT_AUX, "Auxiliary");

        for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
            stmlib::BufferAllocator allocator(sharedBuffers[channel], sizeof(sharedBuffers[channel]));
            voices[channel].Init(&allocator, &userDatas[channel]);

            octaveQuantizers[channel].Init(9, 0.01f, false);
        }

        init();
    }

    void process(const ProcessArgs& args) override {
        using simd::float_4;

        channelCount = std::max(std::max(inputs[INPUT_NOTE].getChannels(),
            inputs[INPUT_TRIGGER].getChannels()), 1);

        selectedChordBank = static_cast<uint8_t>(params[PARAM_CHORD_BANK].getValue());

        bWantHoldModulations = static_cast<bool>(params[PARAM_HOLD_MODULATIONS].getValue());

        selectedSubOscillator = params[PARAM_AUX_SUBOSCILLATOR].getValue();

        float knobLpgColor = params[PARAM_LPG_COLOR].getValue();
        float attenLpgColor = params[PARAM_LPG_COLOR_CV].getValue();

        float knobLpgDecay = params[PARAM_LPG_DECAY].getValue();
        float attenLpgDecay = params[PARAM_LPG_DECAY_CV].getValue();

        float knobFrequency = params[PARAM_FREQUENCY].getValue();

        float knobHarmonics = params[PARAM_HARMONICS].getValue();
        float knobTimbre = params[PARAM_TIMBRE].getValue();
        float knobMorph = params[PARAM_MORPH].getValue();
        float knobAuxCrossfade = params[PARAM_AUX_CROSSFADE].getValue();

        float attenFm = params[PARAM_FREQUENCY_CV].getValue();
        float attenHarmonics = params[PARAM_HARMONICS_CV].getValue();
        float attenTimbre = params[PARAM_TIMBRE_CV].getValue();
        float attenMorph = params[PARAM_MORPH_CV].getValue();

        knobModel = static_cast<int>(params[PARAM_MODEL].getValue());

        int currentChannel;

        int individualChannel;
        bool bIsSubOscillatorOctave;
        float_4 inputVoltages;

        // Calculate pitch for low cpu mode, if needed.
        float pitch = knobFrequency;
        if (bWantLowCpu) {
            pitch += std::log2(plaits::kSampleRate * args.sampleTime);
        }

        if (drbOutputBuffers.empty()) {
            float_4 auxSubOscillators[4];

            float_4 selectedModels[4];

            float_4 lpgColorVoltages[4];
            float_4 lpgDecayVoltages[4];

            float_4 chordBankVoltages;

            for (int channel = 0; channel < channelCount; channel += 4) {
                currentChannel = channel >> 2;

                if (!bHaveInputChordBank) {
                    chordBanks[channel] = selectedChordBank;
                    chordBanks[channel + 1] = selectedChordBank;
                    chordBanks[channel + 2] = selectedChordBank;
                    chordBanks[channel + 3] = selectedChordBank;
                } else {
                    chordBankVoltages = inputs[INPUT_CHORD_BANK].getVoltageSimd<float_4>(channel);
                    chordBankVoltages = simd::round(chordBankVoltages);
                    chordBankVoltages = simd::clamp(chordBankVoltages, 0.f, 2.f);

                    chordBanks[channel] = static_cast<uint8_t>(chordBankVoltages[0]);
                    chordBanks[channel + 1] = static_cast<uint8_t>(chordBankVoltages[1]);
                    chordBanks[channel + 2] = static_cast<uint8_t>(chordBankVoltages[2]);
                    chordBanks[channel + 3] = static_cast<uint8_t>(chordBankVoltages[3]);
                }

                if (!bHaveInputAuxSuboscillator) {
                    auxSubOscillators[currentChannel] = selectedSubOscillator;
                } else {
                    auxSubOscillators[currentChannel] =
                        inputs[INPUT_AUX_SUBOSCILLATOR].getVoltageSimd<float_4>(channel);
                    auxSubOscillators[currentChannel] =
                        simd::round(auxSubOscillators[currentChannel]);
                    auxSubOscillators[currentChannel] =
                        simd::clamp(auxSubOscillators[currentChannel], 0.f, 4.f);
                }

                if (bHaveInputModel && bNotesModelSelection) {
                    selectedModels[currentChannel] = inputs[INPUT_MODEL].getVoltageSimd<float_4>(channel);
                    selectedModels[currentChannel] += 4.f;
                    selectedModels[currentChannel] *= 12.f;
                    selectedModels[currentChannel] = simd::round(selectedModels[currentChannel]);
                    selectedModels[currentChannel] = simd::clamp(selectedModels[currentChannel], 0.f, 23.f);
                } else {
                    selectedModels[currentChannel] = knobModel;
                }

                if (!bHaveInputLpgColor) {
                    lpgColorVoltages[currentChannel] = knobLpgColor;
                } else {
                    lpgColorVoltages[currentChannel] = inputs[INPUT_LPG_COLOR].getVoltageSimd<float_4>(channel);
                    lpgColorVoltages[currentChannel] /= 5.f;
                    lpgColorVoltages[currentChannel] *= attenLpgColor;
                    lpgColorVoltages[currentChannel] += knobLpgColor;
                    lpgColorVoltages[currentChannel] = simd::clamp(lpgColorVoltages[currentChannel], 0.f, 1.f);
                }

                if (!bHaveInputLpgDecay) {
                    lpgDecayVoltages[channel] = knobLpgDecay;
                } else {
                    lpgDecayVoltages[currentChannel] = inputs[INPUT_LPG_DECAY].getVoltageSimd<float_4>(channel);
                    lpgDecayVoltages[currentChannel] /= 5.f;
                    lpgDecayVoltages[currentChannel] *= attenLpgDecay;
                    lpgDecayVoltages[currentChannel] += knobLpgDecay;
                    lpgDecayVoltages[currentChannel] = simd::clamp(lpgDecayVoltages[currentChannel], 0.f, 1.f);
                }
            }

            dsp::Frame<PORT_MAX_CHANNELS * 2> renderFrames[plaits::kBlockSize];

            for (int channel = 0; channel < channelCount; ++channel) {
                currentChannel = channel >> 2;
                individualChannel = channel % 4;

                int selectedModel = static_cast<int>(selectedModels[currentChannel][individualChannel]);

                // Switch models.
                if (selectedModel != patches[channel].engine) {
                    patches[channel].engine = selectedModel;
                }

                // Update patches
                // Similar implementation to original Plaits ui.cc code.
                // TODO: Check with low cpu mode.
                switch (frequencyMode) {
                case funes::FM_LFO:
                    patches[channel].note = -48.37f + pitch * 15.f;
                    break;
                case funes::FM_OCTAVES: {
                    float fineTune = params[PARAM_FREQUENCY_ROOT].getValue() / 4.f;
                    patches[channel].note = 53.f + fineTune * 14.f + 12.f *
                        static_cast<float>(octaveQuantizers[channel].Process(0.5f * pitch / 4.f + 0.5f) - 4.f);
                    break;
                }
                case funes::FM_FULL:
                    patches[channel].note = 60.f + pitch * 12.f;
                    break;
                default:
                    patches[channel].note = static_cast<float>(frequencyMode) * 12.f + pitch * 7.f / 4.f;
                    break;
                }

                patches[channel].harmonics = knobHarmonics;
                patches[channel].timbre = knobTimbre;
                patches[channel].morph = knobMorph;
                patches[channel].lpg_colour = lpgColorVoltages[currentChannel][individualChannel];
                patches[channel].decay = lpgDecayVoltages[currentChannel][individualChannel];
                patches[channel].frequency_modulation_amount = attenFm;
                patches[channel].timbre_modulation_amount = attenTimbre;
                patches[channel].morph_modulation_amount = attenMorph;
                patches[channel].chordBank = chordBanks[channel];
                patches[channel].auxCrossfade = knobAuxCrossfade;
                suboscillatorModes[channel] =
                    static_cast<funes::SuboscillatorModes>(auxSubOscillators[currentChannel][individualChannel]);
                bIsSubOscillatorOctave = suboscillatorModes[channel] > funes::SUBOSCILLATOR_SINE;
                patches[channel].auxSuboscillatorWave = bIsSubOscillatorOctave ? funes::SUBOSCILLATOR_SINE :
                    suboscillatorModes[channel];
                patches[channel].auxSuboscillatorOctave = (suboscillatorModes[channel] - 2) * bIsSubOscillatorOctave;

                holdModulations[channel] = bWantHoldModulations |
                    (inputs[INPUT_HOLD_MODULATIONS].getVoltage(channel) >= 1.f);
                patches[channel].wantHoldModulations = holdModulations[channel];

                // Render output buffer for each channel.
                inputVoltages[0] = inputs[INPUT_MODEL].getVoltage(channel);
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
                plaits::Voice::Frame output[plaits::kBlockSize];
                voices[channel].Render(patches[channel], modulations[channel], output, plaits::kBlockSize);
                // Convert output to frames
                const int channelFrame = channel << 1;
                for (size_t blockNum = 0; blockNum < plaits::kBlockSize; ++blockNum) {
                    renderFrames[blockNum].samples[channelFrame] = output[blockNum].out / 32768.f;
                    renderFrames[blockNum].samples[channelFrame + 1] = output[blockNum].aux / 32768.f;
                }
            }

            // Convert output.
            if (!bWantLowCpu) {
                int inLen = plaits::kBlockSize;
                int outLen = drbOutputBuffers.capacity();
                srcOutputs.setChannels(channelCount << 1);
                srcOutputs.process(renderFrames, &inLen, drbOutputBuffers.endData(), &outLen);
                drbOutputBuffers.endIncr(outLen);
            } else {
                int len = std::min(static_cast<int>(drbOutputBuffers.capacity()), static_cast<int>(plaits::kBlockSize));
                std::memcpy(drbOutputBuffers.endData(), renderFrames, len * sizeof(renderFrames[0]));
                drbOutputBuffers.endIncr(len);
            }
        }

        // Set output
        if (!drbOutputBuffers.empty()) {
            dsp::Frame<PORT_MAX_CHANNELS * 2> outputFrames = drbOutputBuffers.shift();
            float_4 outVoltages;
            float_4 auxVoltages;
            int currentSample;
            for (int channel = 0; channel < channelCount; channel += 4) {
                currentSample = channel << 1;
                // Inverting op-amp on outputs
                outVoltages[0] = -outputFrames.samples[currentSample];
                auxVoltages[0] = -outputFrames.samples[currentSample + 1];
                outVoltages[1] = -outputFrames.samples[currentSample + 2];
                auxVoltages[1] = -outputFrames.samples[currentSample + 3];
                outVoltages[2] = -outputFrames.samples[currentSample + 4];
                auxVoltages[2] = -outputFrames.samples[currentSample + 5];
                outVoltages[3] = -outputFrames.samples[currentSample + 6];
                auxVoltages[3] = -outputFrames.samples[currentSample + 7];
                outVoltages *= 5.f;
                auxVoltages *= 5.f;

                outputs[OUTPUT_OUT].setVoltageSimd(outVoltages, channel);
                outputs[OUTPUT_AUX].setVoltageSimd(auxVoltages, channel);
            }
        }

        outputs[OUTPUT_OUT].setChannels(channelCount);
        outputs[OUTPUT_AUX].setChannels(channelCount);

        // Update model text, custom data lights, chord bank light and frequency mode every 16 samples only.
        if (lightsDivider.process()) {
            const float sampleTime = args.sampleTime * jitteredLightsFrequency;

            if (displayChannel >= channelCount) {
                displayChannel = channelCount - 1;
            }

            if (bDisplayModulatedModel) {
                displayText = funes::displayLabels[voices[displayChannel].active_engine()];
            } else {
                displayText = funes::displayLabels[knobModel];
            }

            int currentLight;
            int currentModel;
            bool bIsChannelActive;
            for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
                currentLight = LIGHT_CHANNEL + channel * 3;
                currentModel = voices[channel].active_engine();
                bIsChannelActive = channel < channelCount;
                lights[currentLight].setBrightnessSmooth(funesmk2::lightColors[currentModel].red *
                    bIsChannelActive, sampleTime);
                lights[currentLight + 1].setBrightnessSmooth(funesmk2::lightColors[currentModel].green *
                    bIsChannelActive, sampleTime);
                lights[currentLight + 2].setBrightnessSmooth(funesmk2::lightColors[currentModel].blue *
                    bIsChannelActive, sampleTime);
            }

            frequencyMode = params[PARAM_FREQ_MODE].getValue();

            const int activeEngine = voices[displayChannel].active_engine();

            bool bCanUseCustomData = isEngineCustomizable(activeEngine);

            bool bHasCustomData = customDataStates[displayChannel] == activeEngine;

            lights[LIGHT_FACTORY_DATA].setBrightness(((bCanUseCustomData & !bHasCustomData) &
                (errorTimeOut == 0)) * kSanguineButtonLightValue);
            lights[LIGHT_CUSTOM_DATA].setBrightness(((bCanUseCustomData & bHasCustomData) &
                (errorTimeOut == 0)) * kSanguineButtonLightValue);
            lights[LIGHT_CUSTOM_DATA + 1].setBrightness((bCanUseCustomData &
                (errorTimeOut > 0)) * kSanguineButtonLightValue);

            if (errorTimeOut != 0) {
                --errorTimeOut;

                if (errorTimeOut <= 0) {
                    errorTimeOut = 0;
                }
            }

            bool bEngineHasChords = (patches[displayChannel].engine == 6) |
                (patches[displayChannel].engine == 7) | (patches[displayChannel].engine == 14);

            lights[LIGHT_CHORD_BANK].setBrightness((bEngineHasChords & ((chordBanks[displayChannel] == 0) |
                (chordBanks[displayChannel] == 2))) * kSanguineButtonLightValue);
            lights[LIGHT_CHORD_BANK + 1].setBrightness((bEngineHasChords &
                (chordBanks[displayChannel] > 0)) * kSanguineButtonLightValue);

            lights[LIGHT_AUX_SUBOSCILLATOR].setBrightness(
                (suboscillatorModes[displayChannel] > funes::SUBOSCILLATOR_SQUARE) * kSanguineButtonLightValue);

            lights[LIGHT_AUX_SUBOSCILLATOR + 1].setBrightness(
                ((suboscillatorModes[displayChannel] == funes::SUBOSCILLATOR_SQUARE) |
                    (suboscillatorModes[displayChannel] == funes::SUBOSCILLATOR_SINE_MINUS_ONE)) *
                kSanguineButtonLightValue);

            lights[LIGHT_AUX_SUBOSCILLATOR + 2].setBrightness(
                (suboscillatorModes[displayChannel] == funes::SUBOSCILLATOR_SINE_MINUS_TWO) *
                kSanguineButtonLightValue);

            lights[LIGHT_HOLD_MODULATIONS].setBrightness(holdModulations[displayChannel] * kSanguineButtonLightValue);
        }
    }

    void init() {
        setEngine(8);
        for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
            patches[channel].lpg_colour = 0.5f;
            patches[channel].decay = 0.5f;
            patches[channel].engine = 8;
        }

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
            case INPUT_MODEL:
                bHaveInputModel = e.connecting;
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
            case INPUT_LPG_COLOR:
                bHaveInputLpgColor = e.connecting;
                break;
            case INPUT_LPG_DECAY:
                bHaveInputLpgDecay = e.connecting;
                break;
            case INPUT_CHORD_BANK:
                bHaveInputChordBank = e.connecting;
                break;
            case INPUT_AUX_SUBOSCILLATOR:
                bHaveInputAuxSuboscillator = e.connecting;
                break;
            case INPUT_HOLD_MODULATIONS:
                bHaveInputHoldModulations = e.connecting;
            default:
                break;
            }
        }
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        srcOutputs.setRates(static_cast<int>(plaits::kSampleRate), static_cast<int>(e.sampleRate));
    }

    void onAdd(const AddEvent& e) override {
        jitteredLightsFrequency = kLightsFrequency + (getId() % kLightsFrequency);
        lightsDivider.setDivision(jitteredLightsFrequency);

        const std::string patchStorageDir = getPatchStorageDirectory();
        const int64_t moduleId = getId();

        for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
            const std::string userDataFilePath = system::join(patchStorageDir,
                string::f(funesmk2::kUserDataFileName, moduleId, channel));
            FILE* userDataFile = fopen(userDataFilePath.c_str(), "rb");

            if (userDataFile != NULL) {
                uint8_t userDataBuffer[plaits::UserData::MAX_USER_DATA_SIZE];

                fread(userDataBuffer, sizeof(uint8_t), plaits::UserData::MAX_USER_DATA_SIZE,
                    userDataFile);
                if (userDataBuffer[plaits::UserData::MAX_USER_DATA_SIZE - 2] == 'U') {
                    userDatas[channel].setBuffer(userDataBuffer);
                    voices[channel].ReloadUserData();
                    customDataStates[channel] = userDataBuffer[plaits::UserData::MAX_USER_DATA_SIZE - 1] - ' ';
                }
                fclose(userDataFile);
            }
        }
    }

    void onSave(const SaveEvent& e) override {
        const std::string patchStorageDir = createPatchStorageDirectory();
        const int64_t moduleId = getId();

        for (int channel = 0; channel < channelCount; ++channel) {
            const uint8_t* userDataBuffer = userDatas[channel].getBuffer();

            const std::string userDataFilePath = system::join(patchStorageDir,
                string::f(funesmk2::kUserDataFileName, moduleId, channel));

            if (userDataBuffer != nullptr) {
                FILE* userDataFile = fopen(userDataFilePath.c_str(), "wb");

                if (userDataFile != NULL) {
                    fwrite(userDataBuffer, sizeof(uint8_t), plaits::UserData::MAX_USER_DATA_SIZE, userDataFile);
                    fclose(userDataFile);
                }
            } else {
                if (system::exists(userDataFilePath)) {
                    system::remove(userDataFilePath);
                }
            }
        }
    }

    json_t* dataToJson() override {
        json_t* rootJ = SanguineModule::dataToJson();

        setJsonBoolean(rootJ, "lowCpu", bWantLowCpu);
        setJsonBoolean(rootJ, "displayModulatedModel", bDisplayModulatedModel);
        setJsonInt(rootJ, "frequencyMode", frequencyMode);
        setJsonBoolean(rootJ, "notesModelSelection", bNotesModelSelection);
        setJsonInt(rootJ, "displayChannel", displayChannel);

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
    }

    void resetCustomData() {
        bool success = userDatas[displayChannel].Save(nullptr, patches[displayChannel].engine);
        if (success) {
            for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
                voices[channel].ReloadUserData();
            }
            customDataStates[displayChannel] = 0;
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
            bool success = userDatas[displayChannel].Save(dataBuffer, patches[displayChannel].engine);
            if (success) {
                for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
                    voices[channel].ReloadUserData();
                }
                // Only 1 engine per channel can use custom data at a time.
                customDataStates[displayChannel] = patches[displayChannel].engine;
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
    }

    void toggleModulatedDisplay() {
        bDisplayModulatedModel = !bDisplayModulatedModel;
    }

    void toggleNotesModelSelection() {
        bNotesModelSelection = !bNotesModelSelection;
        if (bNotesModelSelection) {
            inputs[INPUT_MODEL].setChannels(0);
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
        params[PARAM_CHORD_BANK].setValue(chordBankNum);
    }

    void setSuboscillatorMode(int suboscillatorModeNum) {
        params[PARAM_AUX_SUBOSCILLATOR].setValue(suboscillatorModeNum);
    }

    void toggleHoldVoltagesOnTrigger() {
        bWantHoldModulations = !bWantHoldModulations;
        params[PARAM_HOLD_MODULATIONS].setValue(static_cast<float>(bWantHoldModulations));
    }

    bool isEngineCustomizable(const int engineNumber) {
        return ((engineNumber >= 2) & (engineNumber <= 5)) | (engineNumber == 13);
    }
};

struct FunesMk2Widget : SanguineModuleWidget {
    explicit FunesMk2Widget(FunesMk2* module) {
        setModule(module);

        moduleName = "funesmk2";
        panelSize = SIZE_17;
        backplateColor = PLATE_PURPLE;

        makePanel();

        addScrews(SCREW_ALL);

        FramebufferWidget* funesFramebuffer = new FramebufferWidget();
        addChild(funesFramebuffer);

        SanguineLedDisplayRounded* customDataDisplay =
            new SanguineLedDisplayRounded(76.999f, 11.244f, 11.201f, 2.835f, 3.625f);
        funesFramebuffer->addChild(customDataDisplay);

        addChild(createLightCentered<SmallSimpleLight<GreenLight>>(millimetersToPixelsVec(72.947, 11.244),
            module, FunesMk2::LIGHT_FACTORY_DATA));
        addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(millimetersToPixelsVec(80.952, 11.244),
            module, FunesMk2::LIGHT_CUSTOM_DATA));

        addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(7.821, 26.029), module,
            FunesMk2::INPUT_MODEL));
        SanguineMatrixDisplay* alphaDisplay = new SanguineMatrixDisplay(8, module, 43.18, 26.029);
        funesFramebuffer->addChild(alphaDisplay);
        alphaDisplay->fallbackString = funes::displayLabels[8];
        if (module) {
            alphaDisplay->values.displayText = &module->displayText;
        }
        addParam(createParamCentered<Sanguine1SGray>(millimetersToPixelsVec(76.949, 26.029), module, FunesMk2::PARAM_MODEL));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(7.821, 36.379), module, FunesMk2::PARAM_FREQUENCY_ROOT));

        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(7.821, 46.753), module, FunesMk2::INPUT_FREQUENCY));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.797, 46.741), module, FunesMk2::PARAM_FREQUENCY_CV));
        addParam(createParamCentered<Sanguine1PRed>(millimetersToPixelsVec(33.794, 46.932), module, FunesMk2::PARAM_FREQUENCY));

        SanguineLedDisplayRounded* channelDisplay =
            new SanguineLedDisplayRounded(43.18f, 66.812f, 2.782f, 45.893f, 3.558f);
        funesFramebuffer->addChild(channelDisplay);

        const float baseY = 45.512f;

        const float lightOffsetY = 2.84;

        float currentY = baseY;

        for (int light = 0; light < PORT_MAX_CHANNELS; ++light) {
            addChild(createLightCentered<SmallSimpleLight<RedGreenBlueLight>>(millimetersToPixelsVec(43.18, currentY),
                module, FunesMk2::LIGHT_CHANNEL + light * 3));
            currentY += lightOffsetY;
        }

        addParam(createParamCentered<Sanguine1PGreen>(millimetersToPixelsVec(52.566, 46.932), module, FunesMk2::PARAM_HARMONICS));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(67.556, 46.741), module, FunesMk2::PARAM_HARMONICS_CV));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(78.545, 46.753), module, FunesMk2::INPUT_HARMONICS));

        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(7.821, 56.914), module, FunesMk2::PARAM_FREQ_MODE));

        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(61.35, 56.914),
            module, FunesMk2::PARAM_CHORD_BANK, FunesMk2::LIGHT_CHORD_BANK));
        addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(78.545, 56.914), module, FunesMk2::INPUT_CHORD_BANK));

        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(7.821, 67.099), module, FunesMk2::INPUT_TIMBRE));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.797, 67.087), module, FunesMk2::PARAM_TIMBRE_CV));
        addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(33.794, 67.281), module, FunesMk2::PARAM_TIMBRE));

        addParam(createParamCentered<Sanguine1PPurple>(millimetersToPixelsVec(52.566, 67.281), module, FunesMk2::PARAM_MORPH));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(67.556, 67.087), module, FunesMk2::PARAM_MORPH_CV));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(78.545, 67.099), module, FunesMk2::INPUT_MORPH));

        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(7.821, 87.667), module, FunesMk2::INPUT_LPG_COLOR));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(18.797, 87.667), module, FunesMk2::PARAM_LPG_COLOR_CV));
        addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(33.794, 87.849), module, FunesMk2::PARAM_LPG_COLOR));

        addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(52.566, 87.849), module, FunesMk2::PARAM_LPG_DECAY));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(67.556, 87.667), module, FunesMk2::PARAM_LPG_DECAY_CV));
        addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(78.545, 87.667), module, FunesMk2::INPUT_LPG_DECAY));

        addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(7.821, 101.783), module, FunesMk2::INPUT_HOLD_MODULATIONS));
        addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(18.797, 101.783), module,
            FunesMk2::PARAM_HOLD_MODULATIONS, FunesMk2::LIGHT_HOLD_MODULATIONS));

        addChild(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedGreenBlueLight>>>(millimetersToPixelsVec(43.18, 101.783), module,
            FunesMk2::PARAM_AUX_SUBOSCILLATOR, FunesMk2::LIGHT_AUX_SUBOSCILLATOR));

        addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(78.545, 101.783), module,
            FunesMk2::INPUT_AUX_CROSSFADE));

        addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.821, 117.152), module, FunesMk2::INPUT_TRIGGER));
        addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(18.797, 117.152), module, FunesMk2::INPUT_LEVEL));
        addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(29.774, 117.152), module, FunesMk2::INPUT_NOTE));

        addInput(createInputCentered<BananutBlackPoly>(millimetersToPixelsVec(43.18, 117.152), module, FunesMk2::INPUT_AUX_SUBOSCILLATOR));

        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(56.567, 117.152), module, FunesMk2::OUTPUT_OUT));
        addParam(createParamCentered<Trimpot>(millimetersToPixelsVec(67.556, 114.133), module, FunesMk2::PARAM_AUX_CROSSFADE));
        addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(78.545, 117.152), module, FunesMk2::OUTPUT_AUX));
    }

    void appendContextMenu(Menu* menu) override {
        SanguineModuleWidget::appendContextMenu(menu);

        FunesMk2* module = dynamic_cast<FunesMk2*>(this->module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Synthesis model", "", [=](Menu* menu) {
            menu->addChild(createSubmenuItem("New", "", [=](Menu* menu) {
                for (int i = 0; i < 8; ++i) {
                    menu->addChild(createCheckMenuItem(funes::modelLabels[i], "",
                        [=]() { return module->knobModel == i; },
                        [=]() {	module->setEngine(i); }
                    ));
                }
                }));

            menu->addChild(createSubmenuItem("Pitched", "", [=](Menu* menu) {
                for (int i = 8; i < 16; ++i) {
                    menu->addChild(createCheckMenuItem(funes::modelLabels[i], "",
                        [=]() { return module->knobModel == i; },
                        [=]() {	module->setEngine(i); }
                    ));
                }
                }));

            menu->addChild(createSubmenuItem("Noise/percussive", "", [=](Menu* menu) {
                for (int i = 16; i < 24; ++i) {
                    menu->addChild(createCheckMenuItem(funes::modelLabels[i], "",
                        [=]() { return module->knobModel == i; },
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
            [=]() {return module->selectedChordBank; },
            [=](int i) {module->setChordBank(i); }
        ));

        menu->addChild(createCheckMenuItem("Hold modulation voltages on trigger", "",
            [=]() {return module->bWantHoldModulations; },
            [=]() {module->toggleHoldVoltagesOnTrigger(); }
        ));

        menu->addChild(createIndexSubmenuItem("Aux sub-oscillator", funes::suboscillatorLabels,
            [=]() {return static_cast<int>(module->selectedSubOscillator); },
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

        menu->addChild(createSubmenuItem(string::f("Channel %d custom data", module->displayChannel + 1), "",
            [=](Menu* menu) {
                if (module->isEngineCustomizable(module->patches[module->displayChannel].engine)) {
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

Model* modelFunesMk2 = createModel<FunesMk2, FunesMk2Widget>("FunesMk2");