#pragma once

#include "rack.hpp"
#include <color.hpp>
#include "sanguinehelpers.hpp"
#include "themes.hpp"
#include "sanguinedrawing.hpp"

using namespace rack;

enum HaloTypes { HALO_CIRCULAR, HALO_RECTANGULAR, HALO_NONE };

// Light colors.

struct RGBLightColor {
	float red;
	float green;
	float blue;
};

// Light values
static const float kSanguineButtonLightValue = 0.75f;

// Color constants for decorative lights
static const unsigned int kSanguineBlueLight = rgbColorToInt(0, 167, 255);
static const unsigned int kSanguineYellowLight = rgbColorToInt(239, 250, 100);

// Ports

struct BananutBlack : app::SvgPort {
	BananutBlack();
};

struct BananutBlackPoly : app::SvgPort {
	BananutBlackPoly();
};

struct BananutBlue : app::SvgPort {
	BananutBlue();
};

struct BananutBluePoly : app::SvgPort {
	BananutBluePoly();
};

struct BananutGreen : app::SvgPort {
	BananutGreen();
};

struct BananutGreenPoly : app::SvgPort {
	BananutGreenPoly();
};

struct BananutPurple : app::SvgPort {
	BananutPurple();
};

struct BananutPurplePoly : app::SvgPort {
	BananutPurplePoly();
};

struct BananutRed : app::SvgPort {
	BananutRed();
};

struct BananutRedPoly : app::SvgPort {
	BananutRedPoly();
};

struct BananutYellow : app::SvgPort {
	BananutYellow();
};

struct BananutYellowPoly : app::SvgPort {
	BananutYellowPoly();
};

// Knobs

struct BefacoTinyKnobRed : BefacoTinyKnob {
	BefacoTinyKnobRed();
};

struct BefacoTinyKnobBlack : BefacoTinyKnob {
	BefacoTinyKnobBlack();
};

struct Sanguine1PBlue : Rogan {
	Sanguine1PBlue();
};

struct Sanguine1PGrayCap : Rogan {
	Sanguine1PGrayCap();
};

struct Sanguine1PGreen : Rogan {
	Sanguine1PGreen();
};

struct Sanguine1PPurple : Rogan {
	Sanguine1PPurple();
};

struct Sanguine1PRed : Rogan {
	Sanguine1PRed();
};

struct Sanguine1PYellow : Rogan {
	Sanguine1PYellow();
};

struct Sanguine1PSBlue : Rogan {
	Sanguine1PSBlue();
};

struct Sanguine1PSGreen : Rogan {
	Sanguine1PSGreen();
};

struct Sanguine1PSOrange : Rogan {
	Sanguine1PSOrange();
};

struct Sanguine1PSPurple : Rogan {
	Sanguine1PSPurple();
};

struct Sanguine1PSRed : Rogan {
	Sanguine1PSRed();
};

struct Sanguine1PSYellow : Rogan {
	Sanguine1PSYellow();
};

struct Sanguine1SGray : Rogan {
	Sanguine1SGray();
};

struct Sanguine2PSBlue : Rogan {
	Sanguine2PSBlue();
};

struct Sanguine2PSRed : Rogan {
	Sanguine2PSRed();
};

struct Sanguine3PSGreen : Rogan {
	Sanguine3PSGreen();
};

struct Sanguine3PSRed : Rogan {
	Sanguine3PSRed();
};

// Switches

struct SanguineBlackSwitch : app::SvgSwitch {
	SanguineBlackSwitch();
};

struct SanguineBezel115 : app::SvgSwitch {
	SanguineBezel115();
};

// Displays

enum DisplayType {
	DISPLAY_NUMERIC,
	DISPLAY_STRING
};

struct SanguineBaseSegmentDisplay : TransparentWidget {
	Module* module;
	NVGcolor backgroundColor = nvgRGB(0x38, 0x38, 0x38);
	NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
	NVGcolor textColor = nvgRGB(200, 0, 0);
	uint32_t characterCount;
	union Values {
		std::string* displayText = nullptr;
		int* numberValue;
	} values;

	float fontSize;
	unsigned char haloOpacity = 55;
	unsigned char backgroundCharAlpha = 16;
	DisplayType displayType = DISPLAY_STRING;
	std::string backgroundCharacter = " ";
	std::string fallbackString = "";
	std::string fontName = "";
	math::Vec textMargin = { 2.f, 2.f };
	float kerning = 2.f;
	bool leadingZero = true;
	bool drawHalo = true;
	int fallbackNumber = 0;

