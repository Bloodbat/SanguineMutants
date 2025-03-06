#include "plugin.hpp"
#include "warps/dsp/modulator.h"
#include "sanguinecomponents.hpp"
#include "warpiespals.hpp"
#include "sanguinehelpers.hpp"
#include "warpiescommon.hpp"

#pragma GCC diagnostic ignored "-Wclass-memaccess"

struct Incurvationes : SanguineModule {
	enum ParamIds {
		PARAM_ALGORITHM,
		PARAM_TIMBRE,
		PARAM_CARRIER,
		PARAM_LEVEL_1,
		PARAM_LEVEL_2,
		PARAM_EASTER_EGG,
		PARAMS_COUNT
	};
	enum InputIds {
		INPUT_LEVEL_1,
		INPUT_LEVEL_2,
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
		ENUMS(LIGHT_CHANNEL_ALGORITHM, PORT_MAX_CHANNELS * 3),
		LIGHTS_COUNT
	};


	int frame[PORT_MAX_CHANNELS] = {};
	static const int kLightFrequency = 128;

	warps::Modulator warpsModulator[PORT_MAX_CHANNELS];
	warps::ShortFrame inputFrames[PORT_MAX_CHANNELS][kWarpsBlockSize] = {};
	warps::ShortFrame outputFrames[PORT_MAX_CHANNELS][kWarpsBlockSize] = {};

	bool bEasterEggEnabled = false;

	dsp::ClockDivider lightsDivider;

	warps::Parameters* warpsParameters[PORT_MAX_CHANNELS];

	Incurvationes() {
		config(PARAMS_COUNT, INPUTS_COUNT, OUTPUTS_COUNT, LIGHTS_COUNT);

		configButton(PARAM_EASTER_EGG, "Frequency shifter (Easter egg)");

		configParam(PARAM_ALGORITHM, 0.f, 8.f, 0.f, "Algorithm");

		configParam(PARAM_CARRIER, 0.f, 3.f, 0.f, "Internal oscillator mode");

		configParam(PARAM_TIMBRE, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);

		configParam(PARAM_LEVEL_1, 0.f, 1.f, 1.f, "External oscillator amplitude / internal oscillator frequency", "%", 0.f, 100.f);
		configParam(PARAM_LEVEL_2, 0.f, 1.f, 1.f, "Modulator amplitude", "%", 0.f, 100.f);

		configInput(INPUT_LEVEL_1, "Level 1");
		configInput(INPUT_LEVEL_2, "Level 2");
		configInput(INPUT_ALGORITHM, "Algorithm");
		configInput(INPUT_TIMBRE, "Timbre");
		configInput(INPUT_CARRIER, "Carrier");
		configInput(INPUT_MODULATOR, "Modulator");

		configOutput(OUTPUT_MODULATOR, "Modulator");
		configOutput(OUTPUT_AUX, "Auxiliary");

		configBypass(INPUT_MODULATOR, OUTPUT_MODULATOR);

		for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
			memset(&warpsModulator[channel], 0, sizeof(warps::Modulator));
			warpsModulator[channel].Init(96000.f);
			warpsParameters[channel] = warpsModulator[channel].mutable_parameters();
		}

