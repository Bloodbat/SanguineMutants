#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

#include "sanguinejson.hpp"

#include "undae.hpp"

using namespace sanguineCommonCode;

struct Undae : SanguineModule {

    enum ParamIds {
        PARAM_RESONANCE,
        PARAM_FREQUENCY,
        PARAM_FM,
        PARAMS_COUNT
    };

    enum InputIds {
        INPUT_RESONANCE,
        INPUT_FREQUENCY,
        INPUT_FM,
        INPUT_GAIN,
        INPUT_IN,
        INPUTS_COUNT
    };

    enum OutputIds {
        OUTPUT_BP2,
        OUTPUT_LP2,
        OUTPUT_LP4,
        OUTPUT_LP4VCA,
        OUTPUTS_COUNT
    };

    enum LightIds {
        LIGHTS_COUNT
    };

    undae::UndaeEngine engines[16];

    undae::UndaeEngine::Frame frame;

    int channelCount = 1;

    Undae() {
        config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

        configParam(PARAM_RESONANCE, 0.f, 1.f, 0.f, "Resonance", "%", 0, 100);
        configParam(PARAM_FREQUENCY, std::log2(undae::kFreqKnobMin),
            std::log2(undae::kFreqKnobMax), std::log2(undae::kFreqKnobMax),
            "Frequency", " Hz", 2.f);
        configParam(PARAM_FM, -1.f, 1.f, 0.f, "Frequency modulation", "%", 0, 100);

        configInput(INPUT_RESONANCE, "Resonance");
        configInput(INPUT_FREQUENCY, "Frequency");
        configInput(INPUT_FM, "FM");
        configInput(INPUT_IN, "Signal");
        configInput(INPUT_GAIN, "VCA Gain");

        configOutput(OUTPUT_BP2, "Band-pass 2-pole (12 dB/oct)");
        configOutput(OUTPUT_LP2, "Low-pass 2-pole (12 dB/oct)");
        configOutput(OUTPUT_LP4, "Low-pass 4-pole (24 dB/oct)");
        configOutput(OUTPUT_LP4VCA, "Low-pass 4-pole (24 dB/oct) VCA");

        configBypass(INPUT_IN, OUTPUT_BP2);
        configBypass(INPUT_IN, OUTPUT_LP2);
        configBypass(INPUT_IN, OUTPUT_LP4);
        configBypass(INPUT_IN, OUTPUT_LP4VCA);

        frame.bGainConnected = false;
        frame.bBp2Connected = false;
        frame.bLp2Connected = false;
        frame.bLp4Connected = false;
        frame.bLp4VcaConnected = false;
    }

    void process(const ProcessArgs& args) override {
        channelCount = std::max(inputs[INPUT_IN].getChannels(), 1);

        frame.knobResonance = params[PARAM_RESONANCE].getValue();
        frame.knobFrequency = rescale(params[PARAM_FREQUENCY].getValue(), std::log2(undae::kFreqKnobMin),
            std::log2(undae::kFreqKnobMax), 0.f, 1.f);
        frame.knobFm = params[PARAM_FM].getValue();

        outputs[OUTPUT_BP2].setChannels(channelCount);
        outputs[OUTPUT_LP2].setChannels(channelCount);
        outputs[OUTPUT_LP4].setChannels(channelCount);
        outputs[OUTPUT_LP4VCA].setChannels(channelCount);

        for (int channel = 0; channel < channelCount; ++channel) {
            frame.cvResonance = inputs[INPUT_RESONANCE].getPolyVoltage(channel);
            frame.cvFrequency = inputs[INPUT_FREQUENCY].getPolyVoltage(channel);
            frame.cvFm = inputs[INPUT_FM].getPolyVoltage(channel);
            frame.input = inputs[INPUT_IN].getVoltage(channel);
            frame.cvGain = inputs[INPUT_GAIN].getPolyVoltage(channel);

            engines[channel].process(frame);

            outputs[OUTPUT_BP2].setVoltage(frame.bp2, channel);
            outputs[OUTPUT_LP2].setVoltage(frame.lp2, channel);
            outputs[OUTPUT_LP4].setVoltage(frame.lp4, channel);
            outputs[OUTPUT_LP4VCA].setVoltage(frame.lp4vca, channel);
        }
    }

    void onSampleRateChange(const SampleRateChangeEvent& e) override {
        for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
            engines[channel].setSampleRate(e.sampleRate);
        }
    }

    void onPortChange(const PortChangeEvent& e) override {
        switch (e.type) {
        case Port::INPUT:
            if (e.portId == INPUT_GAIN) {
                frame.bGainConnected = e.connecting;
            }
            break;

        case Port::OUTPUT:
            switch (e.portId) {
            case OUTPUT_BP2:
                frame.bBp2Connected = e.connecting;
                break;

            case OUTPUT_LP2:
                frame.bLp2Connected = e.connecting;
                break;

            case OUTPUT_LP4:
                frame.bLp4Connected = e.connecting;
                break;

            case OUTPUT_LP4VCA:
                frame.bLp4VcaConnected = e.connecting;
                break;
            }
            break;
        }
    }
};

struct UndaeWidget : SanguineModuleWidget {
    explicit UndaeWidget(Undae* module) {
        setModule(module);

        moduleName = "undae";
        panelSize = sanguineThemes::SIZE_8;
        backplateColor = sanguineThemes::PLATE_PURPLE;

        makePanel();

        addScrews(SCREW_TOP_LEFT | SCREW_BOTTOM_LEFT);

        addParam(createParam<Sanguine2PSBlue>(millimetersToPixelsVec(3.955, 18.912), module, Undae::PARAM_RESONANCE));

        addParam(createParam<Sanguine3PSRed>(millimetersToPixelsVec(11.570, 39.475), module, Undae::PARAM_FREQUENCY));

        addParam(createParam<Sanguine2PSPurple>(millimetersToPixelsVec(21.941, 62.644), module, Undae::PARAM_FM));

        addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(2.881, 86.882), module, Undae::INPUT_RESONANCE));
        addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(16.323, 86.882), module, Undae::INPUT_FREQUENCY));
        addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(29.764, 86.882), module, Undae::INPUT_FM));

        addInput(createInput<BananutPurplePoly>(millimetersToPixelsVec(2.881, 99.677), module, Undae::INPUT_GAIN));
        addOutput(createOutput<BananutRedPoly>(millimetersToPixelsVec(16.323, 99.677), module, Undae::OUTPUT_BP2));
        addOutput(createOutput<BananutRedPoly>(millimetersToPixelsVec(29.764, 99.677), module, Undae::OUTPUT_LP2));

        addInput(createInput<BananutGreenPoly>(millimetersToPixelsVec(2.881, 112.473), module, Undae::INPUT_IN));
        addOutput(createOutput<BananutRedPoly>(millimetersToPixelsVec(16.323, 112.473), module, Undae::OUTPUT_LP4));
        addOutput(createOutput<BananutRedPoly>(millimetersToPixelsVec(29.764, 112.473), module, Undae::OUTPUT_LP4VCA));
    }
};

Model* modelUndae = createModel<Undae, UndaeWidget>("Sanguine-Undae");