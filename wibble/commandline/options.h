#ifndef WIBBLE_COMMANDLINE_OPTIONS_H
#define WIBBLE_COMMANDLINE_OPTIONS_H

#include <wibble/commandline/core.h>
#include <string>
#include <vector>

namespace wibble {
namespace commandline {

/// Interface for a parser for one commandline option
class Option : public Managed
{
	std::string m_name;
	mutable std::string m_fullUsage;

protected:
	Option(const std::string& name) : m_name(name) {}
	Option(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: m_name(name), usage(usage), description(description)
	{
		if (shortName != 0)
			shortNames.push_back(shortName);
		if (!longName.empty())
			longNames.push_back(longName);
	}

	/**
	 * Parse the next commandline parameter after the short form of the command
	 * has been found.  It may or may not remove the parameter from the list,
	 * depending on if the option wants a value or not.
	 *
	 * Signal that the option has been found, with the given argument (or 0 if
	 * no argument).
	 *
	 * @returns
	 *   true if it used the argument, else false
	 */
	virtual ArgList::iterator parse(ArgList& list, ArgList::iterator begin) = 0;

	/**
	 * Parse the commandline parameter of a long commandline switch
	 *
	 * @returns true if the parameter has been used
	 */
	virtual bool parse(const std::string& param) = 0;

public:
	virtual ~Option() {}

	const std::string& name() const { return m_name; }

	void addAlias(char c) { shortNames.push_back(c); }
	void addAlias(const std::string& str) { longNames.push_back(str); }

	virtual bool boolValue() const = 0;
	virtual std::string stringValue() const = 0;
	virtual int intValue() const;

	/// Return a full usage message including all the aliases for this option
	const std::string& fullUsage() const;
	std::string fullUsageForMan() const;

	std::vector<char> shortNames;
	std::vector<std::string> longNames;

	std::string usage;
	std::string description;

	friend class OptionGroup;
	friend class Engine;
};

/// Boolean option
class BoolOption : public Option
{
	bool m_value;

protected:
	BoolOption(const std::string& name)
		: Option(name), m_value(false) {}
	BoolOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: Option(name, shortName, longName, usage, description), m_value(false) {}

	virtual ArgList::iterator parse(ArgList&, ArgList::iterator begin) { m_value = true; return begin; }
	virtual bool parse(const std::string&) { m_value = true; return false; }

public:
	bool boolValue() const { return m_value; }
	std::string stringValue() const { return m_value ? "true" : "false"; }

	friend class OptionGroup;
	friend class Engine;
};

// Option needing a compulsory string value
class StringOption : public Option
{
	std::string m_value;

protected:
	StringOption(const std::string& name)
		: Option(name)
	{
		usage = "<val>";
	}
	StringOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: Option(name, shortName, longName, usage, description)
	{
		if (usage.empty())
			this->usage = "<val>";
	}

	ArgList::iterator parse(ArgList& list, ArgList::iterator begin);
	bool parse(const std::string& param);

public:
	bool boolValue() const { return !m_value.empty(); }
	std::string stringValue() const { return m_value; }

	friend class OptionGroup;
	friend class Engine;
};

// Option needing a compulsory int value
class IntOption : public Option
{
	bool m_has_value;
	int m_value;

protected:
	IntOption(const std::string& name)
		: Option(name), m_has_value(false), m_value(0)
	{
		usage = "<num>";
	}
	IntOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: Option(name, shortName, longName, usage, description), m_has_value(0), m_value(0)
	{
		if (usage.empty())
			this->usage = "<num>";
	}

	ArgList::iterator parse(ArgList& list, ArgList::iterator begin);
	bool parse(const std::string& param);

public:
	bool boolValue() const { return m_has_value; }
	int intValue() const { return m_value; }
	std::string stringValue() const;

	friend class OptionGroup;
	friend class Engine;
};

/**
 * Commandline option with a mandatory argument naming a file which must exist.
 */
class ExistingFileOption : public Option
{
	std::string m_value;

protected:
	ExistingFileOption(const std::string& name)
		: Option(name)
	{
		usage = "<file>";
	}
	ExistingFileOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: Option(name, shortName, longName, usage, description)
	{
		if (usage.empty())
			this->usage = "<file>";
	}

	ArgList::iterator parse(ArgList& list, ArgList::iterator begin);
	bool parse(const std::string& str);

public:
	bool boolValue() const { return !m_value.empty(); }
	std::string stringValue() const { return m_value; }

	friend class OptionGroup;
	friend class Engine;
};

/**
 * Group related commandline options
 */
class OptionGroup : public Managed
{
	MemoryManager* m_manager;

protected:
	OptionGroup(MemoryManager* mman = 0, const std::string& description = std::string())
		: m_manager(mman), description(description) {}

public:
	Option* add(Option* o) { options.push_back(o); return o; }

	std::vector<Option*> options;

	std::string description;

	/**
	 * Create a new option
	 */
	template<typename T>
	T* create(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
	{
		T* item = new T(name, shortName, longName, usage, description);
		if (m_manager) m_manager->add(item);
		return item;
	}

	/**
	 * Create a new option and add it to this group
	 */
	template<typename T>
	T* add(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
	{
		T* res = create<T>(name, shortName, longName, usage, description);
		add(res);
		return res;
	}

	friend class Engine;
};

}
}

// vim:set ts=4 sw=4:
#endif
