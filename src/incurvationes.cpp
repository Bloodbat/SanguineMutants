#include "plugin.hpp"
#include "warps/dsp/modulator.h"
#include "sanguinecomponents.hpp"
#include "warpiespals.hpp"
#include "sanguinehelpers.hpp"

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
	const int kLightFrequency = 128;

	warps::Modulator warpsModulator;
	warps::ShortFrame inputFrames[60] = {};
	warps::ShortFrame outputFrames[60] = {};

	bool bEasterEggEnabled = false;

	dsp::ClockDivider lightDivider;

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

		lightDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		warpsParameters->carrier_shape = params[PARAM_CARRIER].getValue();
		bEasterEggEnabled = params[PARAM_EASTER_EGG].getValue();

		warpsModulator.set_easter_egg(bEasterEggEnabled);

		bool isLightsTurn = lightDivider.process();

		if (isLightsTurn) {
			lights[LIGHT_CARRIER + 0].value = (warpsParameters->carrier_shape == 1 || warpsParameters->carrier_shape == 2) ? 1.0 : 0.0;
			lights[LIGHT_CARRIER + 1].value = (warpsParameters->carrier_shape == 2 || warpsParameters->carrier_shape == 3) ? 1.0 : 0.0;

			lights[LIGHT_EASTER_EGG].setBrightness(bEasterEggEnabled ? 1.f : 0.f);
		}

		float_4 f4Voltages;

		// Buffer loop
		if (++frame >= 60) {
			frame = 0;

			// LEVEL1 and LEVEL2 normalized values from cv_scaler.cc and a PR by Brian Head to AI's repository.
			f4Voltages[0] = inputs[INPUT_LEVEL1].getNormalVoltage(5.f);
			f4Voltages[1] = inputs[INPUT_LEVEL2].getNormalVoltage(5.f);
			f4Voltages[2] = inputs[INPUT_ALGORITHM].getVoltage();
			f4Voltages[3] = inputs[INPUT_TIMBRE].getVoltage();

			f4Voltages /= 5.f;

			warpsParameters->channel_drive[0] = clamp(params[PARAM_LEVEL1].getValue() * f4Voltages[0], 0.f, 1.f);
			warpsParameters->channel_drive[1] = clamp(params[PARAM_LEVEL2].getValue() * f4Voltages[1], 0.f, 1.f);

			float algorithmValue = params[PARAM_ALGORITHM].getValue() / 8.f;

			warpsParameters->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.f, 1.f);

			if (isLightsTurn) {
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
			}

			warpsParameters->modulation_parameter = clamp(params[PARAM_TIMBRE].getValue() + f4Voltages[3], 0.f, 1.f);

			warpsParameters->frequency_shift_pot = algorithmValue;
			/* Compromise: prevent crash when modulating frequency shift and keep negative and possitve offsets.
			   Was warpsParameters->frequency_shift_cv = clamp(f4Voltages[2], -1.f, 1.f); */
			warpsParameters->frequency_shift_cv = clamp(f4Voltages[2], -0.99f, 0.99f);
			warpsParameters->phase_shift = warpsParameters->modulation_algorithm;

			warpsParameters->note = 60.f * params[PARAM_LEVEL1].getValue() + 12.f * inputs[INPUT_LEVEL1].getNormalVoltage(2.f) + 12.f;
			warpsParameters->note += log2f(96000.f * args.sampleTime) * 12.f;

			warpsModulator.Process(inputFrames, outputFrames, 60);
		}

		inputFrames[frame].l = clamp(int(inputs[INPUT_CARRIER].getVoltage() / 16.0 * 0x8000), -0x8000, 0x7fff);
		inputFrames[frame].r = clamp(int(inputs[INPUT_MODULATOR].getVoltage() / 16.0 * 0x8000), -0x8000, 0x7fff);
		outputs[OUTPUT_MODULATOR].setVoltage(float(outputFrames[frame].l) / 0x8000 * 5.0);
		outputs[OUTPUT_AUX].setVoltage(float(outputFrames[frame].r) / 0x8000 * 5.0);
	}
};

struct IncurvationesWidget : ModuleWidget {
	IncurvationesWidget(Incurvationes* module) {
		setModule(module);

		SanguinePanel* panel = new SanguinePanel("res/backplate_10hp_purple.svg", "res/incurvationes_faceplate.svg");
		setPanel(panel);

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(45.963, 12.897),
			module, Incurvationes::PARAM_EASTER_EGG, Incurvationes::LIGHT_EASTER_EGG));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(25.4, 37.486), module, Incurvationes::PARAM_ALGORITHM));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(25.4, 37.486),
			module, Incurvationes::LIGHT_ALGORITHM));
		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(42.388, 79.669), module, Incurvationes::PARAM_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(16.906, 63.862),
			module, Incurvationes::PARAM_CARRIER, Incurvationes::LIGHT_CARRIER));

		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(33.894, 63.862), module, Incurvationes::INPUT_ALGORITHM));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(8.412, 79.451), module, Incurvationes::PARAM_LEVEL1));
		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(25.4, 79.451), module, Incurvationes::PARAM_LEVEL2));

		addInput(createInputCentered<BananutYellow>(millimetersToPixelsVec(8.412, 96.146), module, Incurvationes::INPUT_LEVEL1));
		addInput(createInputCentered<BananutBlue>(millimetersToPixelsVec(25.4, 96.146), module, Incurvationes::INPUT_LEVEL2));
		addInput(createInputCentered<BananutPurple>(millimetersToPixelsVec(42.388, 96.146), module, Incurvationes::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(7.925, 112.172), module, Incurvationes::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreen>(millimetersToPixelsVec(18.777, 112.172), module, Incurvationes::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(32.044, 112.172), module, Incurvationes::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRed>(millimetersToPixelsVec(42.896, 112.172), module, Incurvationes::OUTPUT_AUX));
	}
};

Model* modelIncurvationes = createModel<Incurvationes, IncurvationesWidget>("Sanguine-Incurvationes");
