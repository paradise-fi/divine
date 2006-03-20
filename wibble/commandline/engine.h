#ifndef WIBBLE_COMMANDLINE_ENGINE_H
#define WIBBLE_COMMANDLINE_ENGINE_H

#include <wibble/commandline/options.h>
#include <string>
#include <vector>
#include <map>
#include <set>
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

/**
 * Parse commandline options.
 *
 * Normally it parses short or long switches all starting with '-'
 *
 * If other engines are added, then looks in the commandline for a non-switch
 * command to select the operation mode.  This allow to have a custom set of
 * commandline options for every non-switch command.
 */
class Engine
{
	std::string m_name;

protected:
	// Elements added to this engine
	std::vector<OptionGroup*> m_groups;
	std::vector<Option*> m_options;
	std::vector<Engine*> m_commands;

	// Parse tables for commandline options
	std::map<char, Option*> m_short;
	std::map<std::string, Option*> m_long;
	std::map<std::string, Engine*> m_aliases;

	// Command selected with the non-switch command, if any were found, else
	// NULL
	Engine* m_found_command;

	void addWithoutAna(Option* o);
	void addWithoutAna(const std::vector<Option*>& o);
	void add(const std::string& alias, Engine* o);

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

#if 0
	/// Parse a consecutive sequence of switches
	ArgList::iterator parseConsecutiveSwitches(ArgList& list, ArgList::iterator begin);
#endif

	/// Parse all known Options and leave the rest in list
	ArgList::iterator parseKnownSwitches(ArgList& list, ArgList::iterator begin);

public:
	Engine(const std::string& name = std::string(),
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
		: m_name(name), m_found_command(0), primaryAlias(name),
			usage(usage), description(description), longDescription(longDescription) {}

	const std::string& name() const { return m_name; }

	/// Add an Option to this engine
	Option* add(Option* o);

	/// Add an OptionGroup to this engine
	OptionGroup* add(OptionGroup* group);

	/// Add a Engine to this engine
	Engine* add(Engine* o);

	/**
	 * Create an option and add it to this engine
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
	 * Create an OptionGroup and add it to this engine
	 */
	OptionGroup* create(const std::string& description)
	{
		OptionGroup* g = new OptionGroup(description);
		add(g);
		return g;
	}

	/**
	 * Create a Engine and add it to this engine as a command
	 */
	Engine* createEngine(const std::string& name,
					const std::string& usage = std::string(),
					const std::string& description = std::string(),
					const std::string& longDescription = std::string())
	{
		Engine* item = new Engine(name, usage, description, longDescription);
		add(item);
		return item;
	}

	/// Get the OptionGroups that have been added to this engine
	const std::vector<OptionGroup*>& groups() const { return m_groups; }

	/// Get the Options that have been added to this engine
	const std::vector<Option*>& options() const { return m_options; }

	/// Get the Engines that have been added to this engine
	const std::vector<Engine*>& commands() const { return m_commands; }

	Engine* command(const std::string& name) const
	{
		std::map<std::string, Engine*>::const_iterator i = m_aliases.find(name);
		if (i == m_aliases.end())
			return 0;
		else
			return i->second;
	}

	/// Returns true if this Engine has options to parse
	bool hasOptions() const { return !m_groups.empty() || !m_options.empty(); }

	/**
	 * Return the command that has been found in the commandline, or NULL if
	 * none have been found
	 */
	Engine* foundCommand() const { return m_found_command; }


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

/** Keep track of various wibble::commandline components, and deallocate them
 * at object destruction.
 *
 * If an object is added multiple times, it will still be deallocated only once.
 */
class MemoryManager
{
	std::set<Option*> options;
	std::set<OptionGroup*> groups;
	std::set<Engine*> engines;

	Option* addItem(Option* o) { options.insert(o); return o; }
	OptionGroup* addItem(OptionGroup* o) { groups.insert(o); return o; }
	Engine* addItem(Engine* p) { engines.insert(p); return p; }

public:
	~MemoryManager()
	{
		for (std::set<Option*>::const_iterator i = options.begin();
				i != options.end(); ++i)
			delete *i;
		for (std::set<OptionGroup*>::const_iterator i = groups.begin();
				i != groups.end(); ++i)
			delete *i;
		for (std::set<Engine*>::const_iterator i = engines.begin();
				i != engines.end(); ++i)
			delete *i;
	}
	template<typename T>
	T* add(T* item) { addItem(item); return item; }
};

}
}

// vim:set ts=4 sw=4:
#endif
