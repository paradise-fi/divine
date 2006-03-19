#include <wibble/commandline/parser.h>
#include <ctype.h>

using namespace std;

static bool isSwitch(const char* str)
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

namespace wibble {
namespace commandline {

void OptionParser::addWithoutAna(Option* o)
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

void OptionParser::addWithoutAna(const std::vector<Option*>& o)
{
	for (std::vector<Option*>::const_iterator i = o.begin();
			i != o.end(); ++i)
		addWithoutAna(*i);
}

void OptionParser::rebuild()
{
	// Clear the parser tables
	m_short.clear();
	m_long.clear();

	// Add the options from the groups
	for (std::vector<OptionGroup*>::const_iterator i = m_groups.begin();
			i != m_groups.end(); ++i)
		addWithoutAna((*i)->options);

	// Add the stray options
	addWithoutAna(m_options);
}

void OptionParser::add(Option* o)
{
	m_options.push_back(o);	
}

void OptionParser::add(OptionGroup* group)
{
	m_groups.push_back(group);	
}

iter OptionParser::parseConsecutiveSwitches(arglist& list, iter begin)
{
	while (begin != list.end() && isSwitch(*begin))
	{
		const char* opt = *begin;

		// We take care of this switch, then
		arglist::iterator next = begin; ++next;
		list.erase(begin);
		begin = next;
		
		if (opt[1] != '-')
		{
			// Short option
			for (const char* s = opt + 1; *s; ++s)
			{
				map<char, Option*>::const_iterator parser = m_short.find(*s);
				if (parser == m_short.end())
					throw exception::BadOption(string("unknown short option -") + *s);
				// Remove the argument if it gets used
				if (parser->second->parse(*begin))
				{
					next = begin;
					++next;
					list.erase(begin);
					begin = next;
				}
			}
		} else {
			// Long option

			// Split option and argument from "--foo=bar"
			const char* sep = strchr(opt, '=');
			string name;
			if (sep == NULL)
			{
				// No argument
				name = string(opt + 2);
			} else {
				name = string(opt, 2, sep-opt-2);
				++sep;
			}

			map<string, Option*>::const_iterator parser = m_long.find(name);
			if (parser == m_long.end())
				throw exception::BadOption(string("unknown long option --") + name);
			parser->second->parse(sep);
		}
	}

	return begin;
}

iter OptionParser::parse(arglist& list, iter begin)
{
	rebuild();

	bool foundNonSwitches = false;
	iter firstNonSwitch;
	while (begin != list.end())
	{
		// Parse a row of switches
		begin = parseConsecutiveSwitches(list, begin);

		// End of switches?
		if (begin == list.end())
			break;

		// If the end is the "--" marker, take it out of the list as well
		if (strcmp(*begin, "--") == 0)
		{
			iter next = begin; ++next;
			list.erase(begin);
			begin = next;
			break;
		}

		if (!foundNonSwitches)
		{
			// Mark the start of non-switches if we haven't done it already
			firstNonSwitch = begin;
			foundNonSwitches = true;
		}
		while (begin != list.end() && !isSwitch(*begin))
			++begin;
	}
	return foundNonSwitches ? firstNonSwitch : begin;
}



void CommandParser::add(const std::string& alias, OptionParser* o)
{
	map<string, OptionParser*>::iterator a = m_aliases.find(alias);
	if (a != m_aliases.end())
		throw exception::Consistency("command " + alias + " has already been set to " + a->second->name());
	m_aliases[alias] = o;
}

void CommandParser::add(OptionParser& o)
{
	add(o.primaryAlias, &o);
	for (vector<string>::const_iterator i = o.aliases.begin();
			i != o.aliases.end(); ++i)
		add(*i, &o);
}

OptionParser* CommandParser::command(const std::string& name) const
{
	map<string, OptionParser*>::const_iterator i = m_aliases.find(name);
	if (i == m_aliases.end())
		return 0;
	else
		return i->second;
}

iter CommandParser::parse(arglist& list, iter begin)
{
	// Look for the first non-switch in the list
	iter cmd = begin;
	while (cmd != list.end() && isSwitch(*cmd))
		++cmd;

	map<string, OptionParser*>::iterator a;
	if (cmd == list.end())
	{
		// No command has been found.  Try to see if there is a 'generic'
		// OptionParser with the "" alias.
		if ((a = m_aliases.find(string())) != m_aliases.end())
			;
		else
			return begin;
	} else {
		// Handle the command
		a = m_aliases.find(*cmd);
		if (a == m_aliases.end())
			throw exception::BadOption("unknown command " + string(*cmd));

		// Remove the command from list
		if (cmd == begin)
			++begin;
		list.erase(cmd);
	}

	m_last_command = a->second;

	// Invoke the selected parser on the list
	return a->second->parse(list, begin);
}

std::map<std::string, OptionParser*> CommandParser::getCommandInfo() const
{
	std::map<std::string, OptionParser*> info;
	
	// Dig informations from cp
	for(map<string, OptionParser*>::const_iterator i = m_aliases.begin();
			i != m_aliases.end(); i++)
	{
		if (i->first == string())
			continue;
		map<string, OptionParser*>::iterator j = info.find(i->second->name());
		if (j == info.end())
			info[i->second->name()] = i->second;
	}

	return info;
}

}
}


#ifdef COMPILE_TESTSUITE

#include <wibble/tests.h>

