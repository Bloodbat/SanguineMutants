#include "plugin.hpp"
#include "warps/dsp/modulator.h"
#include "sanguinecomponents.hpp"
#include "warpiespals.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Incurvationes : Module {
	enum ParamIds {
		PARAM_ALGORITHM,
		PARAM_TIMBRE,
		PARAM_CARRIER,
		PARAM_LEVEL1,
		PARAM_LEVEL2,
		PARAM_EASTER_EGG,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_LEVEL1,
		INPUT_LEVEL2,
		INPUT_ALGORITHM,
		INPUT_TIMBRE,
		INPUT_CARRIER,
		INPUT_MODULATOR,
		INPUTS_COUNT
	};
	enum OutputIds {
		OUTPUT_MODULATOR,
		OUTPUT_AUX,
		OUTPUTS_COUNT
	};
	enum LightIds {
		ENUMS(LIGHT_CARRIER, 2),
		ENUMS(LIGHT_ALGORITHM, 3),
		LIGHT_EASTER_EGG,
		LIGHTS_COUNT
	};


	int frame = 0;
	warps::Modulator warpsModulator;
	warps::ShortFrame inputFrames[60] = {};
	warps::ShortFrame outputFrames[60] = {};	

	bool bEasterEggEnabled = false;

	warps::Parameters* warpsParameters = warpsModulator.mutable_parameters();

	Incurvationes() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configButton(PARAM_EASTER_EGG, "Frequency shifter (Easter egg)");

		configParam(PARAM_ALGORITHM, 0.0, 8.0, 0.0, "Algorithm");

		configParam(PARAM_CARRIER, 0.f, 3.f, 0.f, "Internal oscillator mode");

		configParam(PARAM_TIMBRE, 0.0, 1.0, 0.5, "Timbre", "%", 0, 100);

		configParam(PARAM_LEVEL1, 0.0, 1.0, 1.0, "External oscillator amplitude / internal oscillator frequency", "%", 0, 100);
		configParam(PARAM_LEVEL2, 0.0, 1.0, 1.0, "Modulator amplitude", "%", 0, 100);

		configInput(INPUT_LEVEL1, "Level 1");
		configInput(INPUT_LEVEL2, "Level 2");
		configInput(INPUT_ALGORITHM, "Algorithm");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_CARRIER, "Carrier");
		configInput(INPUT_MODULATOR, "Modulator");

		configOutput(OUTPUT_MODULATOR, "Modulator");
		configOutput(OUTPUT_AUX, "Auxiliary");

		configBypass(INPUT_MODULATOR, OUTPUT_MODULATOR);

		memset(&warpsModulator, 0, sizeof(warpsModulator));
		warpsModulator.Init(96000.0f);
	}

	void process(const ProcessArgs& args) override {		
		warpsParameters->carrier_shape = params[PARAM_CARRIER].getValue();
		bEasterEggEnabled = params[PARAM_EASTER_EGG].getValue();

		warpsModulator.set_easter_egg(bEasterEggEnabled);

		lights[LIGHT_CARRIER + 0].value = (warpsParameters->carrier_shape == 1 || warpsParameters->carrier_shape == 2) ? 1.0 : 0.0;
		lights[LIGHT_CARRIER + 1].value = (warpsParameters->carrier_shape == 2 || warpsParameters->carrier_shape == 3) ? 1.0 : 0.0;

		lights[LIGHT_EASTER_EGG].setBrightness(bEasterEggEnabled ? 1.f : 0.f);

		// Buffer loop
		if (++frame >= 60) {
			frame = 0;

			warpsParameters->channel_drive[0] = clamp(params[PARAM_LEVEL1].getValue() + inputs[INPUT_LEVEL1].getVoltage() / 5.0f, 0.0f, 1.0f);
			warpsParameters->channel_drive[1] = clamp(params[PARAM_LEVEL2].getValue() + inputs[INPUT_LEVEL2].getVoltage() / 5.0f, 0.0f, 1.0f);

			float algorithmValue = params[PARAM_ALGORITHM].getValue() / 8.0f;
			float algorithmCv = inputs[INPUT_ALGORITHM].getVoltage() / 5.0f;

			warpsParameters->modulation_algorithm = clamp(algorithmValue + algorithmCv, 0.0f, 1.0f);

			const uint8_t(*palette)[3];
			float zone;
			if (!bEasterEggEnabled) {
				zone = 8.0f * warpsParameters->modulation_algorithm;
				palette = paletteWarpsDefault;
			}
			else {
				zone = 8.0f * warpsParameters->phase_shift;
				palette = paletteWarpsFreqsShift;
			}

			MAKE_INTEGRAL_FRACTIONAL(zone);
			int zone_fractional_i = static_cast<int>(zone_fractional * 256);
			for (int i = 0; i < 3; i++) {
				int a = palette[zone_integral][i];
				int b = palette[zone_integral + 1][i];
				lights[LIGHT_ALGORITHM + i].setBrightness(static_cast<float>(a + ((b - a) * zone_fractional_i >> 8)) / 255.0f);
			}

			warpsParameters->modulation_parameter = clamp(params[PARAM_TIMBRE].getValue() + inputs[INPUT_TIMBRE].getVoltage() / 5.0f, 0.0f, 1.0f);

			warpsParameters->frequency_shift_pot = algorithmValue;
			warpsParameters->frequency_shift_cv = clamp(algorithmCv, -1.0f, 1.0f);
			warpsParameters->phase_shift = warpsParameters->modulation_algorithm;
			warpsParameters->note = 60.0 * params[PARAM_LEVEL1].getValue() + 12.0 * inputs[INPUT_LEVEL1].getNormalVoltage(2.0) + 12.0;
			warpsParameters->note += log2f(96000.0f * args.sampleTime) * 12.0f;

			warpsModulator.Process(inputFrames, outputFrames, 60);
		}

		inputFrames[frame].l = clamp((int)(inputs[INPUT_CARRIER].getVoltage() / 16.0 * 0x8000), -0x8000, 0x7fff);
		inputFrames[frame].r = clamp((int)(inputs[INPUT_MODULATOR].getVoltage() / 16.0 * 0x8000), -0x8000, 0x7fff);
		outputs[OUTPUT_MODULATOR].setVoltage((float)outputFrames[frame].l / 0x8000 * 5.0);
		outputs[OUTPUT_AUX].setVoltage((float)outputFrames[frame].r / 0x8000 * 5.0);
	}
};


