#include <wibble/config.h>
#include <wibble/commandline/engine.h>
#include <ostream>

using namespace std;

namespace wibble {
namespace commandline {

void Engine::addWithoutAna(Option* o)
{
	const vector<char>& shorts = o->shortNames;
	for (vector<char>::const_iterator i = shorts.begin(); i != shorts.end(); i++)
	{
		map<char, Option*>::iterator j = m_short.find(*i);
		if (j != m_short.end())
			throw exception::Consistency(
					string("short option ") + *i + " is already mapped to " + j->second->name());
		m_short[*i] = o;
	}

	const vector<string>& longs = o->longNames;
	for (vector<string>::const_iterator i = longs.begin(); i != longs.end(); i++)
	{
		map<string, Option*>::iterator j = m_long.find(*i);
		if (j != m_long.end())
			throw exception::Consistency(
					string("long option ") + *i + " is already mapped to " + j->second->name());
		m_long[*i] = o;
	}
}

void Engine::addWithoutAna(const std::vector<Option*>& o)
{
	for (std::vector<Option*>::const_iterator i = o.begin();
			i != o.end(); ++i)
		addWithoutAna(*i);
}

void Engine::add(const std::string& alias, Engine* o)
{
	map<string, Engine*>::iterator a = m_aliases.find(alias);
	if (a != m_aliases.end())
		throw exception::Consistency("command " + alias + " has already been set to " + a->second->name());
	m_aliases[alias] = o;
}


void Engine::rebuild()
{
	// Clear the engine tables
	m_short.clear();
	m_long.clear();

	// Add the options from the groups
	for (std::vector<OptionGroup*>::const_iterator i = m_groups.begin();
			i != m_groups.end(); ++i)
		addWithoutAna((*i)->options);

	// Add the stray options
	addWithoutAna(m_options);

	// Add the commands
	m_aliases.clear();
	for (vector<Engine*>::const_iterator i = m_commands.begin();
			i != m_commands.end(); ++i)
	{
		add((*i)->primaryAlias, *i);
		for (vector<string>::const_iterator j = (*i)->aliases.begin();
				j != (*i)->aliases.end(); ++j)
			add(*j, *i);
	}
}

std::pair<ArgList::iterator, bool> Engine::parseFirstIfKnown(ArgList& list, ArgList::iterator begin)
{
	std::string& opt = *begin;

	if (opt[1] != '-')
	{
		// Short option
		char c = opt[1];
		// Loopup the option engine
		map<char, Option*>::const_iterator engine = m_short.find(c);
		if (engine == m_short.end())
			return make_pair(begin, false);
		// Parse the arguments, if any
		ArgList::iterator next = begin; ++next;
		engine->second->parse(list, next);
		// Dispose of the parsed argument
		if (opt[2] == 0)
		{
			// Remove what's left of the switch cluster as well
			list.eraseAndAdvance(begin);
		} else {
			// Else only remove the character from the switch
			opt.erase(opt.begin() + 1);
		}
	} else {
		// Long option

		// Split option and argument from "--foo=bar"
		size_t sep = opt.find('=');
		string name, arg;
		if (sep == string::npos)
		{
			// No argument
			name = opt.substr(2);
		} else {
			name = opt.substr(2, sep - 2);
			arg = opt.substr(sep + 1);
		}

		map<string, Option*>::const_iterator engine = m_long.find(name);
		if (engine == m_long.end())
			return make_pair(begin, false);
		engine->second->parse(arg);

		// Remove the parsed element
		list.eraseAndAdvance(begin);
	}
	return make_pair(begin, true);
}

ArgList::iterator Engine::parseKnownSwitches(ArgList& list, ArgList::iterator begin)
{
	// Parse the first items, until it works
	while (1)
	{
		if (begin == list.end())
			return begin;
		if (!list.isSwitch(begin))
			break;
		pair<ArgList::iterator, bool> res = parseFirstIfKnown(list, begin);
		if (!res.second)
			break;
		begin = res.first;
	}

	// Parse the next items
	for (ArgList::iterator cur = begin; cur != list.end(); )
	{
		// Skip non-switches
		if (!list.isSwitch(cur))
		{
			++cur;
			continue;
		}

		pair<ArgList::iterator, bool> res = parseFirstIfKnown(list, cur);
		if (!res.second)
			// If the switch is not handled, move past it
			++cur;
		else
			cur = res.first;
	}

	return begin;
}


Option* Engine::add(Option* o)
{
	m_options.push_back(o);	
	return o;
}

OptionGroup* Engine::add(OptionGroup* group)
{
	m_groups.push_back(group);	
	return group;
}

Engine* Engine::add(Engine* o)
{
	m_commands.push_back(o);
	return o;
}


#if 0
ArgList::iterator OptionEngine::parseConsecutiveSwitches(ArgList& list, ArgList::iterator begin)
{
	while (begin != list.end() && list.isSwitch(*begin))
	{
		pair<ArgList::iterator, bool> res = parseFirstIfKnown(list, begin);

		if (!res.second)
		{
			if ((*begin)[1] != '-')
				throw exception::BadOption(string("unknown short option ") + *begin);
			else
				throw exception::BadOption(string("unknown long option ") + *begin);
		}

		begin = res.first;
	}

	return begin;
}
#endif

#if 0
ArgList::iterator Engine::parse(ArgList& list, ArgList::iterator begin)
{
	rebuild();

	bool foundNonSwitches = false;
	ArgList::iterator firstNonSwitch;
	while (begin != list.end())
	{
		// Parse a row of switches
		begin = parseConsecutiveSwitches(list, begin);

		// End of switches?
		if (begin == list.end())
			break;

		// If the end is the "--" marker, take it out of the list as well
		if (*begin == "--")
		{
			list.eraseAndAdvance(begin);
			break;
		}

		if (!foundNonSwitches)
		{
			// Mark the start of non-switches if we haven't done it already
			firstNonSwitch = begin;
			foundNonSwitches = true;
		}

		// Skip past the non switches
		while (begin != list.end() && !list.isSwitch(begin))
			++begin;
	}
	return foundNonSwitches ? firstNonSwitch : begin;
}
#endif

ArgList::iterator Engine::parse(ArgList& list, ArgList::iterator begin)
{
	rebuild();

	// Parse and remove known switches
	begin = parseKnownSwitches(list, begin);

	m_found_command = 0;

	// Check if we have to handle commands
	if (!m_commands.empty())
	{
		// Look for the first non-switch in the list
		ArgList::iterator cmd = begin;
		while (cmd != list.end() && list.isSwitch(cmd))
			++cmd;

		if (cmd != list.end())
		{
			// A command has been found, ensure that we can handle it
			map<string, Engine*>::iterator a = m_aliases.find(*cmd);
			if (a == m_aliases.end())
				throw exception::BadOption("unknown command " + *cmd);

			// Remove the command from the list
			if (cmd == begin)
				++begin;
			list.erase(cmd);

			// We found a valid command, let's enable subcommand parsing
			m_found_command = a->second;
		}
	}

	if (!m_found_command)
	{
		// If we don't have any more subcommands to parse, then ensure that
		// there are no switches left to process
		for (ArgList::iterator i = begin; i != list.end(); ++i)
		{
			if (*i == "--")
			{
				// Remove '--' and stop looking for switches
				if (begin == i)
				{
					begin++;
					list.erase(i);
				}
				break;
			}
			if (list.isSwitch(i))
				throw exception::BadOption(string("unknown option ") + *i);
		}
		m_found_command = 0;
		return begin;
	} else {
		// Else, invoke the subcommand engine on the list
		return m_found_command->parse(list, begin);
	}
}

void Engine::dump(std::ostream& out, const std::string& pfx)
{
	rebuild();

	out << pfx << "Engine " << name() << ": " << endl;

	if (!m_commands.empty())
	{
		out << pfx << "   " << m_commands.size() << " commands:" << endl;
		for (std::vector<Engine*>::const_iterator i = m_commands.begin();
				i != m_commands.end(); ++i)
			(*i)->dump(out, pfx + "   ");
	}
	if (!m_aliases.empty())
	{
		out << pfx << "   Command parse table:" << endl;
		for (std::map<std::string, Engine*>::const_iterator i = m_aliases.begin();
				i != m_aliases.end(); ++i)
			out << pfx << "      " << i->first << " -> " << i->second->name() << endl;
	}
	if (!m_groups.empty())
	{
		out << pfx << "   " << m_groups.size() << " OptionGroups:" << endl;
		for (std::vector<OptionGroup*>::const_iterator i = m_groups.begin();
				i != m_groups.end(); ++i)
			out << pfx << "      " << (*i)->description << endl;
	}
	if (!m_options.empty())
	{
		out << pfx << "   " << m_options.size() << " Options:" << endl;
		for (std::vector<Option*>::const_iterator i = m_options.begin();
				i != m_options.end(); ++i)
			out << pfx << "      " << (*i)->fullUsage() << endl;
	}
	if (!m_short.empty())
	{
		out << pfx << "   Short options parse table:" << endl;
		for (std::map<char, Option*>::const_iterator i = m_short.begin();
				i != m_short.end(); ++i)
			out << pfx << "      " << i->first << " -> " << i->second->fullUsage() << endl;
	}
	if (!m_long.empty())
	{
		out << pfx << "   Long options parse table:" << endl;
		for (std::map<std::string, Option*>::const_iterator i = m_long.begin();
				i != m_long.end(); ++i)
			out << pfx << "      " << i->first << " -> " << i->second->fullUsage() << endl;
	}
}

}
}


#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests.h>

namespace wibble {
namespace tut {

struct commandline_engine_shar {
};
TESTGRP( commandline_engine );

using namespace commandline;

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
		antani = create<BoolOption>("antani", 'a', "antani");
		blinda = create<StringOption>("blinda", 'b', "blinda");

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
		help = create<BoolOption>("help", 'h', "help", "get help");

		scramble = createEngine("scramble");
		scramble_random = scramble->create<BoolOption>("random", 'r', "random");
		scramble_yell = scramble->create<StringOption>("yell", 0, "yell");
		scramble->aliases.push_back("mess");

		fix = createEngine("fix");
		fix_quick = fix->create<BoolOption>("quick", 'Q', "quick");
		fix_yell = fix->create<StringOption>("yell", 0, "yell");
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
	OptionGroup* group = engine.create("test option group");
	BoolOption* testBool = group->create<BoolOption>("tbool", 0, "testbool", "<val>", "a test bool switch");
	IntOption* testInt = group->create<IntOption>("tint", 0, "testint", "<val>", "a test int switch");
	StringOption* testString = group->create<StringOption>("tstring", 0, "teststring", "<val>", "a test string switch");
	BoolOption* testBool1 = engine.create<BoolOption>("tbool", 0, "testbool1", "<val>", "a test bool switch");
	IntOption* testInt1 = engine.create<IntOption>("tint", 0, "testint1", "<val>", "a test int switch");
	StringOption* testString1 = engine.create<StringOption>("tstring", 0, "teststring1", "<val>", "a test string switch");

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

	delete testString1;
	delete testInt1;
	delete testBool1;
	delete testString;
	delete testInt;
	delete testBool;
	delete group;
}

}

#endif

// vim:set ts=4 sw=4:
