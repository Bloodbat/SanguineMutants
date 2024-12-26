#include "plugin.hpp"

Plugin* pluginInstance;

void init(rack::Plugin* p) {
	pluginInstance = p;

	p->addModel(modelFunes);
	p->addModel(modelMutantsBlank);
	p->addModel(modelApices);
	p->addModel(modelAleae);
	p->addModel(modelNodi);
	p->addModel(modelContextus);
	p->addModel(modelNebulae);
	p->addModel(modelEtesia);
	p->addModel(modelMortuus);
	p->addModel(modelFluctus);
	p->addModel(modelIncurvationes);
	p->addModel(modelDistortiones);
	p->addModel(modelMutuus);
	p->addModel(modelExplorator);
	p->addModel(modelMarmora);
	p->addModel(modelAnuli);
	p->addModel(modelVelamina);
	p->addModel(modelAestus);
	p->addModel(modelTemulenti);
	p->addModel(modelVimina);
	p->addModel(modelNix);
	p->addModel(modelAnsa);

	getDefaultTheme();
}