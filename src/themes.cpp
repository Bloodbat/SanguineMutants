#include "themes.hpp"

// Config file stuff: adapted from Venom's plugin.

static std::string sanguineConfigFileName = rack::asset::user("SanguineModules.json");

void getDefaultTheme() {
	FILE* configFile = fopen(sanguineConfigFileName.c_str(), "r");
	if (configFile) {
		json_error_t error;
		json_t* rootJ = json_loadf(configFile, 0, &error);
		json_t* themeJ = json_object_get(rootJ, "defaultSanguineTheme");
		if (themeJ)
			defaultTheme = FaceplateThemes(json_integer_value(themeJ));
		fclose(configFile);
		json_decref(rootJ);
	}
}

void setDefaultTheme(int themeNum) {
	if (defaultTheme != FaceplateThemes(themeNum)) {
		FILE* configFile = fopen(sanguineConfigFileName.c_str(), "w");
		if (configFile) {
			defaultTheme = FaceplateThemes(themeNum);
			json_t* rootJ = json_object();
			json_object_set_new(rootJ, "defaultSanguineTheme", json_integer(defaultTheme));
			json_dumpf(rootJ, configFile, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
			fclose(configFile);
			json_decref(rootJ);
		}
	}
}