struct IncurvationesWidget : ModuleWidget {
	IncurvationesWidget(Incurvationes* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/incurvationes_faceplate.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(mm2px(Vec(45.963, 12.897)), module,
			Incurvationes::PARAM_EASTER_EGG, Incurvationes::LIGHT_EASTER_EGG));

		addParam(createParamCentered<Rogan6PSWhite>(mm2px(Vec(25.4, 37.486)), module, Incurvationes::PARAM_ALGORITHM));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(mm2px(Vec(25.4, 37.486)), module, Incurvationes::LIGHT_ALGORITHM));
		addParam(createParamCentered<Rogan1PWhite>(mm2px(Vec(42.388, 79.669)), module, Incurvationes::PARAM_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(mm2px(Vec(16.906, 63.862)), module,
			Incurvationes::PARAM_CARRIER, Incurvationes::LIGHT_CARRIER));

		addInput(createInputCentered<BananutPurple>(mm2px(Vec(33.894, 63.862)), module, Incurvationes::INPUT_ALGORITHM));

		addParam(createParamCentered<Sanguine1PYellow>(mm2px(Vec(8.412, 79.451)), module, Incurvationes::PARAM_LEVEL1));
		addParam(createParamCentered<Sanguine1PBlue>(mm2px(Vec(25.4, 79.451)), module, Incurvationes::PARAM_LEVEL2));

		addInput(createInputCentered<BananutYellow>(mm2px(Vec(8.412, 96.146)), module, Incurvationes::INPUT_LEVEL1));
		addInput(createInputCentered<BananutBlue>(mm2px(Vec(25.4, 96.146)), module, Incurvationes::INPUT_LEVEL2));
		addInput(createInputCentered<BananutPurple>(mm2px(Vec(42.388, 96.146)), module, Incurvationes::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreen>(mm2px(Vec(7.925, 112.172)), module, Incurvationes::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreen>(mm2px(Vec(18.777, 112.172)), module, Incurvationes::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(32.044, 112.172)), module, Incurvationes::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(mm2px(Vec(42.896, 112.172)), module, Incurvationes::OUTPUT_AUX));
	}
};


Model* modelIncurvationes = createModel<Incurvationes, IncurvationesWidget>("Sanguine-Incurvationes");
