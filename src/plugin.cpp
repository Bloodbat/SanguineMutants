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
}
