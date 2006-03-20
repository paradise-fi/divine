#include <wibble/commandline/parser.h>
#include <ctype.h>
#include <ostream>

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

void Parser::addWithoutAna(Option* o)
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

void Parser::addWithoutAna(const std::vector<Option*>& o)
{
	for (std::vector<Option*>::const_iterator i = o.begin();
			i != o.end(); ++i)
		addWithoutAna(*i);
}

void Parser::add(const std::string& alias, Parser* o)
{
	map<string, Parser*>::iterator a = m_aliases.find(alias);
	if (a != m_aliases.end())
		throw exception::Consistency("command " + alias + " has already been set to " + a->second->name());
	m_aliases[alias] = o;
}


void Parser::rebuild()
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

	// Add the commands
	m_aliases.clear();
	for (vector<Parser*>::const_iterator i = m_commands.begin();
			i != m_commands.end(); ++i)
	{
		add((*i)->primaryAlias, *i);
		for (vector<string>::const_iterator j = (*i)->aliases.begin();
				j != (*i)->aliases.end(); ++j)
			add(*j, *i);
	}
}

std::pair<ArgList::iterator, bool> Parser::parseFirstIfKnown(ArgList& list, ArgList::iterator begin)
{
	std::string& opt = *begin;

	if (opt[1] != '-')
	{
		// Short option
		char c = opt[1];
		// Loopup the option parser
		map<char, Option*>::const_iterator parser = m_short.find(c);
		if (parser == m_short.end())
			return make_pair(begin, false);
		// Parse the arguments, if any
		ArgList::iterator next = begin; ++next;
		parser->second->parse(list, next);
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

		map<string, Option*>::const_iterator parser = m_long.find(name);
		if (parser == m_long.end())
			return make_pair(begin, false);
		parser->second->parse(arg);

		// Remove the parsed element
		list.eraseAndAdvance(begin);
	}
	return make_pair(begin, true);
}

ArgList::iterator Parser::parseKnownSwitches(ArgList& list, ArgList::iterator begin)
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


void Parser::add(Option* o)
{
	m_options.push_back(o);	
}

void Parser::add(OptionGroup* group)
{
	m_groups.push_back(group);	
}

void Parser::add(Parser* o)
{
	m_commands.push_back(o);
}


#if 0
ArgList::iterator OptionParser::parseConsecutiveSwitches(ArgList& list, ArgList::iterator begin)
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
ArgList::iterator Parser::parse(ArgList& list, ArgList::iterator begin)
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

ArgList::iterator Parser::parse(ArgList& list, ArgList::iterator begin)
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
			map<string, Parser*>::iterator a = m_aliases.find(*cmd);
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
		// Else, invoke the subcommand parser on the list
		return m_found_command->parse(list, begin);
	}
}

void Parser::dump(std::ostream& out, const std::string& pfx)
{
	rebuild();

	out << pfx << "Parser " << name() << ": " << endl;

	if (!m_commands.empty())
	{
		out << pfx << "   " << m_commands.size() << " commands:" << endl;
		for (std::vector<Parser*>::const_iterator i = m_commands.begin();
				i != m_commands.end(); ++i)
			(*i)->dump(out, pfx + "   ");
	}
	if (!m_aliases.empty())
	{
		out << pfx << "   Command parse table:" << endl;
		for (std::map<std::string, Parser*>::const_iterator i = m_aliases.begin();
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

class TestParser : public Parser
{
public:
	TestParser() :
		Parser("test"),
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
		ArgList opts;
		opts.push_back("ciaps");
		opts.push_back("-b");
		opts.push_back("cippo");
		opts.push_back("foobar");

		TestParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 2u);
		ensure_equals(string(*opts.begin()), string("ciaps"));
		ensure_equals(string(*opts.rbegin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), false);
		ensure_equals(parser.blinda.stringValue(), "cippo");
	}
	{
		ArgList opts;
		opts.push_back("-a");
		opts.push_back("foobar");

		TestParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 1u);
		ensure_equals(string(*opts.begin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.boolValue(), false);
	}
	{
		ArgList opts;
		opts.push_back("-ab");
		opts.push_back("cippo");

		TestParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.end());
		ensure_equals(opts.size(), 0u);
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.stringValue(), "cippo");
	}
	{
		ArgList opts;
		opts.push_back("--an-tani");
		opts.push_back("foobar");

		TestParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 1u);
		ensure_equals(string(*opts.begin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.boolValue(), false);
	}
	{
		ArgList opts;
		opts.push_back("--blinda=cippo");
		opts.push_back("foobar");
		opts.push_back("--antani");

		TestParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.begin());
		ensure_equals(opts.size(), 1u);
		ensure_equals(string(*opts.begin()), string("foobar"));
		ensure_equals(parser.antani.boolValue(), true);
		ensure_equals(parser.blinda.stringValue(), "cippo");
	}
}