		lightsDivider.setDivision(kLightFrequency);
	}

	void process(const ProcessArgs& args) override {
		using simd::float_4;

		int channelCount = std::max(std::max(inputs[INPUT_CARRIER].getChannels(), inputs[INPUT_MODULATOR].getChannels()), 1);

		bEasterEggEnabled = params[PARAM_EASTER_EGG].getValue();

		float algorithmValue = params[PARAM_ALGORITHM].getValue() / 8.f;

		for (int channel = 0; channel < channelCount; ++channel) {

			warpsParameters[channel]->carrier_shape = params[PARAM_CARRIER].getValue();

			warpsModulator[channel].set_easter_egg(bEasterEggEnabled);

			float_4 f4Voltages;

			// Buffer loop
			if (++frame[channel] >= kWarpsBlockSize) {
				frame[channel] = 0;

				// LEVEL1 and LEVEL2 normalized values from cv_scaler.cc and a PR by Brian Head to AI's repository.
				f4Voltages[0] = inputs[INPUT_LEVEL_1].getNormalVoltage(5.f, channel);
				f4Voltages[1] = inputs[INPUT_LEVEL_2].getNormalVoltage(5.f, channel);
				f4Voltages[2] = inputs[INPUT_ALGORITHM].getVoltage(channel);
				f4Voltages[3] = inputs[INPUT_TIMBRE].getVoltage(channel);

				f4Voltages /= 5.f;

				warpsParameters[channel]->channel_drive[0] = clamp(params[PARAM_LEVEL_1].getValue() * f4Voltages[0], 0.f, 1.f);
				warpsParameters[channel]->channel_drive[1] = clamp(params[PARAM_LEVEL_2].getValue() * f4Voltages[1], 0.f, 1.f);

				warpsParameters[channel]->modulation_algorithm = clamp(algorithmValue + f4Voltages[2], 0.f, 1.f);

				warpsParameters[channel]->modulation_parameter = clamp(params[PARAM_TIMBRE].getValue() + f4Voltages[3], 0.f, 1.f);

				warpsParameters[channel]->frequency_shift_pot = algorithmValue;
				warpsParameters[channel]->frequency_shift_cv = clamp(f4Voltages[2], -1.f, 1.f);
				warpsParameters[channel]->phase_shift = warpsParameters[channel]->modulation_algorithm;

				warpsParameters[channel]->note = 60.f * params[PARAM_LEVEL_1].getValue() + 12.f *
					inputs[INPUT_LEVEL_1].getNormalVoltage(2.f, channel) + 12.f;
				warpsParameters[channel]->note += log2f(96000.f * args.sampleTime) * 12.f;

				warpsModulator[channel].Process(inputFrames[channel], outputFrames[channel], kWarpsBlockSize);
			}

			inputFrames[channel][frame[channel]].l = clamp(static_cast<int>(inputs[INPUT_CARRIER].getVoltage(channel) / 8.f * 32768), -32768, 32767);
			inputFrames[channel][frame[channel]].r = clamp(static_cast<int>(inputs[INPUT_MODULATOR].getVoltage(channel) / 8.f * 32768), -32768, 32767);

			outputs[OUTPUT_MODULATOR].setVoltage(static_cast<float>(outputFrames[channel][frame[channel]].l) / 32768 * 5.f, channel);
			outputs[OUTPUT_AUX].setVoltage(static_cast<float>(outputFrames[channel][frame[channel]].r) / 32768 * 5.f, channel);
		}

		outputs[OUTPUT_MODULATOR].setChannels(channelCount);
		outputs[OUTPUT_AUX].setChannels(channelCount);

		if (lightsDivider.process()) {
			lights[LIGHT_CARRIER + 0].value = (warpsParameters[0]->carrier_shape == 1
				|| warpsParameters[0]->carrier_shape == 2) ? kSanguineButtonLightValue : 0.f;
			lights[LIGHT_CARRIER + 1].value = (warpsParameters[0]->carrier_shape == 2
				|| warpsParameters[0]->carrier_shape == 3) ? kSanguineButtonLightValue : 0.f;

			lights[LIGHT_EASTER_EGG].setBrightness(bEasterEggEnabled ? kSanguineButtonLightValue : 0.f);

			const uint8_t(*palette)[3];

			palette = bEasterEggEnabled ? paletteWarpsFreqsShift : paletteWarpsDefault;
			float colorValues[PORT_MAX_CHANNELS][3] = {};

			for (int channel = 0; channel < PORT_MAX_CHANNELS; ++channel) {
				const int currentLight = LIGHT_CHANNEL_ALGORITHM + channel * 3;

				for (int rgbComponent = 0; rgbComponent < 3; ++rgbComponent) {
					lights[currentLight + rgbComponent].setBrightness(0.f);
				}

				if (channel < channelCount) {
					float zone = 8.f * (bEasterEggEnabled ? warpsParameters[channel]->phase_shift : warpsParameters[channel]->modulation_algorithm);

					MAKE_INTEGRAL_FRACTIONAL(zone);
					int zone_fractional_i = static_cast<int>(zone_fractional * 256);
					for (int rgbComponent = 0; rgbComponent < 3; ++rgbComponent) {
						int a = palette[zone_integral][rgbComponent];
						int b = palette[zone_integral + 1][rgbComponent];

						colorValues[channel][rgbComponent] = static_cast<float>(a + ((b - a) * zone_fractional_i >> 8)) / 255.f;

						lights[currentLight + rgbComponent].setBrightness(colorValues[channel][rgbComponent]);
					}
				}
			}

			lights[LIGHT_ALGORITHM + 0].setBrightness(colorValues[0][0]);
			lights[LIGHT_ALGORITHM + 1].setBrightness(colorValues[0][1]);
			lights[LIGHT_ALGORITHM + 2].setBrightness(colorValues[0][2]);
		}
	}
};