	SanguineBaseSegmentDisplay(uint32_t newCharacterCount, Module* theModule);
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineAlphaDisplay : SanguineBaseSegmentDisplay {
	SanguineAlphaDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineLedNumberDisplay : SanguineBaseSegmentDisplay {
	SanguineLedNumberDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMatrixDisplay : SanguineBaseSegmentDisplay {
	SanguineMatrixDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineTinyNumericDisplay : SanguineLedNumberDisplay {
	SanguineTinyNumericDisplay(uint32_t newCharacterCount, Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct Sanguine96x32OLEDDisplay : TransparentWidget {
	Module* module;
	std::string* oledText = nullptr;
	std::string fallbackString = "";
	std::string fontName = "";
	NVGcolor textColor = nvgRGB(254, 254, 254);
	Sanguine96x32OLEDDisplay(Module* theModule, const float X, const float Y, bool createCentered = true);
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

// Switches

struct SanguineLightUpSwitch : app::SvgSwitch {
	std::vector<NVGcolor> halos;
	SanguineLightUpSwitch();
	void addHalo(NVGcolor haloColor);
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineLightUpRGBSwitch : app::SvgSwitch {
	std::vector<unsigned int>colors;
	std::vector<NVGcolor> halos;
	SvgWidget* glyph;
	TransformWidget* transformWidget;
	SanguineLightUpRGBSwitch();
	void addColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);
	void addColor(unsigned int color);
	void addHalo(NVGcolor haloColor);
	void drawLayer(const DrawArgs& args, int layer) override;
	void setBackground(const std::string& fileName);
	void setGlyph(const std::string& fileName, const float offsetX, const float offsetY);
};

struct Befaco2StepSwitch : app::SvgSwitch {
	Befaco2StepSwitch();
};

// Lights
struct SanguineMultiColoredShapedLight : SvgWidget {
	Module* module;
	std::shared_ptr<window::Svg> svgGradient = nullptr;
	NVGcolor* innerColor = nullptr;
	NVGcolor* outerColor = nullptr;
	HaloTypes* haloType = nullptr;
	unsigned char haloOpacity = 175;
	float haloRadiusFactor = 8.f;
	float haloX = 0.f;

	SanguineMultiColoredShapedLight();
	static float getLineCrossing(math::Vec p0, math::Vec p1, math::Vec p2, math::Vec p3);
	static NVGcolor getNVGColor(uint32_t color);
	static NVGpaint getPaint(NVGcontext* vg, NSVGpaint* p, NVGcolor innerColor, NVGcolor outerColor);
	void drawLayer(const DrawArgs& args, int layer) override;
};

template <typename TBase = GrayModuleLightWidget>
struct TOrangeLight : TBase {
	TOrangeLight() {
		this->addBaseColor(SCHEME_ORANGE);
	}
};
using OrangeLight = TOrangeLight<>;

template <typename TBase = GrayModuleLightWidget>
struct TPurpleLight : TBase {
	TPurpleLight() {
		this->addBaseColor(SCHEME_PURPLE);
	}
};
using PurpleLight = TPurpleLight<>;

// A light for displaying on top of a CKD6. Must add a color by subclassing or templating.
template <typename TBase>
struct CKD6Light : TBase {
	CKD6Light() {
		this->bgColor = color::BLACK_TRANSPARENT;
		this->box.size = Vec(28.f - 6.f, 28.f - 6.f);
	}
};

template <typename Base>
struct Rogan6PSLight : Base {
	Rogan6PSLight() {
		this->box.size = mm2px(Vec(23.04f, 23.04f));
		this->borderColor = nvgRGBA(0, 0, 0, 0);
	}
};

template <typename TBase>
struct SanguineShapedAcrylicLed : TSvgLight<TBase> {
	static constexpr float backgroundGrey = 30.f / 255.f;

	void draw(const Widget::DrawArgs& args) override {
		// Drawn in the background: these lights should include a background, otherwise designs show: they include transparency.
	}

	// drawLayer logic partially based on code by BaconPaul and Hemmer.
	void drawLayer(const Widget::DrawArgs& args, int layer) override {
		if (layer == 1) {

			if (!this->sw->svg) {
				return;
			}

			if (this->module && !this->module->isBypassed()) {
				// When LED is off, draw the background (grey value #1E1E1E), but if on then progressively blend away to zero.
				float backgroundFactor = std::max(0.f, 1.f - this->color.a) * backgroundGrey;

				if (backgroundFactor > 0.f) {
					NVGcolor blendedColor = nvgRGBf(backgroundFactor, backgroundFactor, backgroundFactor);

					const NSVGimage* mySvg = this->sw->svg->handle;
					const int fillColor = rgbColorToInt(static_cast<int>(blendedColor.r * 255), static_cast<int>(blendedColor.g * 255),
						static_cast<int>(blendedColor.b * 255), static_cast<int>(blendedColor.a * 255));

					fillSvgSolidColor(mySvg, fillColor);

					nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
					svgDraw(args.vg, this->sw->svg->handle);
				}

				// Main RGB color.
				const NSVGimage* mySvg = this->sw->svg->handle;
				const int fillColor = rgbColorToInt(static_cast<int>(this->color.r * 255), static_cast<int>(this->color.g * 255),
					static_cast<int>(this->color.b * 255), static_cast<int>(this->color.a * 255));

				fillSvgSolidColor(mySvg, fillColor);

				nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
				svgDraw(args.vg, this->sw->svg->handle);
				this->drawHalo(args);
			} else if (!this->module && this->color.a > 0.f) {
				// Main RGB color.
				const NSVGimage* mySvg = this->sw->svg->handle;
				const int fillColor = rgbColorToInt(static_cast<int>(this->color.r * 255), static_cast<int>(this->color.g * 255),
					static_cast<int>(this->color.b * 255), static_cast<int>(this->color.a * 255));

				fillSvgSolidColor(mySvg, fillColor);

				nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);
				svgDraw(args.vg, this->sw->svg->handle);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

template <typename TBase>
struct SanguineBezelLight115 : TBase {
	SanguineBezelLight115() {
		this->borderColor = color::WHITE_TRANSPARENT;
		this->bgColor = color::WHITE_TRANSPARENT;
		this->box.size = mm2px(math::Vec(10.583, 10.583));
	}
};

// Light states
enum LightModes {
	LIGHT_OFF,
	LIGHT_ON,
	LIGHT_BLINK,
	LIGHT_BLINK_SLOW,
	LIGHT_BLINK_FAST
};

// Decorations

struct SanguineShapedLight : SvgLight {
	Module* module;
	SanguineShapedLight(Module* theModule, const std::string& shapeFileName, const float X, const float Y, bool createCentered = true);
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineStaticRGBLight : SvgLight {
	unsigned int lightColor;
	SanguineStaticRGBLight(Module* theModule, const std::string& shapeFileName, const float X, const float Y,
		bool createCentered, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255);
	SanguineStaticRGBLight(Module* theModule, const std::string& shapeFileName, const float X, const float Y,
		bool createCentered, unsigned int newLightColor);
	void draw(const DrawArgs& args) override;
	void drawLayer(const DrawArgs& args, int layer) override;
};

struct SanguineMonoInputLight : SanguineStaticRGBLight {
	SanguineMonoInputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMonoOutputLight : SanguineStaticRGBLight {
	SanguineMonoOutputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguinePolyInputLight : SanguineStaticRGBLight {
	SanguinePolyInputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguinePolyOutputLight : SanguineStaticRGBLight {
	SanguinePolyOutputLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineBloodLogoLight : SanguineShapedLight {
	SanguineBloodLogoLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMonstersLogoLight : SanguineShapedLight {
	SanguineMonstersLogoLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

struct SanguineMutantsLogoLight : SanguineShapedLight {
	SanguineMutantsLogoLight(Module* theModule, const float X, const float Y, bool createCentered = true);
};

// Panels
struct SanguinePanel : SvgPanel {
	widget::SvgWidget* foreground;
	SanguinePanel(const std::string& newBackgroundFileName, const std::string& newForegroundFileName);
	void addLayer(const std::string& layerFileName);
};

// Modules
struct SanguineModule : Module {
	enum ExpanderPositions { EXPANDER_LEFT, EXPANDER_RIGHT };

	bool bUniqueTheme = false;
	FaceplateThemes previousTheme = THEME_NONE;
	FaceplateThemes currentTheme = THEME_VITRIOL;
	void dataFromJson(json_t* rootJ) override;
	json_t* dataToJson() override;
	void addExpander(Model* model, ModuleWidget* parentModuleWidget, ExpanderPositions expanderPosition = EXPANDER_RIGHT);
	void setModuleTheme(int themeNum);
};

// Module widgets
struct SanguineModuleWidget : ModuleWidget {
	enum ScrewIds {
		SCREW_TOP_LEFT = 1,
		SCREW_BOTTOM_LEFT = 2,
		SCREW_TOP_RIGHT = 4,
		SCREW_BOTTOM_RIGHT = 8,
		SCREW_ALL = 15
	};

	bool bFaceplateSuffix = true;
	bool bHasCommon = true;
	std::string moduleName;
	PanelSizes panelSize = SIZE_34;
	BackplateColors backplateColor = PLATE_PURPLE;
	void addScrews(int screwIds);
	void appendContextMenu(Menu* menu) override;
	void makePanel();
	void step() override;
};