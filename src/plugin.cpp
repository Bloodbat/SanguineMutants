#include "plugin.hpp"

/* TODO: expanders have been disabled for MetaModule: it doesn't support them.
   If MetaModule ever supports them, re-enable them here and add them to the MetaModule json manifest. */

#if defined(METAMODULE_BUILTIN)
extern Plugin* pluginInstance;
#else
Plugin* pluginInstance;
#endif

#if defined(METAMODULE_BUILTIN)
void init_SanguineMutants(rack::Plugin* p)
#else
void init(rack::Plugin* p)
#endif
{
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
#ifndef METAMODULE
	p->addModel(modelNix);
	p->addModel(modelAnsa);
#endif
	p->addModel(modelScalaria);

	getDefaultTheme();
}