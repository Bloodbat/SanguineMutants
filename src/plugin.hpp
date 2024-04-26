#include <rack.hpp>

using namespace rack;

extern Plugin* pluginInstance;

extern Model* modelFunes;

struct BananutRed : app::SvgPort {
	BananutRed() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutRed.svg")));
	}
};

struct BananutGreen : app::SvgPort {
	BananutGreen() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutGreen.svg")));
	}
};

struct BananutPurple : app::SvgPort {
	BananutPurple() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutPurple.svg")));
	}
};

/*template <typename Base>
struct Rogan6PSLight : Base {
	Rogan6PSLight() {
		this->box.size = mm2px(Vec(23.04, 23.04));
		// this->bgColor = nvgRGBA(0, 0, 0, 0);
		this->borderColor = nvgRGBA(0, 0, 0, 0);
	}
};*/
