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

		FramebufferWidget* blankFrameBuffer = new FramebufferWidget();
		addChild(blankFrameBuffer);		

		SanguineShapedLight* mutantsLight = new SanguineShapedLight();
		mutantsLight->box.pos = mm2px(Vec(5.432, 43.423));
		mutantsLight->wrap();
		mutantsLight->module = module;
		mutantsLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/mutants_glowy_blank.svg")));
		blankFrameBuffer->addChild(mutantsLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight();
		bloodLight->box.pos = mm2px(Vec(4.468, 107.571));
		bloodLight->wrap();
		bloodLight->module = module;
		bloodLight->setSvg(Svg::load(asset::plugin(pluginInstance, "res/blood_glowy_blank.svg")));
		blankFrameBuffer->addChild(bloodLight);

		SanguineShapedLight* sanguineLogo = new SanguineShapedLight();
		sanguineLogo->box.pos = mm2px(Vec(11.597, 104.861));
		sanguineLogo->wrap();
		sanguineLogo->module = module;
		sanguineLogo->setSvg(Svg::load(asset::plugin(pluginInstance, "res/sanguine_lit_blank.svg")));
		blankFrameBuffer->addChild(sanguineLogo);
	}
};

Model* modelMutantsBlank = createModel<MutantsBlank, MutantsBlankWidget>("Sanguine-Mutants-Blank");
