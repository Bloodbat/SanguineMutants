#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"
#include "sanguinejson.hpp"
#include "sanguinetimers.hpp"

#include "grids/reticula_pattern_generator.hpp"
#include "reticula.hpp"

struct Reticula : SanguineModule {
    enum ParamIds {
        PARAM_CLOCK,
        PARAM_SWING,
        PARAM_MAP_X,
        PARAM_MAP_Y,
        PARAM_CHAOS,
        PARAM_TAP,
        PARAM_RUN,
        PARAM_SEQUENCER_MODE,
        PARAM_BD_DENSITY,
        PARAM_SD_DENSITY,
        PARAM_HH_DENSITY,
        PARAM_RESET,
        PARAM_CLOCK_OUTPUT_SOURCE,
        PARAMS_COUNT
    };

    enum InputIds {
        INPUT_EXTERNAL_CLOCK,
        INPUT_SWING,
        INPUT_MAP_X,
        INPUT_MAP_Y,
        INPUT_RESET,
        INPUT_RUN,
        INPUT_CHAOS,
        INPUT_FILL_BD,
        INPUT_FILL_SD,
        INPUT_FILL_HH,
        INPUTS_COUNT
    };

    enum OutputIds {
        OUTPUT_CLOCK,
        OUTPUT_RESET,
        OUTPUT_TRIGGER_BD,
        OUTPUT_TRIGGER_SD,
        OUTPUT_TRIGGER_HH,
        OUTPUT_ACCENT_BD,
        OUTPUT_ACCENT_SD,
        OUTPUT_ACCENT_HH,
        OUTPUTS_COUNT
    };

    enum LightIds {
        ENUMS(LIGHT_TAP, 2),
        LIGHT_RUN,
        ENUMS(LIGHT_SEQUENCER_MODE, 2),
        LIGHT_CHANNEL_BD,
        LIGHT_CHANNEL_SD,
        LIGHT_CHANNEL_HH,
        LIGHT_RESET,
        ENUMS(LIGHT_CLOCK_OUTPUT_SOURCE, 2),
        LIGHTS_COUNT
    };

    static const int kMaxChannels = 3;
    static const int kMaxDrumPorts = 6;
    static const int kLightsFrequency = 16;

    int elapsedTicks = 0;
    int sequenceStep = 0;
    int64_t tapStamps[2] = { 0,0 };
    uint8_t tickCount;

    reticula::PatternGeneratorModes sequencerMode = reticula::PATTERN_ORIGINAL;
    reticula::GateModes runMode = reticula::MODE_TRIGGER;
    reticula::GateModes outputMode = reticula::MODE_TRIGGER;
    reticula::ClockOutputSources clockOutputSource = reticula::CLOCK_SOURCE_FIRST_BEAT;

    reticula::ExternalClockResolutions extClockResolution = reticula::RESOLUTION_24_PPQN;

    bool advanceStep = false;
    bool bUseExternalClock = false;
    bool bNeedExternalReset = true;
    bool bIsModuleRunning = false;
    bool bUseTapTempo = false;
    bool bWantSequencerResets = true;

    // Drum triggers.
    bool gateStates[kMaxDrumPorts];

    dsp::ClockDivider lightsDivider;

    dsp::BooleanTrigger btRun;
    dsp::BooleanTrigger btReset;
    dsp::BooleanTrigger btTapTempo;

    dsp::SchmittTrigger stClock;
    dsp::SchmittTrigger stReset;
    dsp::SchmittTrigger stRun;

    dsp::PulseGenerator pgClock;
    dsp::PulseGenerator pgReset;
    dsp::PulseGenerator pgDrums[kMaxDrumPorts];

    Metronome metronome;
    reticula::PatternGenerator patternGenerator;

    float tempoParam = 120.f;

    float swing = 0.5f;
    float swingHighTempo = 0.f;
    float swingLowTempo = 0.f;

    float mapX = 0.f;
    float mapY = 0.f;
    float chaos = 0.f;
    float BDFill = 0.f;
    float SNFill = 0.f;
    float HHFill = 0.f;

    float lastKnobTempo = 0.f;

    std::string tempoDisplay = "120";

