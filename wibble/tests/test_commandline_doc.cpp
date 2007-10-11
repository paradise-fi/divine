#include <wibble/config.h>
#include <wibble/commandline/doc.h>
#include <wibble/tests/tut-wibble.h>
#include <sstream>

namespace tut {

struct wibble_commandline_shar {
};
TESTGRP(wibble_commandline);

using namespace wibble::commandline;

#include <iostream>
template<> template<>
void to::test<1>()
{
	StandardParserWithMandatoryCommand p("test", "1.0", 1, "enrico@enricozini.org");
	//Parser p("test");
	//p.add<BoolOption>("antani", 'a', "antani", "blinda", "supercazzola");
	//p.add<BoolOption>("antani", 'a', "antani", "", "supercazzola");
	//OptionGroup* g = p.addGroup("Test options");
	//g->add<BoolOption>("antani", 'a', "antani", "", "supercazzola");
	Engine* e = p.addEngine("testEngine");
	OptionGroup* g = e->addGroup("Test options");
	g->add<BoolOption>("antani", 'a', "antani", "", "supercazzola");

	Help h("testapp", "1.0");

	std::stringstream str;
	//h.outputHelp(str, p);
	const char* opts[] = {"test", "help", "testEngine", NULL};
	p.parse(3, opts);


	std::cerr << str.str() << std::endl;
}

}

// vim:set ts=4 sw=4:
