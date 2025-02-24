#include "plugin.hpp"
#include "sanguinecomponents.hpp"
#include "sanguinehelpers.hpp"

struct MutantsBlank : SanguineModule {

};

struct MutantsBlankWidget : SanguineModuleWidget {
	explicit MutantsBlankWidget(MutantsBlank* module) {
		setModule(module);

		moduleName = "mutants_blank";
		panelSize = SIZE_10;
		backplateColor = PLATE_PURPLE;
		bHasCommon = false;
		bFaceplateSuffix = false;

		makePanel();

		addScrews(SCREW_ALL);

		SanguineShapedLight* mutantsLight = new SanguineShapedLight(module, "res/mutants_glowy_blank.svg", 25.914, 51.81);
		addChild(mutantsLight);

		SanguineShapedLight* bloodLight = new SanguineShapedLight(module, "res/blood_glowy_blank.svg", 7.907, 114.709);
		addChild(bloodLight);

		SanguineShapedLight* sanguineLogo = new SanguineShapedLight(module, "res/sanguine_lit_blank.svg", 29.204, 113.209);
		addChild(sanguineLogo);
	}
};

Model* modelMutantsBlank = createModel<MutantsBlank, MutantsBlankWidget>("Sanguine-Mutants-Blank");