class TestCParser : public Parser
{
public:
	class Scramble : public Parser
	{
	public:
		Scramble() :
			Parser("scramble"),
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
	class Fix : public Parser
	{
	public:
		Fix() :
			Parser("fix"),
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
		Parser("test"),
		help("help", 'h', "help", "get help")
	{
		add(&scramble);
		add(&fix);
		add(&help);
	}

	Scramble scramble;
	Fix fix;
	BoolOption help;
};

// Test CommandParser
template<> template<>
void to::test<3>()
{
	{
		ArgList opts;
		opts.push_back("--yell=foo");
		opts.push_back("mess");
		opts.push_back("-r");

		TestCParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.end());
		ensure_equals(opts.size(), 0u);
		ensure_equals(parser.foundCommand(), &parser.scramble);
		ensure_equals(parser.scramble.yell.stringValue(), "foo");
		ensure_equals(parser.scramble.random.boolValue(), true);
		ensure_equals(parser.fix.yell.stringValue(), string());
		ensure_equals(parser.fix.quick.boolValue(), false);
		ensure_equals(parser.help.boolValue(), false);
	}
	{
		ArgList opts;
		opts.push_back("--yell=foo");
		opts.push_back("fix");
		opts.push_back("--help");
		opts.push_back("-Q");

		TestCParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.end());
		ensure_equals(opts.size(), 0u);
		ensure_equals(parser.foundCommand(), &parser.fix);
		ensure_equals(parser.scramble.yell.stringValue(), string());
		ensure_equals(parser.scramble.random.boolValue(), false);
		ensure_equals(parser.fix.yell.stringValue(), "foo");
		ensure_equals(parser.fix.quick.boolValue(), true);
		ensure_equals(parser.help.boolValue(), true);
	}
	{
		ArgList opts;
		opts.push_back("--help");

		TestCParser parser;
		ArgList::iterator i = parser.parseList(opts);
		ensure(i == opts.end());
		ensure_equals(opts.size(), 0u);
		ensure_equals(parser.foundCommand(), (Parser*)0);
		ensure_equals(parser.scramble.yell.stringValue(), string());
		ensure_equals(parser.scramble.random.boolValue(), false);
		ensure_equals(parser.fix.yell.stringValue(), string());
		ensure_equals(parser.fix.quick.boolValue(), false);
		ensure_equals(parser.help.boolValue(), true);
	}
}

// Test creation shortcuts
template<> template<>
void to::test<4>()
{
	Parser parser("test", "[options]", "test parser", "this is the long description of a test parser");
	OptionGroup* group = parser.create("test option group");
	BoolOption* testBool = group->create<BoolOption>("tbool", 0, "testbool", "<val>", "a test bool switch");
	IntOption* testInt = group->create<IntOption>("tint", 0, "testint", "<val>", "a test int switch");
	StringOption* testString = group->create<StringOption>("tstring", 0, "teststring", "<val>", "a test string switch");
	BoolOption* testBool1 = parser.create<BoolOption>("tbool", 0, "testbool1", "<val>", "a test bool switch");
	IntOption* testInt1 = parser.create<IntOption>("tint", 0, "testint1", "<val>", "a test int switch");
	StringOption* testString1 = parser.create<StringOption>("tstring", 0, "teststring1", "<val>", "a test string switch");

	ArgList opts;
	opts.push_back("--testbool=true");
	opts.push_back("--testint=3");
	opts.push_back("--teststring=antani");
	opts.push_back("--testbool1=true");
	opts.push_back("--testint1=5");
	opts.push_back("--teststring1=blinda");

	ArgList::iterator i = parser.parseList(opts);
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
