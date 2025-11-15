#pragma once

#include <rack.hpp>
#ifndef USING_CARDINAL_NOT_RACK
#include "themes.hpp"
#else
#include "../../SanguineModulesCommon/src/themes.hpp"
#endif

using namespace rack;

/* TODO: expanders have been disabled for MetaModule: it doesn't support them.
   If MetaModule ever supports them, re-enable them here and add them to the MetaModule json manifest. */

extern Plugin* pluginInstance;

extern Model* modelFunes;
extern Model* modelMutantsBlank;
extern Model* modelApices;
extern Model* modelAleae;
extern Model* modelNodi;
extern Model* modelContextus;
extern Model* modelNebulae;
extern Model* modelEtesia;
extern Model* modelMortuus;
extern Model* modelFluctus;
extern Model* modelIncurvationes;
extern Model* modelDistortiones;
extern Model* modelMutuus;
extern Model* modelExplorator;
extern Model* modelMarmora;
extern Model* modelAnuli;
extern Model* modelVelamina;
extern Model* modelAestus;
extern Model* modelTemulenti;
extern Model* modelVimina;
extern Model* modelScalaria;

// MetaModule disabled modules go here!
#ifndef METAMODULE
extern Model* modelNix;
extern Model* modelAnsa;
#endif