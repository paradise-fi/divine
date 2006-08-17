#include <wibble/config.h>
#include <wibble/commandline/engine.h>
#if 0
#include <ostream>
#endif

#include <wibble/tests/tut-wibble.h>

namespace tut {

struct commandline_engine_shar {
};
TESTGRP( commandline_engine );

using namespace wibble::commandline;
using namespace std;

// Happy trick to get access to protected methods we need to use for the tests
template<typename T>
class Public : public T
{
public:
	Public(MemoryManager* mman = 0, const std::string& name = std::string(),
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: T(mman, name, usage, description, longDescription) {}

	ArgList::iterator parseList(ArgList& list) { return T::parseList(list); }
    ArgList::iterator parse(ArgList& list, ArgList::iterator begin)
	{
		return T::parse(list, begin);
	}
};

class TestEngine : public Public<Engine>
{
	MemoryManager mman;

public:
	TestEngine() : Public<Engine>(&mman)
	{
		antani = add<BoolOption>("antani", 'a', "antani");
		blinda = add<StringOption>("blinda", 'b', "blinda");

		antani->addAlias("an-tani");
	}

	BoolOption* antani;
	StringOption* blinda;
};

// Test mix of options and arguments
template<> template<>
void to::test<1>()
{
	ArgList opts;
	opts.push_back("ciaps");
	opts.push_back("-b");
	opts.push_back("cippo");
	opts.push_back("foobar");

	TestEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.begin());
	ensure_equals(opts.size(), 2u);
	ensure_equals(string(*opts.begin()), string("ciaps"));
	ensure_equals(string(*opts.rbegin()), string("foobar"));
	ensure_equals(engine.antani->boolValue(), false);
	ensure_equals(engine.blinda->stringValue(), "cippo");
}

// Test only options
template<> template<>
void to::test<2>()
{
	ArgList opts;
	opts.push_back("-a");
	opts.push_back("foobar");

	TestEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.begin());
	ensure_equals(opts.size(), 1u);
	ensure_equals(string(*opts.begin()), string("foobar"));
	ensure_equals(engine.antani->boolValue(), true);
	ensure_equals(engine.blinda->boolValue(), false);
}

// Test clustered short options
template<> template<>
void to::test<3>()
{
	ArgList opts;
	opts.push_back("-ab");
	opts.push_back("cippo");

	TestEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.end());
	ensure_equals(opts.size(), 0u);
	ensure_equals(engine.antani->boolValue(), true);
	ensure_equals(engine.blinda->stringValue(), "cippo");
}

// Test long options with dashes inside
template<> template<>
void to::test<4>()
{
	ArgList opts;
	opts.push_back("--an-tani");
	opts.push_back("foobar");

	TestEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.begin());
	ensure_equals(opts.size(), 1u);
	ensure_equals(string(*opts.begin()), string("foobar"));
	ensure_equals(engine.antani->boolValue(), true);
	ensure_equals(engine.blinda->boolValue(), false);
}

// Test long options with arguments
template<> template<>
void to::test<5>()
{
	ArgList opts;
	opts.push_back("--blinda=cippo");
	opts.push_back("foobar");
	opts.push_back("--antani");

	TestEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.begin());
	ensure_equals(opts.size(), 1u);
	ensure_equals(string(*opts.begin()), string("foobar"));
	ensure_equals(engine.antani->boolValue(), true);
	ensure_equals(engine.blinda->stringValue(), "cippo");
}

class TestCEngine : public Public<Engine>
{
	MemoryManager mman;

public:
	TestCEngine() : Public<Engine>(&mman)
	{
		help = add<BoolOption>("help", 'h', "help", "get help");

		scramble = addEngine("scramble");
		scramble_random = scramble->add<BoolOption>("random", 'r', "random");
		scramble_yell = scramble->add<StringOption>("yell", 0, "yell");
		scramble->aliases.push_back("mess");

		fix = addEngine("fix");
		fix_quick = fix->add<BoolOption>("quick", 'Q', "quick");
		fix_yell = fix->add<StringOption>("yell", 0, "yell");
	}