namespace tut {

struct wibble_commandline_parser_shar {
};
TESTGRP(wibble_commandline_parser);

using namespace wibble::commandline;

// Test isSwitch
template<> template<>
void to::test<1>()
{
	ensure_equals(isSwitch("-a"), true);
	ensure_equals(isSwitch("-afdg"), true);
	ensure_equals(isSwitch("--antani"), true);
	ensure_equals(isSwitch("--antani-blinda"), true);
	ensure_equals(isSwitch("-"), false);
	ensure_equals(isSwitch("--"), false);
	ensure_equals(isSwitch("antani"), false);
	ensure_equals(isSwitch("a-ntani"), false);
	ensure_equals(isSwitch("a--ntani"), false);
}

class TestParser : public OptionParser
{
public:
	TestParser() :
		OptionParser("test"),
		antani("antani"),
		blinda("blinda")
	{
		antani.addAlias('a');
		antani.addAlias("antani");
		antani.addAlias("an-tani");

		blinda.addAlias('b');
		blinda.addAlias("blinda");

		add(&antani);
		add(&blinda);
	}

	BoolOption antani;
	StringOption blinda;
};

// Test OptionParser
template<> template<>
void to::test<2>()
{
	{
		list<const char*> opts;
		opts.push_back("ciaps");
		opts.push_back("-b");
		opts.push_back("cippo");
		opts.push_back("foobar");

		TestParser parser;
		iter i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 2u);
		ensure_equals(string(*opts.begin()), string("ciaps"));
		ensure_equals(string(*opts.rbegin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), false);
		ensure_equals(parser.blinda.stringValue(), "cippo");
	}
	{
		list<const char*> opts;
		opts.push_back("-a");
		opts.push_back("foobar");

		TestParser parser;
		iter i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 1u);
		ensure_equals(string(*opts.begin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.boolValue(), false);
	}
	{
		list<const char*> opts;
		opts.push_back("-ab");
		opts.push_back("cippo");

		TestParser parser;
		iter i = parser.parseList(opts);
		ensure(i == opts.end());
		ensure_equals(opts.size(), 0u);
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.stringValue(), "cippo");
	}
	{
		list<const char*> opts;
		opts.push_back("--an-tani");
		opts.push_back("foobar");

		TestParser parser;
		iter i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 1u);
		ensure_equals(string(*opts.begin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.boolValue(), false);
	}
	{
		list<const char*> opts;
		opts.push_back("--blinda=cippo");
		opts.push_back("foobar");
		opts.push_back("--antani");

		TestParser parser;
		iter i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 1u);
		ensure_equals(string(*opts.begin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.stringValue(), "cippo");
	}
}

class TestCParser : public CommandParser
{
public:
	class Scramble : public OptionParser
	{
	public:
		Scramble() :
			OptionParser("scramble"),
			random("random"),
			yell("yell")
		{
			random.addAlias('r');
			random.addAlias("random");
			yell.addAlias("yell");
			add(&random);
			add(&yell);
			aliases.push_back("mess");
		}

		BoolOption random;
		StringOption yell;
	};
	class Fix : public OptionParser
	{
	public:
		Fix() :
			OptionParser("fix"),
			quick("quick"),
			yell("yell")
		{
			quick.addAlias('Q');
			quick.addAlias("quick");
			yell.addAlias("yell");
			add(&quick);
			add(&yell);
		}

		BoolOption quick;
		StringOption yell;
	};

	TestCParser() :
		CommandParser("test")
	{
		add(scramble);
		add(fix);
	}

	Scramble scramble;
	Fix fix;
};

// Test CommandParser
template<> template<>
void to::test<3>()
{
	{
		list<const char*> opts;
		opts.push_back("--yell=foo");
		opts.push_back("mess");
		opts.push_back("-r");

		TestCParser parser;
		iter i = parser.parseList(opts);
		ensure(i == opts.end());
		ensure_equals(opts.size(), 0u);
		ensure_equals(parser.lastCommand(), &parser.scramble);
		ensure_equals(parser.scramble.yell.stringValue(), "foo");
		ensure_equals(parser.scramble.random.boolValue(), true);
		ensure_equals(parser.fix.yell.stringValue(), string());
		ensure_equals(parser.fix.quick.boolValue(), false);
	}
	{
		list<const char*> opts;
		opts.push_back("--yell=foo");
		opts.push_back("fix");
		opts.push_back("-Q");

		TestCParser parser;
		iter i = parser.parseList(opts);
		ensure(i == opts.end());
		ensure_equals(opts.size(), 0u);
		ensure_equals(parser.lastCommand(), &parser.fix);
		ensure_equals(parser.scramble.yell.stringValue(), string());
		ensure_equals(parser.scramble.random.boolValue(), false);
		ensure_equals(parser.fix.yell.stringValue(), "foo");
		ensure_equals(parser.fix.quick.boolValue(), true);
	}
}

// Test creation shortcuts
template<> template<>
void to::test<4>()
{
	OptionParser parser("test", "[options]", "test parser", "this is the long description of a test parser");
	OptionGroup* group = parser.create("test option group");
	BoolOption* testBool = group->create<BoolOption>("tbool", 0, "testbool", "<val>", "a test bool switch");
	IntOption* testInt = group->create<IntOption>("tint", 0, "testint", "<val>", "a test int switch");
	StringOption* testString = group->create<StringOption>("tstring", 0, "teststring", "<val>", "a test string switch");
	BoolOption* testBool1 = parser.create<BoolOption>("tbool", 0, "testbool1", "<val>", "a test bool switch");
	IntOption* testInt1 = parser.create<IntOption>("tint", 0, "testint1", "<val>", "a test int switch");
	StringOption* testString1 = parser.create<StringOption>("tstring", 0, "teststring1", "<val>", "a test string switch");

	list<const char*> opts;
	opts.push_back("--testbool=true");
	opts.push_back("--testint=3");
	opts.push_back("--teststring=antani");
	opts.push_back("--testbool1=true");
	opts.push_back("--testint1=5");
	opts.push_back("--teststring1=blinda");

	iter i = parser.parseList(opts);
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
