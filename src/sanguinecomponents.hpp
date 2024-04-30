using namespace rack;

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

struct BananutBlack : app::SvgPort {
	BananutBlack() {
		setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/BananutBlack.svg")));
	}
};