    struct ReticulaKnob : ParamQuantity {
        std::string getDisplayValueString() override {
            Reticula* moduleReticula = static_cast<Reticula*>(module);
            if (moduleReticula->sequencerMode == reticula::PATTERN_EUCLIDEAN) {
                int euclidianLength = (static_cast<int>(getValue()) >> 3) + 1;
                return string::f("%d", euclidianLength);
            } else {
                return ParamQuantity::getDisplayValueString();
            }
        }

        std::string getLabel() override {
            Reticula* moduleReticula = static_cast<Reticula*>(module);
            if (moduleReticula->sequencerMode == reticula::PATTERN_EUCLIDEAN) {
                return "Length";
            } else {
                return ParamQuantity::getLabel();
            }
        }

        std::string getUnit() override {
            Reticula* moduleReticula = static_cast<Reticula*>(module);
            if (moduleReticula->sequencerMode == reticula::PATTERN_EUCLIDEAN) {
                return "";
            } else {
                return ParamQuantity::getUnit();
            }
        }

        void setDisplayValue(float 	displayValue) override {
            Reticula* moduleReticula = static_cast<Reticula*>(module);
            if (moduleReticula->sequencerMode == reticula::PATTERN_EUCLIDEAN) {
                float lengthValue = (static_cast<int>(displayValue) << 3) - 1;
                setImmediateValue(lengthValue);
            } else {
                ParamQuantity::setDisplayValue(displayValue);
            }
        }
    };

    Reticula() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

        configInput(INPUT_EXTERNAL_CLOCK, "External clock");

        configParam(PARAM_CLOCK, 40.f, 240.f, 120.f, "Clock", " BPM");

        configParam<ReticulaKnob>(PARAM_MAP_X, 0.f, 255.f, 0.f, "Map X", "%", 0.f, (1.f / 255.f) * 100.f);
        configParam<ReticulaKnob>(PARAM_MAP_Y, 0.f, 255.f, 0.f, "Map Y", "%", 0.f, (1.f / 255.f) * 100.f);
        configParam<ReticulaKnob>(PARAM_CHAOS, 0.f, 255.f, 0.f, "Chaos", "%", 0.f, (1.f / 255.f) * 100);

        configButton(PARAM_TAP, "Tap / Reset");

        configInput(INPUT_MAP_X, "Map X CV");
        configInput(INPUT_MAP_Y, "Map Y CV");
        configInput(INPUT_CHAOS, "Chaos CV");

        configInput(INPUT_RESET, "Reset");

        configButton(PARAM_RUN, "Run");

        configInput(INPUT_RUN, "Run");

        configSwitch(PARAM_SEQUENCER_MODE, 0.f, 2.f, 0.f, "Sequencer mode", reticula::sequencerModeLabels);
        configSwitch(PARAM_CLOCK_OUTPUT_SOURCE, 0.f, 2.f, 0.f, "Clock output source", reticula::clockOutputSourceLabels);

        configParam(PARAM_SWING, 0.f, 9.f, 0.f, "Swing", "%", 0.f, 10.f);
        configParam(PARAM_BD_DENSITY, 0.f, 255.f, 127.5f, "Bass drum fill rate", "%", 0.f, (1.f / 255.f) * 100.f);
        configParam(PARAM_SD_DENSITY, 0.f, 255.f, 127.5f, "Snare drum fill rate", "%", 0.f, (1.f / 255.f) * 100.f);
        configParam(PARAM_HH_DENSITY, 0.f, 255.f, 127.5f, "Hi-hat fill rate", "%", 0.f, (1.f / 255.f) * 100.f);

        configInput(INPUT_SWING, "Swing CV");
        configInput(INPUT_FILL_BD, "Bass drum fill CV");
        configInput(INPUT_FILL_SD, "Snare drum fill CV");
        configInput(INPUT_FILL_HH, "Hi-hat fill CV");

        configOutput(OUTPUT_CLOCK, "Clock");
        configOutput(OUTPUT_TRIGGER_BD, "Bass drum trigger");
        configOutput(OUTPUT_TRIGGER_SD, "Snare drum trigger");
        configOutput(OUTPUT_TRIGGER_HH, "Hi-hat trigger");

