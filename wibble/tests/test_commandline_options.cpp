#include <wibble/config.h>
#include <wibble/commandline/options.h>
#include <wibble/tests/tut-wibble.h>

namespace tut {

struct commandline_options_shar {
};
TESTGRP( commandline_options );

using namespace std;
using namespace wibble::commandline;

// Happy trick to get access to protected methods we need to use for the tests
template<typename T>
class Public : public T
{
public:
	Public(const std::string& name)
		: T(name) {}
	Public(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: T(name, shortName, longName, usage, description) {}

	virtual ArgList::iterator parse(ArgList& a, ArgList::iterator begin) { return T::parse(a, begin); }
	virtual bool parse(const std::string& str) { return T::parse(str); }
};

// Test BoolOption
template<> template<>
void to::test<1>()
{
	Public<BoolOption> opt("test");

	ensure_equals(opt.name(), string("test"));
	ensure_equals(opt.boolValue(), false);
	ensure_equals(opt.stringValue(), string("false"));

	ensure_equals(opt.parse(string()), false);
	ensure_equals(opt.boolValue(), true);
	ensure_equals(opt.stringValue(), string("true"));
}

// Test IntOption
template<> template<>
void to::test<2>()
{
	Public<IntOption> opt("test");

	ensure_equals(opt.name(), string("test"));
	ensure_equals(opt.boolValue(), false);
	ensure_equals(opt.intValue(), 0);
	ensure_equals(opt.stringValue(), string("0"));

	ensure_equals(opt.parse("42"), true);
	ensure_equals(opt.boolValue(), true);
	ensure_equals(opt.intValue(), 42);
	ensure_equals(opt.stringValue(), string("42"));
}

// Test StringOption
template<> template<>
void to::test<3>()
{
	Public<StringOption> opt("test");

	ensure_equals(opt.name(), string("test"));
	ensure_equals(opt.boolValue(), false);
	ensure_equals(opt.stringValue(), string());

	ensure_equals(opt.parse("-a"), true);
	ensure_equals(opt.boolValue(), true);
	ensure_equals(opt.stringValue(), "-a");
}

}

// vim:set ts=4 sw=4:
