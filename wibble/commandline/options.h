#ifndef WIBBLE_COMMANDLINE_OPTIONS_H
#define WIBBLE_COMMANDLINE_OPTIONS_H

#include <wibble/commandline/core.h>
#include <string>
#include <vector>

namespace wibble {
namespace commandline {

// Types of values for the command line options

struct Bool
{
	typedef bool value_type;
	static bool parse(const std::string& val);

	static bool toBool(const value_type& val);
	static int toInt(const value_type& val);
	static std::string toString(const value_type& val);
};

struct Int
{
	typedef int value_type;
	static int parse(const std::string& val);

	static bool toBool(const value_type& val);
	static int toInt(const value_type& val);
	static std::string toString(const value_type& val);
};

struct String
{
	typedef std::string value_type;
	static std::string parse(const std::string& val);

	static bool toBool(const value_type& val);
	static int toInt(const value_type& val);
	static std::string toString(const value_type& val);
};

struct ExistingFile
{
	typedef std::string value_type;
	static std::string parse(const std::string& val);
	static std::string toString(const value_type& val);
};

/// Interface for a parser for one commandline option
class Option : public Managed
{
	std::string m_name;
	mutable std::string m_fullUsage;

protected:
	bool m_isset;

	Option(const std::string& name) : m_name(name), m_isset(false) {}
	Option(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: m_name(name), m_isset(false), usage(usage), description(description)
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
	Option();
	virtual ~Option() {}

	bool isSet() const { return m_isset; }
	const std::string& name() const { return m_name; }

	void addAlias(char c) { shortNames.push_back(c); }
	void addAlias(const std::string& str) { longNames.push_back(str); }

	/// Return a full usage message including all the aliases for this option
	const std::string& fullUsage() const;
	std::string fullUsageForMan() const;

	std::vector<char> shortNames;
	std::vector<std::string> longNames;

	std::string usage;
	std::string description;

	// Set to true if the option should not be documented
	bool hidden;

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

	virtual ArgList::iterator parse(ArgList&, ArgList::iterator begin) { m_isset = true; m_value = true; return begin; }
	virtual bool parse(const std::string&) { m_isset = true; m_value = true; return false; }

public:
	bool boolValue() const { return m_value; }
	std::string stringValue() const { return m_value ? "true" : "false"; }

	friend class OptionGroup;
	friend class Engine;
};

template<typename T>
class SingleOption : public Option
{
protected:
	typename T::value_type m_value;

	SingleOption(const std::string& name)
		: Option(name)
	{
		usage = "<val>";
	}
	SingleOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: Option(name, shortName, longName, usage, description)
	{
		if (usage.empty())
			this->usage = "<val>";
	}

	ArgList::iterator parse(ArgList& list, ArgList::iterator begin)
	{
		if (begin == list.end())
			throw exception::BadOption("no string argument found");
		m_value = T::parse(*begin);
		m_isset = true;
		// Remove the parsed element
		return list.eraseAndAdvance(begin);
	}
	bool parse(const std::string& param)
	{
		m_value = T::parse(param);
		m_isset = true;
		return true;
	}

public:
        void setValue( const typename T::value_type &a ) {
            m_value = a;
        }

	typename T::value_type value() const { return m_value; }

	// Deprecated
	bool boolValue() const { return T::toBool(m_value); }
	int intValue() const { return T::toInt(m_value); }
	std::string stringValue() const { return T::toString(m_value); }

	friend class OptionGroup;
	friend class Engine;
};

// Option needing a compulsory string value
typedef SingleOption<String> StringOption;

// Option needing a compulsory int value
struct IntOption : public SingleOption<Int>
{
protected:
	IntOption(const std::string& name) : SingleOption<Int>(name)
	{
		m_value = 0;
	}
	IntOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: SingleOption<Int>(name, shortName, longName, usage, description)
	{
		m_value = 0;
	}
public:
	friend class OptionGroup;
	friend class Engine;
};

/**
 * Commandline option with a mandatory argument naming a file which must exist.
 */
typedef SingleOption<ExistingFile> ExistingFileOption;


// Option that can be specified multiple times
template<typename T>
class VectorOption : public Option
{
	std::vector< typename T::value_type > m_values;

protected:
	VectorOption(const std::string& name)
		: Option(name)
	{
		usage = "<val>";
	}
	VectorOption(const std::string& name,
			char shortName,
			const std::string& longName,
			const std::string& usage = std::string(),
			const std::string& description = std::string())
		: Option(name, shortName, longName, usage, description)
	{
		if (usage.empty())
			this->usage = "<val>";
	}

	ArgList::iterator parse(ArgList& list, ArgList::iterator begin)
	{
		if (begin == list.end())
			throw exception::BadOption("no string argument found");
		m_isset = true;
		m_values.push_back(T::parse(*begin));
		// Remove the parsed element
		return list.eraseAndAdvance(begin);
	}
	bool parse(const std::string& param)
	{
		m_isset = true;
		m_values.push_back(T::parse(param));
		return true;
	}

public:
	bool boolValue() const { return !m_values.empty(); }
	const std::vector< typename T::value_type >& values() const { return m_values; }

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
		: m_manager(mman), description(description), hidden(false) {}

public:
	Option* add(Option* o) { options.push_back(o); return o; }

	std::vector<Option*> options;

	std::string description;

	// Set to true if the option group should not be documented
	bool hidden;

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