        configOutput(OUTPUT_RESET, "Reset");
        configOutput(OUTPUT_ACCENT_BD, "Bass drum accent");
        configOutput(OUTPUT_ACCENT_SD, "Snare drum accent");
        configOutput(OUTPUT_ACCENT_HH, "Hi-hat accent");

        lightsDivider.setDivision(kLightsFrequency);

        metronome = Metronome(120, APP->engine->getSampleRate(), 24.f, 0.f);
        metronome.setUndividedClockTap(true);
        tickCount = reticula::ticksGranularities[2];

        srand(time(NULL));

        for (int i = 0; i < 6; ++i) {
            gateStates[i] = false;
        }

    }

    void process(const ProcessArgs& args) override {
        bool bIsRunConnected = inputs[INPUT_RUN].isConnected();

        if (runMode == reticula::MODE_TRIGGER) {
            if (!bIsRunConnected && (btRun.process(params[PARAM_RUN].getValue()) ||
                stRun.process(inputs[INPUT_RUN].getVoltage()))) {
                bIsModuleRunning = !bIsModuleRunning;
            }
        } else {
            if (bIsRunConnected) {
                bIsModuleRunning = inputs[INPUT_RUN].getVoltage() >= 1.f;
            } else {
                if (btRun.process(params[PARAM_RUN].getValue())) {
                    bIsModuleRunning = !bIsModuleRunning;
                }
            }

            if (!bIsModuleRunning) {
                metronome.reset();
            }
        }

        if (stReset.process(inputs[INPUT_RESET].getVoltage()) ||
            btReset.process(params[PARAM_RESET].getValue())) {
            patternGenerator.reset();
            metronome.reset();
            sequenceStep = 0;
            elapsedTicks = 0;
            pgReset.trigger();
        }

        // Clock, tempo, tap tempo and swing.
        if (btTapTempo.process(params[PARAM_TAP].getValue())) {
            tapStamps[0] = tapStamps[1];
            tapStamps[1] = args.frame;

            if (tapStamps[0] != 0) {
                int64_t tapDifferencce = tapStamps[1] - tapStamps[0];
                tempoParam = (60 * args.sampleRate) / tapDifferencce;

                lastKnobTempo = params[PARAM_CLOCK].getValue();
                bUseTapTempo = true;
            }
        }

        float newKnobTempo = params[PARAM_CLOCK].getValue();
        if (newKnobTempo != lastKnobTempo) {
            tempoParam = newKnobTempo;
            bUseTapTempo = false;
            lastKnobTempo = newKnobTempo;
        }

        bUseExternalClock = inputs[INPUT_EXTERNAL_CLOCK].isConnected();

        if (bIsModuleRunning) {
            if (tempoParam >= 30 && tempoParam <= 480) {
                swing = clamp(params[PARAM_SWING].getValue() + inputs[INPUT_SWING].getVoltage() / 5.f, 0.f, 0.9f);
                swingHighTempo = tempoParam / (1 - swing);
                swingLowTempo = tempoParam / (1 + swing);
                if (elapsedTicks < 6) {
                    metronome.setTempo(swingLowTempo);
                } else {
                    metronome.setTempo(swingHighTempo);
                }
            }

            // External clock select
            if (bUseExternalClock) {
                if (bNeedExternalReset) {
                    patternGenerator.reset();
                    bNeedExternalReset = false;
                }
                tickCount = reticula::ticksGranularities[extClockResolution];
            } else {
                bNeedExternalReset = true;
                tickCount = reticula::ticksGranularities[2];
                metronome.process();
            }

            mapX = params[PARAM_MAP_X].getValue() + ((inputs[INPUT_MAP_X].getVoltage() / 5.f) * 255.f);
            mapX = clamp(mapX, 0.f, 255.f);
            mapY = params[PARAM_MAP_Y].getValue() + ((inputs[INPUT_MAP_Y].getVoltage() / 5.f) * 255.f);
            mapY = clamp(mapY, 0.f, 255.f);
            BDFill = params[PARAM_BD_DENSITY].getValue() + ((inputs[INPUT_FILL_BD].getVoltage() / 5.f) * 255.f);
            BDFill = clamp(BDFill, 0.f, 255.f);
            SNFill = params[PARAM_SD_DENSITY].getValue() + ((inputs[INPUT_FILL_SD].getVoltage() / 5.f) * 255.f);
            SNFill = clamp(SNFill, 0.f, 255.f);
            HHFill = params[PARAM_HH_DENSITY].getValue() + ((inputs[INPUT_FILL_HH].getVoltage() / 5.f) * 255.f);
            HHFill = clamp(HHFill, 0.f, 255.f);
            chaos = params[PARAM_CHAOS].getValue() + ((inputs[INPUT_CHAOS].getVoltage() / 5.f) * 255.f);
            chaos = clamp(chaos, 0.f, 255.f);

            if (bUseExternalClock) {
                if (stClock.process(inputs[INPUT_EXTERNAL_CLOCK].getVoltage())) {
                    advanceStep = true;
                }
            } else if (metronome.hasTicked()) {
                advanceStep = true;
                ++elapsedTicks;
                elapsedTicks %= 12;
            } else {
                advanceStep = false;
            }

            patternGenerator.setMapX(mapX);
            patternGenerator.setMapY(mapY);
            patternGenerator.setBDDensity(BDFill);
            patternGenerator.setSDDensity(SNFill);
            patternGenerator.setHHDensity(HHFill);
            patternGenerator.setRandomness(chaos);

            patternGenerator.setEuclideanLength(0, mapX);
            patternGenerator.setEuclideanLength(1, mapY);
            patternGenerator.setEuclideanLength(2, chaos);

            bool bDoClock = false;

            if (advanceStep) {
                patternGenerator.tick(tickCount);
                for (int i = 0; i < kMaxDrumPorts; ++i) {
                    if (patternGenerator.getDrumState(i)) {
                        gateStates[i] = true;
                        if (outputMode == reticula::MODE_TRIGGER) {
                            pgDrums[i].trigger();
                        }
                    }
                }

                bDoClock = patternGenerator.getClockState();

                if (patternGenerator.getResetState() && bWantSequencerResets) {
                    pgReset.trigger();
                }

                ++sequenceStep;
                if (sequenceStep >= 32) {
                    sequenceStep = 0;
                }
                advanceStep = false;
            }

            switch (clockOutputSource) {
            case reticula::CLOCK_SOURCE_FIRST_BEAT:
                bDoClock = bIsModuleRunning && patternGenerator.getFirstBeat();
                break;

            case reticula::CLOCK_SOURCE_BEAT:
                bDoClock = bIsModuleRunning && patternGenerator.getBeat();
                break;

            case reticula::CLOCK_SOURCE_PATTERN:
                bDoClock = bDoClock && bIsModuleRunning;
                break;
            }

            lights[LIGHT_TAP + 0].setBrightnessSmooth((bDoClock && !bUseTapTempo) || (bDoClock && bUseExternalClock) ?
                kSanguineButtonLightValue : 0.f, args.sampleTime);
            lights[LIGHT_TAP + 1].setBrightnessSmooth((bDoClock && bUseTapTempo) || (bDoClock && bUseExternalClock) ?
                kSanguineButtonLightValue : 0.f, args.sampleTime);

            if (bDoClock) {
                pgClock.trigger();
            }
        }

        outputs[OUTPUT_CLOCK].setVoltage(pgClock.process(args.sampleTime) * 10.f);
        outputs[OUTPUT_RESET].setVoltage(pgReset.process(args.sampleTime) * 10.f);

        for (int drum = 0; drum < kMaxDrumPorts; ++drum) {
            float drumVoltage = 0.f;
            if (outputMode == reticula::MODE_TRIGGER) {
                drumVoltage = static_cast<float>(pgDrums[drum].process(args.sampleTime)) * 10.f;
            } else {
                if (!bUseExternalClock) {
                    if (metronome.getElapsedTickTime() < 0.5 && gateStates[drum]) {
                        drumVoltage = 10.f;
                    } else {
                        gateStates[drum] = false;
                    }
                } else {
                    if (inputs[INPUT_EXTERNAL_CLOCK].getVoltage() > 0 && gateStates[drum]) {
                        gateStates[drum] = false;
                        drumVoltage = 10.f;
                    }
                    if (inputs[INPUT_EXTERNAL_CLOCK].getVoltage() <= 0) {
                        drumVoltage = 0.f;
                    }
                }
            }

            outputs[OUTPUT_TRIGGER_BD + drum].setVoltage(drumVoltage);

            if (drum < 3) {
                lights[LIGHT_CHANNEL_BD + drum].setBrightnessSmooth(drumVoltage, args.sampleTime);
            }
        }

        lights[LIGHT_RESET].setBrightnessSmooth(static_cast<bool>(outputs[OUTPUT_RESET].getVoltage()) ?
            kSanguineButtonLightValue : 0.f, args.sampleTime);

        if (lightsDivider.process()) {
            const float sampleTime = kLightsFrequency * args.sampleTime;

            sequencerMode = static_cast<reticula::PatternGeneratorModes>(params[PARAM_SEQUENCER_MODE].getValue());
            lights[LIGHT_SEQUENCER_MODE + 0].setBrightnessSmooth(sequencerMode <= reticula::PATTERN_HENRI ?
                kSanguineButtonLightValue : 0.f, sampleTime);
            lights[LIGHT_SEQUENCER_MODE + 1].setBrightnessSmooth(sequencerMode >= reticula::PATTERN_HENRI ?
                kSanguineButtonLightValue : 0.f, sampleTime);

            clockOutputSource = static_cast<reticula::ClockOutputSources>(params[PARAM_CLOCK_OUTPUT_SOURCE].getValue());
            lights[LIGHT_CLOCK_OUTPUT_SOURCE + 0].setBrightnessSmooth(clockOutputSource <= reticula::CLOCK_SOURCE_BEAT ?
                kSanguineButtonLightValue : 0.f, sampleTime);
            lights[LIGHT_CLOCK_OUTPUT_SOURCE + 1].setBrightnessSmooth(clockOutputSource >= reticula::CLOCK_SOURCE_BEAT ?
                kSanguineButtonLightValue : 0.f, sampleTime);

            patternGenerator.setPatternMode(sequencerMode);

            lights[LIGHT_RUN].setBrightnessSmooth(bIsModuleRunning ? kSanguineButtonLightValue : 0.f, sampleTime);

            if (!bUseExternalClock) {
                int tempo = static_cast<int>(tempoParam);
                tempoDisplay = std::to_string(tempo);
                if (tempo < 100) {
                    tempoDisplay.insert(0, 1, '0');
                }
            } else {
                tempoDisplay = reticula::externalClockDisplay;
            }
        }
    }

    void onSampleRateChange() override {
        metronome.setSampleRate(APP->engine->getSampleRate());
    }

    void onRandomize(const RandomizeEvent& e) override {
        if (!bUseTapTempo && !bUseExternalClock) {
            params[PARAM_CLOCK].setValue(random::uniform());
        }
        params[PARAM_MAP_X].setValue(random::uniform());
        params[PARAM_MAP_Y].setValue(random::uniform());
        params[PARAM_CHAOS].setValue(random::uniform());
        params[PARAM_BD_DENSITY].setValue(random::uniform());
        params[PARAM_SD_DENSITY].setValue(random::uniform());
        params[PARAM_HH_DENSITY].setValue(random::uniform());
        params[PARAM_SWING].setValue(random::uniform());
    }

    json_t* dataToJson() override {
        json_t* rootJ = SanguineModule::dataToJson();

        setJsonBoolean(rootJ, "IsModuleRunning", bIsModuleRunning);

        setJsonBoolean(rootJ, "UseTapTempo", bUseTapTempo);
        if (bUseTapTempo) {
            setJsonFloat(rootJ, "TapTempoValue", tempoParam);
            setJsonFloat(rootJ, "LastKnobTempo", lastKnobTempo);
        }

        setJsonInt(rootJ, "ExternalClockResolution", extClockResolution);

        setJsonBoolean(rootJ, "WantSequencerResets", bWantSequencerResets);

        setJsonInt(rootJ, "RunMode", runMode);

        setJsonInt(rootJ, "OutputMode", outputMode);

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        SanguineModule::dataFromJson(rootJ);

        json_int_t intValue;

        getJsonBoolean(rootJ, "UseTapTempo", bUseTapTempo);

        if (bUseTapTempo) {
            getJsonFloat(rootJ, "TapTempoValue", tempoParam);
            getJsonFloat(rootJ, "LastKnobTempo", lastKnobTempo);
        }

        if (getJsonInt(rootJ, "ExternalClockResolution", intValue)) {
            extClockResolution = static_cast<reticula::ExternalClockResolutions>(intValue);
        }

        getJsonBoolean(rootJ, "WantSequencerResets", bWantSequencerResets);

        getJsonBoolean(rootJ, "IsModuleRunning", bIsModuleRunning);

        if (getJsonInt(rootJ, "RunMode", intValue)) {
            runMode = static_cast<reticula::GateModes>(intValue);
        }

        if (getJsonInt(rootJ, "OutputMode", intValue)) {
            outputMode = static_cast<reticula::GateModes>(intValue);
        }
    }

    void setSequencerMode(int newMode) {
        params[PARAM_SEQUENCER_MODE].setValue(newMode);
    }

    void setClockOutputSource(int newMode) {
        params[PARAM_CLOCK_OUTPUT_SOURCE].setValue(newMode);
    }
};

struct ReticulaWidget : SanguineModuleWidget {
    explicit ReticulaWidget(Reticula* module) {
        setModule(module);

        moduleName = "reticula";
        panelSize = SIZE_16;
        backplateColor = PLATE_PURPLE;

        makePanel();

        addScrews(SCREW_ALL);

        FramebufferWidget* reticulaFramebuffer = new FramebufferWidget();
        addChild(reticulaFramebuffer);

        addInput(createInput<BananutGreen>(millimetersToPixelsVec(2.107, 9.143), module, Reticula::INPUT_EXTERNAL_CLOCK));

        addParam(createParam<Sanguine2PSRed>(millimetersToPixelsVec(13.833, 12.321), module, Reticula::PARAM_CLOCK));

        addParam(createParam<Sanguine1PBlue>(millimetersToPixelsVec(34.688, 14.874), module, Reticula::PARAM_MAP_X));
        addParam(createParam<Sanguine1PBlue>(millimetersToPixelsVec(51.199, 14.874), module, Reticula::PARAM_MAP_Y));
        addParam(createParam<Sanguine1PGreen>(millimetersToPixelsVec(67.71, 14.874), module, Reticula::PARAM_CHAOS));

        addParam(createLightParam<VCVLightBezel<GreenRedLight>>(millimetersToPixelsVec(2.607, 23.512), module,
            Reticula::PARAM_TAP, Reticula::LIGHT_TAP));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(35.963, 31.452), module, Reticula::INPUT_MAP_X));
        addInput(createInput<BananutPurple>(millimetersToPixelsVec(52.474, 31.452), module, Reticula::INPUT_MAP_Y));
        addInput(createInput<BananutPurple>(millimetersToPixelsVec(68.985, 31.452), module, Reticula::INPUT_CHAOS));

        addParam(createLightParamCentered<VCVLightBezel<RedLight>>(millimetersToPixelsVec(6.107, 49.725), module,
            Reticula::PARAM_RUN, Reticula::LIGHT_RUN));
        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(21.208, 49.725), module, Reticula::INPUT_RUN));

        addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(60.065, 49.725), module, Reticula::INPUT_RESET));
        addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(millimetersToPixelsVec(75.169, 49.725), module,
            Reticula::PARAM_RESET, Reticula::LIGHT_RESET));

        addChild(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(33.421, 49.725),
            module, Reticula::PARAM_SEQUENCER_MODE, Reticula::LIGHT_SEQUENCER_MODE));

        addChild(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(47.757, 49.725),
            module, Reticula::PARAM_CLOCK_OUTPUT_SOURCE, Reticula::LIGHT_CLOCK_OUTPUT_SOURCE));

        addParam(createParam<Sanguine1PRed>(millimetersToPixelsVec(7.176, 60.635), module, Reticula::PARAM_SWING));
        addParam(createParam<Sanguine1PPurple>(millimetersToPixelsVec(25.966, 60.635), module, Reticula::PARAM_BD_DENSITY));
        addParam(createParam<Sanguine1PPurple>(millimetersToPixelsVec(44.756, 60.635), module, Reticula::PARAM_SD_DENSITY));
        addParam(createParam<Sanguine1PPurple>(millimetersToPixelsVec(63.550, 60.635), module, Reticula::PARAM_HH_DENSITY));

        addChild(createLight<MediumLight<RedLight>>(millimetersToPixelsVec(29.703, 79.699), module, Reticula::LIGHT_CHANNEL_BD));
        addChild(createLight<MediumLight<RedLight>>(millimetersToPixelsVec(48.493, 79.699), module, Reticula::LIGHT_CHANNEL_SD));
        addChild(createLight<MediumLight<RedLight>>(millimetersToPixelsVec(67.287, 79.699), module, Reticula::LIGHT_CHANNEL_HH));

        addInput(createInput<BananutPurple>(millimetersToPixelsVec(8.451, 83.340), module, Reticula::INPUT_SWING));
        addInput(createInput<BananutPurple>(millimetersToPixelsVec(27.241, 83.340), module, Reticula::INPUT_FILL_BD));
        addInput(createInput<BananutPurple>(millimetersToPixelsVec(46.031, 83.340), module, Reticula::INPUT_FILL_SD));
        addInput(createInput<BananutPurple>(millimetersToPixelsVec(64.825, 83.340), module, Reticula::INPUT_FILL_HH));

        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(8.451, 99.589), module, Reticula::OUTPUT_CLOCK));
        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(27.241, 99.589), module, Reticula::OUTPUT_TRIGGER_BD));
        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(46.031, 99.589), module, Reticula::OUTPUT_TRIGGER_SD));
        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(64.825, 99.589), module, Reticula::OUTPUT_TRIGGER_HH));

        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(8.451, 112.785), module, Reticula::OUTPUT_RESET));
        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(27.241, 112.785), module, Reticula::OUTPUT_ACCENT_BD));
        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(46.031, 112.785), module, Reticula::OUTPUT_ACCENT_SD));
        addOutput(createOutput<BananutRed>(millimetersToPixelsVec(64.825, 112.785), module, Reticula::OUTPUT_ACCENT_HH));

        SanguineTinyNumericDisplay* displayBPM = new SanguineTinyNumericDisplay(3, module, 21.208, 35.452);
        displayBPM->displayType = DISPLAY_STRING;
        reticulaFramebuffer->addChild(displayBPM);
        displayBPM->fallbackString = "120";

        if (module) {
            displayBPM->values.displayText = &module->tempoDisplay;
        }
    }

    void appendContextMenu(Menu* menu) override {
        SanguineModuleWidget::appendContextMenu(menu);

        Reticula* module = dynamic_cast<Reticula*>(this->module);

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexSubmenuItem("Sequencer mode", reticula::sequencerModeLabels,
            [=]() {return module->sequencerMode; },
            [=](int i) {module->setSequencerMode(i); }
        ));

        menu->addChild(new MenuSeparator);

        menu->addChild(createIndexSubmenuItem("Clock output source", reticula::clockOutputSourceLabels,
            [=]() {return module->clockOutputSource; },
            [=](int i) {module->setClockOutputSource(i); }
        ));

        menu->addChild(new MenuSeparator);

        menu->addChild(createSubmenuItem("Options", "",
            [=](Menu* menu) {
                menu->addChild(createCheckMenuItem("Output sequencer resets", "",
                    [=]() {return module->bWantSequencerResets; },
                    [=]() {module->bWantSequencerResets = !(module->bWantSequencerResets); }));

                menu->addChild(createIndexSubmenuItem("Run mode", reticula::gateModesLabels,
                    [=]() {return module->runMode; },
                    [=](int i) {module->runMode = static_cast<reticula::GateModes>(i); }
                ));

                menu->addChild(createIndexSubmenuItem("Output mode", reticula::gateModesLabels,
                    [=]() {return module->outputMode; },
                    [=](int i) {module->outputMode = static_cast<reticula::GateModes>(i); }
                ));

                menu->addChild(createIndexSubmenuItem("External clock resolution", reticula::clockResolutionLabels,
                    [=]() {return module->extClockResolution; },
                    [=](int i) {module->extClockResolution = static_cast<reticula::ExternalClockResolutions>(i); }
                ));
            }
        ));
    }
};

Model* modelReticula = createModel<Reticula, ReticulaWidget>("Sanguine-Reticula");