	BoolOption*		help;
	Engine*			scramble;
	BoolOption*		scramble_random;
	StringOption*	scramble_yell;
	Engine*			fix;
	BoolOption*		fix_quick;
	StringOption*	fix_yell;
};

// Test command with arguments
template<> template<>
void to::test<6>()
{
	ArgList opts;
	opts.push_back("--yell=foo");
	opts.push_back("mess");
	opts.push_back("-r");

	TestCEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.end());
	ensure_equals(opts.size(), 0u);
	ensure_equals(engine.foundCommand(), engine.scramble);
	ensure_equals(engine.scramble_yell->stringValue(), "foo");
	ensure_equals(engine.scramble_random->boolValue(), true);
	ensure_equals(engine.fix_yell->stringValue(), string());
	ensure_equals(engine.fix_quick->boolValue(), false);
	ensure_equals(engine.help->boolValue(), false);
}

// Test the other command, with overlapping arguments
template<> template<>
void to::test<7>()
{
	ArgList opts;
	opts.push_back("--yell=foo");
	opts.push_back("fix");
	opts.push_back("--help");
	opts.push_back("-Q");

	TestCEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.end());
	ensure_equals(opts.size(), 0u);
	ensure_equals(engine.foundCommand(), engine.fix);
	ensure_equals(engine.scramble_yell->stringValue(), string());
	ensure_equals(engine.scramble_random->boolValue(), false);
	ensure_equals(engine.fix_yell->stringValue(), "foo");
	ensure_equals(engine.fix_quick->boolValue(), true);
	ensure_equals(engine.help->boolValue(), true);
}

// Test invocation without command
template<> template<>
void to::test<8>()
{
	ArgList opts;
	opts.push_back("--help");

	TestCEngine engine;
	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.end());
	ensure_equals(opts.size(), 0u);
	ensure_equals(engine.foundCommand(), (Engine*)0);
	ensure_equals(engine.scramble_yell->stringValue(), string());
	ensure_equals(engine.scramble_random->boolValue(), false);
	ensure_equals(engine.fix_yell->stringValue(), string());
	ensure_equals(engine.fix_quick->boolValue(), false);
	ensure_equals(engine.help->boolValue(), true);
}

// Test creation shortcuts
template<> template<>
void to::test<9>()
{
	MemoryManager mman;
	Public<Engine> engine(&mman, "test", "[options]", "test engine", "this is the long description of a test engine");
	OptionGroup* group = engine.addGroup("test option group");
	BoolOption* testBool = group->add<BoolOption>("tbool", 0, "testbool", "<val>", "a test bool switch");
	IntOption* testInt = group->add<IntOption>("tint", 0, "testint", "<val>", "a test int switch");
	StringOption* testString = group->add<StringOption>("tstring", 0, "teststring", "<val>", "a test string switch");
	BoolOption* testBool1 = engine.add<BoolOption>("tbool", 0, "testbool1", "<val>", "a test bool switch");
	IntOption* testInt1 = engine.add<IntOption>("tint", 0, "testint1", "<val>", "a test int switch");
	StringOption* testString1 = engine.add<StringOption>("tstring", 0, "teststring1", "<val>", "a test string switch");

	ArgList opts;
	opts.push_back("--testbool=true");
	opts.push_back("--testint=3");
	opts.push_back("--teststring=antani");
	opts.push_back("--testbool1=true");
	opts.push_back("--testint1=5");
	opts.push_back("--teststring1=blinda");

	ArgList::iterator i = engine.parseList(opts);
	ensure(i == opts.end());
	ensure_equals(opts.size(), 0u);
	ensure_equals(testBool->boolValue(), true);
	ensure_equals(testInt->intValue(), 3);
	ensure_equals(testString->stringValue(), "antani");
	ensure_equals(testBool1->boolValue(), true);
	ensure_equals(testInt1->intValue(), 5);
	ensure_equals(testString1->stringValue(), "blinda");
}

}

// vim:set ts=4 sw=4:
