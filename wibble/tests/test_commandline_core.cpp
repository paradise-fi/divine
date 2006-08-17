#include <wibble/config.h>
#include <wibble/commandline/core.h>

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct commandline_core_shar {
};

TESTGRP( commandline_core );

using namespace wibble::commandline;

// Test isSwitch
template<> template<>
void to::test<1>()
{
	ensure_equals(ArgList::isSwitch("-a"), true);
	ensure_equals(ArgList::isSwitch("-afdg"), true);
	ensure_equals(ArgList::isSwitch("--antani"), true);
	ensure_equals(ArgList::isSwitch("--antani-blinda"), true);
	ensure_equals(ArgList::isSwitch("-"), false);
	ensure_equals(ArgList::isSwitch("--"), false);
	ensure_equals(ArgList::isSwitch("antani"), false);
	ensure_equals(ArgList::isSwitch("a-ntani"), false);
	ensure_equals(ArgList::isSwitch("a--ntani"), false);
}

// Test eraseAndAdvance
template<> template<>
void to::test<2>()
{
	ArgList list;
	list.push_back("1");
	list.push_back("2");
	list.push_back("3");

	ArgList::iterator begin = list.begin();
	ensure_equals(list.size(), 3u);

	list.eraseAndAdvance(begin);
	ensure(begin == list.begin());
	ensure_equals(list.size(), 2u);

	list.eraseAndAdvance(begin);
	ensure(begin == list.begin());
	ensure_equals(list.size(), 1u);

	list.eraseAndAdvance(begin);
	ensure(begin == list.begin());
	ensure_equals(list.size(), 0u);
	ensure(begin == list.end());
}

}

// vim:set ts=4 sw=4:
