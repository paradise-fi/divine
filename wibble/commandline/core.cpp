#include <wibble/commandline/core.h>
#include <ctype.h>
#include <string.h>

namespace wibble {
namespace commandline {

bool ArgList::isSwitch(const const_iterator& iter)
{
	return ArgList::isSwitch(*iter);
}

bool ArgList::isSwitch(const iterator& iter)
{
	return ArgList::isSwitch(*iter);
}


bool ArgList::isSwitch(const std::string& str)
{
	// No empty strings
	if (str[0] == 0)
		return false;
	// Must start with a dash
	if (str[0] != '-')
		return false;
	// Must not be "-" (usually it means 'stdin' file argument)
	if (str[1] == 0)
		return false;
	// Must not be "--" (end of switches)
	if (str == "--")
		return false;
	return true;
}

bool ArgList::isSwitch(const char* str)
{
	// No empty strings
	if (str[0] == 0)
		return false;
	// Must start with a dash
	if (str[0] != '-')
		return false;
	// Must not be "-" (usually it means 'stdin' file argument)
	if (str[1] == 0)
		return false;
	// Must not be "--" (end of switches)
	if (strcmp(str, "--") == 0)
		return false;
	return true;
}

}
}


#ifdef COMPILE_TESTSUITE

#include <wibble/tests.h>

namespace tut {

struct wibble_commandline_core_shar {
};
TESTGRP(wibble_commandline_core);

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

#endif

// vim:set ts=4 sw=4:
