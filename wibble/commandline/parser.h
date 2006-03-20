#ifndef WIBBLE_COMMANDLINE_PARSER_H
#define WIBBLE_COMMANDLINE_PARSER_H

#include <wibble/commandline/options.h>
#include <string>
#include <vector>
#include <map>
#include <iosfwd>

namespace wibble {
namespace commandline {

#if 0
  -- This help is left around to be reintegrated when I found something
	 appropriate.  It documents the general behavior of functions in the form
	 ArgList::iterator parse(ArgList& list, ArgList::iterator begin);

	/**
	 * Parse the list of arguments, starting at 'begin' and removing the
	 * arguments it successfully parses.
	 *
	 * The 'begin' iterator can be invalidated by this function.
	 *
	 * @returns
	 *   An iterator to the first unparsed argument (can be list.end())
	 */
#endif

/// Parser of many short or long switches all starting with '-'
class OptionParser
{
	std::string m_name;

protected:
	// Elements added to this parser
	std::vector<OptionGroup*> m_groups;
	std::vector<Option*> m_options;

	// Parse tables for commandline options
	std::map<char, Option*> m_short;
	std::map<std::string, Option*> m_long;

	void addWithoutAna(Option* o);
	void addWithoutAna(const std::vector<Option*>& o);

	// Rebuild the parse tables
	void rebuild();

	/**
	 * Handle the commandline switch at 'begin'.
	 *
	 * If the switch at 'begin' cannot be handled, the list is untouched and
	 * 'begin',false is returned.  Else, the switch is removed and the new begin is
	 * returned.
	 */
	std::pair<ArgList::iterator, bool> parseFirstIfKnown(ArgList& list, ArgList::iterator begin);

	/// Parse a consecutive sequence of switches
	ArgList::iterator parseConsecutiveSwitches(ArgList& list, ArgList::iterator begin);

public:
	OptionParser(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: m_name(name), primaryAlias(name),
			usage(usage), description(description), longDescription(longDescription) {}

	const std::string& name() const { return m_name; }

	/// Add an Option to this parser
	void add(Option* o);

	/// Add an OptionGroup to this parser
	void add(OptionGroup* group);

	/**
	 * Create an option and add it to this parser
	 */
	template<typename T>
	T* create(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
	{
		T* item = new T(name, shortName, longName, usage, description);
		add(item);
		return item;
	}

	/**
	 * Create an OptionGroup and add it to this parser
	 */
	OptionGroup* create(const std::string& description)
	{
		OptionGroup* g = new OptionGroup(description);
		add(g);
		return g;
	}

	/// Get the OptionGroups that have been added to this parser
	const std::vector<OptionGroup*>& groups() const { return m_groups; }

	/// Get the Options that have been added to this parser
	const std::vector<Option*>& options() const { return m_options; }

	/**
	 * Parse the list of arguments, starting at the beginning and removing the
	 * arguments it successfully parses.
	 *
	 * @returns
	 *   An iterator to the first unparsed argument (can be list.end())
	 */
	ArgList::iterator parseList(ArgList& list) { return parse(list, list.begin()); }

	/**
	 * Parse all the switches in list, leaving only the non-switch arguments or
	 * the arguments following "--"
	 */
	ArgList::iterator parse(ArgList& list, ArgList::iterator begin);

	void dump(std::ostream& out, const std::string& prefix = std::string());

	std::string primaryAlias;
	std::vector<std::string> aliases;
	std::string usage;
	std::string description;
	std::string longDescription;
	std::string examples;
};

/**
 * Parse commandline options for programs accepting a non-switch command.
 *
 * For every non-switch command there can be a custom set of commandline
 * options.
 */
class CommandParser : public OptionParser
{
	OptionParser* m_last_command;
	std::map<std::string, OptionParser*> m_aliases;
	std::vector<OptionParser*> m_commands;

	ArgList::iterator parseGlobalSwitches(ArgList& list, ArgList::iterator begin);
	void add(const std::string& alias, OptionParser* o);
	void rebuild();

public:
	CommandParser(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: OptionParser(name, usage, description, longDescription), m_last_command(0) {}

	OptionParser* lastCommand() const { return m_last_command; }

	const std::vector<OptionParser*>& commands() const { return m_commands; }

	/// Add an Option to this parser
	void add(Option* o) { OptionParser::add(o); }
	void add(OptionGroup* group) { OptionParser::add(group); }
	void add(OptionParser* o);

#if 0
	/**
	 * Create an option and add it to this parser
	 */
	template<typename T>
	T* create(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
	{
		T* item = new T(name, shortName, longName, usage, description);
		add(item);
		return item;
	}

	/**
	 * Create an OptionGroup and add it to this parser
	 */
	OptionGroup* create(const std::string& description)
	{
		OptionGroup* g = new OptionGroup(description);
		add(g);
		return g;
	}
#endif

	/**
	 * Create a new OptionParser and add it to this parser
	 */
	OptionParser* create(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
	{
		OptionParser* item = new OptionParser(name, usage, description, longDescription);
		add(item);
		return item;
	}

	// Repeated here to make it call the right parse function
	ArgList::iterator parseList(ArgList& list) { return parse(list, list.begin()); }

	/**
	 * Look for a command as the first non-switch parameter found, then invoke
	 * the corresponding switch parser.
	 *
	 * After this function, only non-switch arguments will be left in list
	 *
	 * If no commands have been found, returns begin.
	 */
	ArgList::iterator parse(ArgList& list, ArgList::iterator begin);

	void dump(std::ostream& out, const std::string& prefix = std::string());
};

/**
 * Main parser for commandline arguments.
 *
 * It is implemented as a template, to allow to base it on top of any available
 * commandline parser.
 */
template<class Base>
class MainParser : public Base
{
	ArgList args;

public:
	MainParser(const std::string& name) : Base(name) {}
	MainParser(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: Base(name, usage, description, longDescription) {}

	ArgList parse(int argc, const char* argv[])
	{
		for (int i = 1; i < argc; i++)
			args.push_back(argv[i]);
		this->parseList(args);
		return args;
	}

	bool hasNext() const { return !args.empty(); }

	std::string next()
	{
		if (args.empty())
			return std::string();
		std::string res(*args.begin());
		args.erase(args.begin());
		return res;
	}
};

}
}

// vim:set ts=4 sw=4:
#endif
