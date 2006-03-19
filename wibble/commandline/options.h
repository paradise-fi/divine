#ifndef WIBBLE_COMMANDLINE_OPTIONS_H
#define WIBBLE_COMMANDLINE_OPTIONS_H

#include <wibble/commandline/core.h>
#include <string>
#include <vector>

namespace wibble {
namespace commandline {

/// Interface for a parser for one commandline option
class Option
{
	std::string m_name;
	mutable std::string m_fullUsage;

public:
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
	virtual ~Option() {}

	const std::string& name() const { return m_name; }

	void addAlias(char c) { shortNames.push_back(c); }
	void addAlias(const std::string& str) { longNames.push_back(str); }

	virtual bool boolValue() const = 0;
	virtual std::string stringValue() const = 0;
	virtual int intValue() const;

	/**
	 * Signal that the option has been found, with the given argument (or 0 if
	 * no argument).
	 *
	 * @returns
	 *   true if it used the argument, else false
	 */
	virtual bool parse(const char* str = 0) = 0;

	/// Return a full usage message including all the aliases for this option
	const std::string& fullUsage() const;
	std::string fullUsageForMan() const;

	std::vector<char> shortNames;
	std::vector<std::string> longNames;

	std::string usage;
	std::string description;
};

/// Boolean option
class BoolOption : public Option
{
	bool m_value;
public:
	BoolOption(const std::string& name)
		: Option(name), m_value(false) {}
	BoolOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: Option(name, shortName, longName, usage, description), m_value(false) {}

	bool boolValue() const { return m_value; }
	std::string stringValue() const { return m_value ? "true" : "false"; }

	bool parse(const char*) { m_value = true; return false; }
};

// Option needing a compulsory string value
class StringOption : public Option
{
	std::string m_value;
public:
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

	bool boolValue() const { return !m_value.empty(); }
	std::string stringValue() const { return m_value; }

	bool parse(const char* str);
};

// Option needing a compulsory int value
class IntOption : public Option
{
	bool m_has_value;
	int m_value;

public:
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


	bool boolValue() const { return m_has_value; }
	int intValue() const { return m_value; }
	std::string stringValue() const;

	bool parse(const char* str);
};

/**
 * Commandline option with a mandatory argument naming a file which must exist.
 */
class ExistingFileOption : public Option
{
	std::string m_value;
public:
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


	bool boolValue() const { return !m_value.empty(); }
	std::string stringValue() const { return m_value; }

	bool parse(const char* str);
};

/**
 * Group related commandline options
 */
class OptionGroup
{

public:
	OptionGroup(const std::string& description = std::string())
		: description(description) {}

	void add(Option* o) { options.push_back(o); }

	std::vector<Option*> options;

	std::string description;

	/**
	 * Create a new option and add it to this group
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
};

}
}

// vim:set ts=4 sw=4:
#endif
