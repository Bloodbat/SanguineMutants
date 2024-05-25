#include "plugin.hpp"
#include "sanguinecomponents.hpp"

struct MutantsBlank : Module {

};

struct MutantsBlankWidget : ModuleWidget {
	MutantsBlankWidget(MutantsBlank* module) {
		setModule(module);
		setPanel(Svg::load(asset::plugin(pluginInstance, "res/mutants_blank.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		SanguineShapedLight* mutantsLight = new SanguineShapedLight();
		mutantsLight->box.pos = mm2px(Vec(5.432, 43.423));
		mutantsLight->module = module;
		mutantsLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/mutants_glowy_blank.svg")));
		addChild(mutantsLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = mm2px(Vec(4.468, 107.571));
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy_blank.svg")));
		addChild(bloodLight);

		SanguineShapedLight* sanguineLogo = new SanguineShapedLight();
		sanguineLogo->box.pos = mm2px(Vec(11.597, 104.861));
		sanguineLogo->module = module;
		sanguineLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/sanguine_lit_blank.svg")));
		addChild(sanguineLogo);
	}
};

Model* modelMutantsBlank = createModel<MutantsBlank, MutantsBlankWidget>("Sanguine-Mutants-Blank");
