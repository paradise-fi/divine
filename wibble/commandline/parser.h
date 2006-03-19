#ifndef WIBBLE_COMMANDLINE_PARSER_H
#define WIBBLE_COMMANDLINE_PARSER_H

#include <wibble/commandline/options.h>
#include <string>
#include <vector>
#include <map>

namespace wibble {
namespace commandline {

/// Interface for everything that would parse an arglist
class Parser
{
	std::string m_name;

public:
	Parser(const std::string& name) : m_name(name) {}
	virtual ~Parser() {}
	
	const std::string& name() const { return m_name; }

	/**
	 * Parse the list of arguments, starting at the beginning and removing the
	 * arguments it successfully parses.
	 *
	 * @returns
	 *   An iterator to the first unparsed argument (can be list.end())
	 */
	virtual iter parseList(arglist& list) { return parse(list, list.begin()); }

	/**
	 * Parse the list of arguments, starting at 'begin' and removing the
	 * arguments it successfully parses.
	 *
	 * The 'begin' iterator can be invalidated by this function.
	 *
	 * @returns
	 *   An iterator to the first unparsed argument (can be list.end())
	 */
	virtual iter parse(arglist& list, iter begin) = 0;
};

/// Parser of many short or long switches all starting with '-'
class OptionParser : public Parser
{
	std::map<char, Option*> m_short;
	std::map<std::string, Option*> m_long;
	std::vector<OptionGroup*> m_groups;
	std::vector<Option*> m_options;

	/// Parse a consecutive sequence of switches
	iter parseConsecutiveSwitches(arglist& list, iter begin);

	void addWithoutAna(Option* o);
	void addWithoutAna(const std::vector<Option*>& o);
	void rebuild();

public:
	OptionParser(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: Parser(name), primaryAlias(name),
			usage(usage), description(description), longDescription(longDescription) {}

	void add(Option* o);
	void add(OptionGroup* group);

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
	OptionGroup* create(const std::string& description)
	{
		OptionGroup* g = new OptionGroup(description);
		add(g);
		return g;
	}

	const std::vector<OptionGroup*>& groups() const { return m_groups; }
	const std::vector<Option*>& options() const { return m_options; }

	/**
	 * Parse all the switches in list, leaving only the non-switch arguments or
	 * the arguments following "--"
	 */
	virtual iter parse(arglist& list, iter begin);

	std::string primaryAlias;
	std::vector<std::string> aliases;
	std::string usage;
	std::string description;
	std::string longDescription;
	std::string examples;
};

class CommandParser : public Parser
{
	OptionParser* m_last_command;
	std::map<std::string, OptionParser*> m_aliases;

	void add(const std::string& alias, OptionParser* o);

public:
	CommandParser(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: Parser(name), m_last_command(0),
			usage(usage), description(description), longDescription(longDescription) {}

	OptionParser* lastCommand() const { return m_last_command; }
	OptionParser* command(const std::string& name) const;

	void add(OptionParser& o);

	/**
	 * Look for a command as the first non-switch parameter found, then invoke
	 * the corresponding switch parser.
	 *
	 * After this function, only non-switch arguments will be left in list
	 *
	 * If no commands have been found, returns begin.
	 */
	virtual iter parse(arglist& list, iter begin);

	std::map<std::string, OptionParser*> getCommandInfo() const;

	std::string usage;
	std::string description;
	std::string longDescription;
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
	arglist args;

public:
	MainParser(const std::string& name) : Base(name) {}
	MainParser(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: Base(name, usage, description, longDescription) {}

	arglist parse(int argc, const char* argv[])
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