struct IncurvationesWidget : SanguineModuleWidget {
	explicit IncurvationesWidget(Incurvationes* module) {
		setModule(module);

		moduleName = "incurvationes";
		panelSize = SIZE_10;
		backplateColor = PLATE_PURPLE;

		makePanel();

		addScrews(SCREW_ALL);

		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<RedLight>>>(millimetersToPixelsVec(45.963, 12.897),
			module, Incurvationes::PARAM_EASTER_EGG, Incurvationes::LIGHT_EASTER_EGG));

		addParam(createParamCentered<Rogan6PSWhite>(millimetersToPixelsVec(25.4, 37.486), module, Incurvationes::PARAM_ALGORITHM));
		addChild(createLightCentered<Rogan6PSLight<RedGreenBlueLight>>(millimetersToPixelsVec(25.4, 37.486),
			module, Incurvationes::LIGHT_ALGORITHM));
		addParam(createParamCentered<Rogan1PWhite>(millimetersToPixelsVec(42.388, 79.669), module, Incurvationes::PARAM_TIMBRE));
		addParam(createLightParamCentered<VCVLightLatch<MediumSimpleLight<GreenRedLight>>>(millimetersToPixelsVec(25.4, 63.862),
			module, Incurvationes::PARAM_CARRIER, Incurvationes::LIGHT_CARRIER));

		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(42.388, 63.862), module, Incurvationes::INPUT_ALGORITHM));

		addParam(createParamCentered<Sanguine1PYellow>(millimetersToPixelsVec(8.412, 79.451), module, Incurvationes::PARAM_LEVEL_1));
		addParam(createParamCentered<Sanguine1PBlue>(millimetersToPixelsVec(25.4, 79.451), module, Incurvationes::PARAM_LEVEL_2));

		addInput(createInputCentered<BananutYellowPoly>(millimetersToPixelsVec(8.412, 96.146), module, Incurvationes::INPUT_LEVEL_1));
		addInput(createInputCentered<BananutBluePoly>(millimetersToPixelsVec(25.4, 96.146), module, Incurvationes::INPUT_LEVEL_2));
		addInput(createInputCentered<BananutPurplePoly>(millimetersToPixelsVec(42.388, 96.146), module, Incurvationes::INPUT_TIMBRE));

		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(7.925, 112.172), module, Incurvationes::INPUT_CARRIER));
		addInput(createInputCentered<BananutGreenPoly>(millimetersToPixelsVec(18.777, 112.172), module, Incurvationes::INPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(32.044, 112.172), module, Incurvationes::OUTPUT_MODULATOR));
		addOutput(createOutputCentered<BananutRedPoly>(millimetersToPixelsVec(42.896, 112.172), module, Incurvationes::OUTPUT_AUX));

		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(14.281, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 0 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.398, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 1 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(18.516, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 2 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.633, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 3 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(30.148, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 4 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(32.265, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 5 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(34.382, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 6 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(36.5, 62.532), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 7 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(14.281, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 8 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(16.398, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 9 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(18.516, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 10 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(20.633, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 11 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(30.148, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 12 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(32.265, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 13 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(34.382, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 14 * 3));
		addChild(createLightCentered<TinyLight<RedGreenBlueLight>>(millimetersToPixelsVec(36.5, 65.191), module, Incurvationes::LIGHT_CHANNEL_ALGORITHM + 15 * 3));
	}
};

Model* modelIncurvationes = createModel<Incurvationes, IncurvationesWidget>("Sanguine-Incurvationes");
