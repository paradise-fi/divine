// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * Commandline parser.
 */

/*
 * (c) 2007 Enrico Zini <enrico@enricozini.org>
 * (c) 2014 Vladimír Štill <xstill@fi.muni.cz>
 * (c) 2014 Petr Ročkai <me@mornfall.net>
 */

/* Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE. */

#include <brick-string.h>
#include <brick-unittest.h>

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <iostream>

#ifdef __unix
#include <unistd.h>
#endif

#ifndef BRICK_COMMANDLINE_H
#define BRICK_COMMANDLINE_H

namespace brick {
namespace commandline {

struct BadOption : std::runtime_error
{
    BadOption(const std::string& error) throw ()
        : std::runtime_error( error + " while parsing commandline options" )
    {
    }

    ~BadOption() throw () {}
};

struct ApiError : std::logic_error
{
    ApiError(const std::string& error) throw ()
        : std::logic_error( error + " while constructing commandline parser" )
    {
    }

    ~ApiError() throw () {}
};

class ArgList : public std::list<std::string>
{
public:
    // Remove the item pointed by the iterator, and advance the iterator to the
    // next item.  Returns i itself.
    inline iterator& eraseAndAdvance(iterator& i)
    {
        if (i == end())
            return i;
        iterator next = i;
        ++next;
        erase(i);
        i = next;
        return i;
    }

    static bool isSwitch(const char* str);
    static bool isSwitch(const std::string& str);
    static bool isSwitch(const const_iterator& iter);
    static bool isSwitch(const iterator& iter);
};

class Managed
{
public:
    virtual ~Managed() {}
};

/** Keep track of various wibble::commandline components, and deallocate them
 * at object destruction.
 *
 * If an object is added multiple times, it will still be deallocated only once.
 */
class MemoryManager
{
    std::set<Managed*> components;

    Managed* addManaged(Managed* o) { components.insert(o); return o; }
public:
    ~MemoryManager()
    {
        for (std::set<Managed*>::const_iterator i = components.begin();
             i != components.end(); ++i)
            delete *i;
    }

    template<typename T>
    T* add(T* item) { addManaged(item); return item; }
};

struct Bool
{
    typedef bool value_type;
    static bool parse(const std::string& val);

    static bool toBool(const value_type& val);
    static int toInt(const value_type& val);
    static std::string toString(const value_type& val);
    static bool init_val;
};

struct Int
{
    typedef int value_type;
    static int parse(const std::string& val);

    static bool toBool(const value_type& val);
    static int toInt(const value_type& val);
    static std::string toString(const value_type& val);
    static int init_val;
};

struct String
{
    typedef std::string value_type;
    static std::string parse(const std::string& val);

    static bool toBool(const value_type& val);
    static int toInt(const value_type& val);
    static std::string toString(const value_type& val);
    static std::string init_val;
};

struct ExistingFile
{
    typedef std::string value_type;
    static std::string parse(const std::string& val);
    static std::string toString(const value_type& val);
    static std::string init_val;
};

/// Interface for a parser for one commandline option
class Option : public Managed
{
    std::string m_name;
    mutable std::string m_fullUsage;

protected:
    bool m_isset;

    Option(const std::string& name) : m_name(name), m_isset(false), hidden(false) {}
    Option(const std::string& name,
           char shortName,
           const std::string& longName,
           const std::string& usage = std::string(),
           const std::string& description = std::string())
        : m_name(name), m_isset(false), usage(usage), description(description), hidden(false)
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

    /**
     * Notify that the option is present in the command line, but has no
     * arguments
     */
    virtual void parse_noarg() = 0;

    /// Return true if the argument to this function can be omitted
    virtual bool arg_is_optional() const { return false; }

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

    virtual ArgList::iterator parse(ArgList&, ArgList::iterator begin) { parse_noarg(); return begin; }
    virtual bool parse(const std::string&) { parse_noarg(); return false; }
    virtual void parse_noarg() { m_isset = true; m_value = true; }

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
        : Option(name), m_value(T::init_val)
    {
        usage = "<val>";
    }
    SingleOption(const std::string& name,
                 char shortName,
                 const std::string& longName,
                 const std::string& usage = std::string(),
                 const std::string& description = std::string())
        : Option(name, shortName, longName, usage, description), m_value(T::init_val)
    {
        if (usage.empty())
            this->usage = "<val>";
    }

    ArgList::iterator parse(ArgList& list, ArgList::iterator begin)
    {
        if (begin == list.end())
            throw BadOption("option requires an argument");
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
    void parse_noarg()
    {
        throw BadOption("option requires an argument");
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

/**
 * Single option whose value can be or not be specified.
 *
 * It works for long option style only: short options with an optional argument
 * would be ambiguous.
 */
template<typename T>
class SingleOptvalOption : public Option
{
protected:
    typename T::value_type m_value;
    bool m_hasval;

    SingleOptvalOption(const std::string& name)
        : Option(name), m_value(T::init_val), m_hasval(false)
    {
        usage = "<val>";
    }
    SingleOptvalOption(const std::string& name,
                       char shortName,
                       const std::string& longName,
                       const std::string& usage = std::string(),
                       const std::string& description = std::string())
        : Option(name, 0, longName, usage, description), m_value(T::init_val), m_hasval(false)
    {
        if (shortName != 0)
            throw ApiError(
                "creating option " + name + " with optional value"
                "short options with optional values are not allowed");
        if (usage.empty())
            this->usage = "<val>";
    }

    ArgList::iterator parse(ArgList& list, ArgList::iterator begin)
    {
        throw ApiError(
            "parsing option with optional value"
            "short options with optional values are not allowed");
    }
    bool parse(const std::string& param)
    {
        m_value = T::parse(param);
        m_isset = true;
        m_hasval = true;
        return true;
    }
    void parse_noarg()
    {
        m_isset = true;
        m_hasval = false;
    }

    virtual bool arg_is_optional() const { return true; }

public:
    bool hasValue() const { return m_hasval; }

    void setValue( const typename T::value_type &a ) {
        m_value = a;
    }

    typename T::value_type value() const { return m_value; }

    friend class OptionGroup;
    friend class Engine;
};

// Option needing a compulsory string value
typedef SingleOption<String> StringOption;

// Option with an optional string value
typedef SingleOptvalOption<String> OptvalStringOption;

// Option needing a compulsory int value
typedef SingleOption<Int> IntOption;

// Option with an optional int value
typedef SingleOptvalOption<Int> OptvalIntOption;

/// Commandline option with a mandatory argument naming a file which must exist.
typedef SingleOption<ExistingFile> ExistingFileOption;

/// Commandline option with an optional argument naming a file which must exist.
typedef SingleOptvalOption<ExistingFile> OptvalExistingFileOption;


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
            throw BadOption("no string argument found");
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
    void parse_noarg()
    {
        throw BadOption("option requires an argument");
    }

public:
    bool boolValue() const { return !m_values.empty(); }
    const std::vector< typename T::value_type >& values() const { return m_values; }

    friend class OptionGroup;
    friend class Engine;
};

template< typename T >
class VectorOptvalOption : public VectorOption< T > {
    bool _emptyVal;

protected:
    VectorOptvalOption(const std::string& name)
        : VectorOption< T >( name ), _emptyVal( false )
    {
        this->usage = "<val>";
    }

    VectorOptvalOption(const std::string& name,
                       char shortName,
                       const std::string& longName,
                       const std::string& usage = std::string(),
                       const std::string& description = std::string())
        : VectorOption< T >(name, 0, longName, usage, description), _emptyVal( false )
    {
        if (shortName != 0)
            throw ApiError(
                "creating option " + name + " with optional value"
                "short options with optional values are not allowed");
    }

    virtual bool arg_is_optional() const { return true; }

    void parse_noarg() {
        _emptyVal = true;
        this->m_isset = true;
    }

public:
    bool emptyValueSet() { return _emptyVal; }

    friend class OptionGroup;
    friend class Engine;
};

typedef VectorOptvalOption<String> OptvalStringVectorOption;

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

/**
 * Parse commandline options.
 *
 * Normally it parses short or long switches all starting with '-'
 *
 * If other engines are added, then looks in the commandline for a non-switch
 * command to select the operation mode.  This allow to have a custom set of
 * commandline options for every non-switch command.
 */

class Engine : public Managed
{
    MemoryManager* m_manager;
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

    /// Parse all known Options and leave the rest in list
    ArgList::iterator parseKnownSwitches(ArgList& list, ArgList::iterator begin);

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


    Engine(MemoryManager* mman = 0, const std::string& name = std::string(),
           const std::string& usage = std::string(),
           const std::string& description = std::string(),
           const std::string& longDescription = std::string())
        : m_manager(mman), m_name(name), m_found_command(0), primaryAlias(name),
          usage(usage), description(description), longDescription(longDescription), hidden(false),
          no_switches_after_first_arg(false), partial_matching( false ) {}

public:
    const std::string& name() const { return m_name; }

    /// Add an Option to this engine
    Option* add(Option* o);

    /// Add an OptionGroup to this engine
    OptionGroup* add(OptionGroup* group);

    /// Add a Engine to this engine
    Engine* add(Engine* o);

    /**
     * Create an option
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
     * Create an option and add to this engine
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

    /**
     * Create an OptionGroup
     */
    OptionGroup* createGroup(const std::string& description)
    {
        OptionGroup* g = new OptionGroup(m_manager, description);
        if (m_manager) m_manager->add(g);
        return g;
    }

    /**
     * Create an OptionGroup and add it to this engine
     */
    OptionGroup* addGroup(const std::string& description)
    {
        return add(createGroup(description));
    }

    /**
     * Create a Engine
     */
    Engine* createEngine(const std::string& name,
                         const std::string& usage = std::string(),
                         const std::string& description = std::string(),
                         const std::string& longDescription = std::string())
    {
        Engine* item = new Engine(m_manager, name, usage, description, longDescription);
        if (m_manager) m_manager->add(item);
        return item;
    }

    /**
     * Create a Engine and add it to this engine as a command
     */
    Engine* addEngine(const std::string& name,
                      const std::string& usage = std::string(),
                      const std::string& description = std::string(),
                      const std::string& longDescription = std::string())
    {
        return add(createEngine(name, usage, description, longDescription));
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


    void dump(std::ostream& out, const std::string& prefix = std::string());

    std::string primaryAlias;
    std::vector<std::string> aliases;
    std::string usage;
    std::string description;
    std::string longDescription;
    std::string examples;

    // Set to true if the engine should not be documented
    bool hidden;

    // Set to true if no switches should be parsed after the first
    // non-switch argument, and they should be just left in the argument
    // list
    bool no_switches_after_first_arg;

    /* Enable partial/prefix matching of long options, that is instead
     * of fully specifiing long option, unabiguous prefix can be used
     *
     * NOTE: In engine with subcommands, if prefix of subcommand's option
     * matches complete (higher-level) command option it is NOT considered
     * ambiguous but it is considered to be option of given (higher-level)
     * command.
     */
    bool partial_matching;

    /* set partial matching for this engine and all subcomand engines */
    void setPartialMatchingRecursively( bool );


    friend class Parser;
};

/**
 * Generic parser for commandline arguments.
 */
class Parser : public Engine
{
protected:
    ArgList m_args;

    MemoryManager m_manager;

public:
    Parser(const std::string& name,
           const std::string& usage = std::string(),
           const std::string& description = std::string(),
           const std::string& longDescription = std::string())
        : Engine(&m_manager, name, usage, description, longDescription) {}

    /**
     * Parse the commandline
     *
     * @returns true if it also took care of performing the action requested by
     *   the user, or false if the caller should do it instead.
     */
    bool parse(int argc, const char* argv[])
    {
        m_args.clear();
        for (int i = 1; i < argc; i++)
            m_args.push_back(argv[i]);
        parseList(m_args);
        return false;
    }

    bool hasNext() const { return !m_args.empty(); }

    std::string next()
    {
        if (m_args.empty())
            return std::string();
        std::string res(*m_args.begin());
        m_args.erase(m_args.begin());
        return res;
    }
};

class HelpWriter;

class DocMaker
{
protected:
    std::string m_app;
    std::string m_ver;

public:
    DocMaker(const std::string& app, const std::string& ver)
        : m_app(app), m_ver(ver) {}
};

class Help : public DocMaker
{
protected:
    void outputOptions(std::ostream& out, HelpWriter& writer, const Engine& cp);

public:
    Help(const std::string& app, const std::string& ver)
        : DocMaker(app, ver) {}

    void outputVersion(std::ostream& out);
    void outputHelp(std::ostream& out, const Engine& cp);
};

class Manpage : public DocMaker
{
public:
    enum where { BEFORE, BEGINNING, END };

private:
    struct Hook
    {
        std::string section;
        where placement;
        std::string text;

        Hook(const std::string& section, where placement, const std::string& text)
            : section(section), placement(placement), text(text) {}
    };

    int m_section;
    std::string m_author;

    std::vector<Hook> hooks;
    std::string lastSection;

    void outputParagraph(std::ostream& out, const std::string& str);
    void outputOption(std::ostream& out, const Option* o);
    void outputOptions(std::ostream& out, const Engine& p);
    void runHooks(std::ostream& out, const std::string& section, where where);
    void startSection(std::ostream& out, const std::string& name);
    void endSection(std::ostream& out);


public:
    Manpage(const std::string& app, const std::string& ver, int section, const std::string& author)
        : DocMaker(app, ver), m_section(section), m_author(author) {}

    void addHook(const std::string& section, where placement, const std::string& text)
    {
        hooks.push_back(Hook(section, placement, text));
    }
    void readHooks(const std::string& file);

    void output(std::ostream& out, const Engine& cp);
};


/**
 * Parser for commandline arguments, with builting help functions.
 */
class StandardParser : public Parser
{
protected:
    std::string m_version;

public:
    StandardParser(const std::string& appname, const std::string& version) :
        Parser(appname), m_version(version)
    {
        helpGroup = addGroup("Help options");
        help = helpGroup->add<BoolOption>("help", 'h', "help", "",
                                          "print commandline help and exit");
        help->addAlias('?');
        this->version = helpGroup->add<BoolOption>("version", 0, "version", "",
                                                   "print the program version and exit");
    }

    void outputHelp(std::ostream& out);

    bool parse(int argc, const char* argv[]);

    OptionGroup* helpGroup;
    BoolOption* help;
    BoolOption* version;
};

/**
 * Parser for commandline arguments, with builting help functions and manpage
 * generation.
 */
class StandardParserWithManpage : public StandardParser
{
protected:
    int m_section;
    std::string m_author;

public:
    StandardParserWithManpage(
        const std::string& appname,
        const std::string& version,
        int section,
        const std::string& author) :
        StandardParser(appname, version),
        m_section(section), m_author(author)
    {
        manpage = helpGroup->add<StringOption>("manpage", 0, "manpage", "[hooks]",
                                               "output the " + name() + " manpage and exit");
    }

    bool parse(int argc, const char* argv[]);

    StringOption* manpage;
};

/**
 * Parser for commandline arguments, with builting help functions and manpage
 * generation, and requiring a mandatory command.
 */
class StandardParserWithMandatoryCommand : public StandardParserWithManpage
{
public:
    StandardParserWithMandatoryCommand(
        const std::string& appname,
        const std::string& version,
        int section,
        const std::string& author) :
        StandardParserWithManpage(appname, version, section, author)
    {
        helpCommand = addEngine("help", "[command]", "print help information",
				"With no arguments, print a summary of available commands.  "
				"If given a command name as argument, print detailed informations "
				"about that command.");
    }

    bool parse(int argc, const char* argv[]);

    Engine* helpCommand;
};

namespace {

typedef std::vector< std::map< std::string, Option * >::const_iterator > PartialMatches;

PartialMatches _partialMatches( std::map< std::string, Option * > longOpts, std::string name )
{
    std::map< std::string, Option * >::const_iterator engine = longOpts.lower_bound( name );
    PartialMatches candidates;
    for ( ; engine != longOpts.end(); ++engine ) {
        if ( engine->first.size() < name.size() )
            break;
        if ( std::equal( name.begin(), name.end(), engine->first.begin() ) )
            candidates.push_back( engine );
        else
            break;
    }
    return candidates;
}

}

bool ArgList::isSwitch(const const_iterator& iter)
{
    return ArgList::isSwitch(*iter);
}

bool ArgList::isSwitch(const iterator& iter)
{
    return ArgList::isSwitch(*iter);
}

bool ArgList::isSwitch(const char* str)
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

bool ArgList::isSwitch(const std::string& str) {
    return ArgList::isSwitch( str.c_str() );
}

void Engine::addWithoutAna(Option* o)
{
    const std::vector<char>& shorts = o->shortNames;
    for (std::vector<char>::const_iterator i = shorts.begin(); i != shorts.end(); i++)
    {
        std::map<char, Option*>::iterator j = m_short.find(*i);
        if (j != m_short.end())
            throw ApiError(std::string("short option ") + *i + " is already mapped to " + j->second->name());
        m_short[*i] = o;
    }

    const std::vector<std::string>& longs = o->longNames;
    for (std::vector<std::string>::const_iterator i = longs.begin(); i != longs.end(); i++)
    {
        std::map<std::string, Option*>::iterator j = m_long.find(*i);
        if (j != m_long.end())
            throw ApiError(std::string("long option ") + *i + " is already mapped to " + j->second->name());
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
    std::map<std::string, Engine*>::iterator a = m_aliases.find(alias);
    if (a != m_aliases.end())
        throw ApiError("command " + alias + " has already been set to " + a->second->name());
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
    for (std::vector<Engine*>::const_iterator i = m_commands.begin();
         i != m_commands.end(); ++i)
    {
        add((*i)->primaryAlias, *i);
        for (std::vector<std::string>::const_iterator j = (*i)->aliases.begin();
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
        std::map<char, Option*>::const_iterator engine = m_short.find(c);
        if (engine == m_short.end())
            return std::make_pair(begin, false);
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
        std::string name, arg;
        bool has_arg;
        if (sep == std::string::npos)
        {
            // No argument
            name = opt.substr(2);
            has_arg = false;
        } else {
            name = opt.substr(2, sep - 2);
            arg = opt.substr(sep + 1);
            has_arg = true;
        }

        std::map<std::string, Option*>::const_iterator engine = m_long.find(name);
        if (engine == m_long.end()) {
            if ( partial_matching ) {
                PartialMatches candidates = _partialMatches( m_long, name );
                if ( candidates.size() == 1 )
                    engine = candidates[ 0 ];
                else
                    return std::make_pair(begin, false);
            } else
                return std::make_pair(begin, false);
        }
        if (has_arg)
            engine->second->parse(arg);
        else
            engine->second->parse_noarg();

        // Remove the parsed element
        list.eraseAndAdvance(begin);
    }
    return make_pair(begin, true);
}

ArgList::iterator Engine::parseKnownSwitches(ArgList& list, ArgList::iterator begin)
{
    // Parse the first items, chopping them off the list, until it works
    while (1)
    {
        if (begin == list.end())
            return begin;
        if (!list.isSwitch(begin))
            break;
        std::pair<ArgList::iterator, bool> res = parseFirstIfKnown(list, begin);
        if (!res.second)
            break;
        begin = res.first;
    }

    // If requested, stop here
    if (no_switches_after_first_arg)
        return begin;

    // Parse the next items, chopping off the list only those that we know
    for (ArgList::iterator cur = begin; cur != list.end(); )
    {
        // Skip non-switches
        if (!list.isSwitch(cur))
        {
            ++cur;
            continue;
        }

        std::pair<ArgList::iterator, bool> res = parseFirstIfKnown(list, cur);
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

void Engine::setPartialMatchingRecursively( bool value ) {
    partial_matching = value;
    for ( std::vector< Engine * >::iterator i = m_commands.begin(); i != m_commands.end(); ++i )
        (*i)->setPartialMatchingRecursively( value );
}


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
            std::map<std::string, Engine*>::iterator a = m_aliases.find(*cmd);
            if (a == m_aliases.end())
                throw BadOption("unknown command " + *cmd);

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
            if (list.isSwitch(i)) {
                std::stringstream ss;
                ss << "unknown option " << *i;
                if ( partial_matching && i->size() > 2 && (*i)[1] == '-' ) {
                    PartialMatches pmatches = _partialMatches( m_long, i->substr( 2 )  );
                    if ( pmatches.size() >= 2 ) {
                        ss << std::endl << "Option is ambiguous, possible candidates are: ";
                        for ( PartialMatches::const_iterator it = pmatches.begin(); it != pmatches.end(); ++it )
                        {
                            ss << (*it)->first << (it + 1 == pmatches.end() ? "" : ", ");
                        }
                    }
                }
                throw BadOption( ss.str() );
            }
            else if (no_switches_after_first_arg)
                // If requested, stop looking for switches
                // after the first non-switch argument
                break;
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

    out << pfx << "Engine " << name() << ": " << std::endl;

    if (!m_commands.empty())
    {
        out << pfx << "   " << m_commands.size() << " commands:" << std::endl;
        for (std::vector<Engine*>::const_iterator i = m_commands.begin();
             i != m_commands.end(); ++i)
            (*i)->dump(out, pfx + "   ");
    }
    if (!m_aliases.empty())
    {
        out << pfx << "   Command parse table:" << std::endl;
        for (std::map<std::string, Engine*>::const_iterator i = m_aliases.begin();
             i != m_aliases.end(); ++i)
            out << pfx << "      " << i->first << " -> " << i->second->name() << std::endl;
    }
    if (!m_groups.empty())
    {
        out << pfx << "   " << m_groups.size() << " OptionGroups:" << std::endl;
        for (std::vector<OptionGroup*>::const_iterator i = m_groups.begin();
             i != m_groups.end(); ++i)
            out << pfx << "      " << (*i)->description << std::endl;
    }
    if (!m_options.empty())
    {
        out << pfx << "   " << m_options.size() << " Options:" << std::endl;
        for (std::vector<Option*>::const_iterator i = m_options.begin();
             i != m_options.end(); ++i)
            out << pfx << "      " << (*i)->fullUsage() << std::endl;
    }
    if (!m_short.empty())
    {
        out << pfx << "   Short options parse table:" << std::endl;
        for (std::map<char, Option*>::const_iterator i = m_short.begin();
             i != m_short.end(); ++i)
            out << pfx << "      " << i->first << " -> " << i->second->fullUsage() << std::endl;
    }
    if (!m_long.empty())
    {
        out << pfx << "   Long options parse table:" << std::endl;
        for (std::map<std::string, Option*>::const_iterator i = m_long.begin();
             i != m_long.end(); ++i)
            out << pfx << "      " << i->first << " -> " << i->second->fullUsage() << std::endl;
    }
}

bool Bool::parse(const std::string& val)
{
    if (val == "true" || val == "t" || val == "1" || val == "yes" || val == "y")
        return true;
    if (val == "false" || val == "f" || val == "0" || val == "no" || val == "n")
        return false;
    throw BadOption("invalid true/false value: \"" + val + "\"");
}

bool Bool::toBool(const bool& val) { return val; }
int Bool::toInt(const value_type& val) { return val ? 1 : 0; }
std::string Bool::toString(const value_type& val) { return val ? "true" : "false"; }
bool Bool::init_val = false;

int Int::parse(const std::string& val)
{
    // Ensure that we're all numeric
    for (std::string::const_iterator s = val.begin(); s != val.end(); ++s)
        if (!isdigit(*s))
            throw BadOption("value " + val + " must be numeric");
    return strtoul(val.c_str(), NULL, 10);
}

bool Int::toBool(const int& val) { return static_cast<bool>(val); }
int Int::toInt(const int& val) { return val; }
std::string Int::toString(const int& val) {
    std::stringstream str;
    str << val; return str.str();
}
int Int::init_val = 0;

std::string String::parse(const std::string& val)
{
    return val;
}
bool String::toBool(const std::string& val) { return !val.empty(); }
int String::toInt(const std::string& val) { return strtoul(val.c_str(), NULL, 10); }
std::string String::toString(const std::string& val) { return val; }
std::string String::init_val = std::string();

#ifdef __unix
std::string ExistingFile::parse(const std::string& val)
{
    if (access(val.c_str(), F_OK) == -1)
        throw BadOption("file " + val + " must exist");
    return val;
}
#endif
std::string ExistingFile::toString(const std::string& val) { return val; }
std::string ExistingFile::init_val = std::string();

namespace {

std::string fmtshort(char c, const std::string& usage)
{
    if (usage.empty())
        return std::string("-") + c;
    else
        return std::string("-") + c + " " + usage;
}

std::string fmtlong(const std::string& c, const std::string& usage, bool optional=false)
{
    if (usage.empty())
        return std::string("--") + c;
    else if (optional)
        return std::string("--") + c + "[=" + usage + "]";
    else
        return std::string("--") + c + "=" + usage;
}

std::string manfmtshort(char c, const std::string& usage)
{
    if (usage.empty())
        return std::string("\\-") + c;
    else
        return std::string("\\-") + c + " \\fI" + usage + "\\fP";
}

std::string manfmtlong(const std::string& c, const std::string& usage, bool optional=false)
{
    if (usage.empty())
        return std::string("\\-\\-") + c;
    else if (optional)
        return std::string("\\-\\-") + c + "[=\\fI" + usage + "\\fP]";
    else
        return std::string("\\-\\-") + c + "=\\fI" + usage + "\\fP";
}

}

Option::Option() : hidden(false) {}

const std::string& Option::fullUsage() const
{
    if (m_fullUsage.empty())
    {
        for (std::vector<char>::const_iterator i = shortNames.begin();
             i != shortNames.end(); i++)
        {
            if (!m_fullUsage.empty())
                m_fullUsage += ", ";
            m_fullUsage += fmtshort(*i, usage);
        }

        for (std::vector<std::string>::const_iterator i = longNames.begin();
             i != longNames.end(); i++)
        {
            if (!m_fullUsage.empty())
                m_fullUsage += ", ";
            m_fullUsage += fmtlong(*i, usage, arg_is_optional());
        }
    }
    return m_fullUsage;
}

std::string Option::fullUsageForMan() const
{
    std::string res;

    for (std::vector<char>::const_iterator i = shortNames.begin();
         i != shortNames.end(); i++)
    {
        if (!res.empty()) res += ", ";
        res += manfmtshort(*i, usage);
    }

    for (std::vector<std::string>::const_iterator i = longNames.begin();
         i != longNames.end(); i++)
    {
        if (!res.empty()) res += ", ";
        res += manfmtlong(*i, usage, arg_is_optional());
    }

    return res;
}

void StandardParser::outputHelp(std::ostream& out)
{
    commandline::Help help(name(), m_version);
    commandline::Engine* e = foundCommand();

    if (e)
        // Help on a specific command
        help.outputHelp(out, *e);
    else
        // General help
        help.outputHelp(out, *this);
}

bool StandardParser::parse(int argc, const char* argv[])
{
    if (Parser::parse(argc, argv))
        return true;

    if (help->boolValue())
    {
        // Provide help as requested
        outputHelp(std::cout);
        return true;
    }
    if (version->boolValue())
    {
        // Print the program version
        commandline::Help help(name(), m_version);
        help.outputVersion(std::cout);
        return true;
    }
    return false;
}

bool StandardParserWithManpage::parse(int argc, const char* argv[])
{
    if (StandardParser::parse(argc, argv))
        return true;
    if (manpage->isSet())
    {
        // Output the manpage
        commandline::Manpage man(name(), m_version, m_section, m_author);
        std::string hooks(manpage->value());
        if (!hooks.empty())
            man.readHooks(hooks);
        man.output(std::cout, *this);
        return true;
    }
    return false;
}

bool StandardParserWithMandatoryCommand::parse(int argc, const char* argv[])
{
    if (StandardParserWithManpage::parse(argc, argv))
        return true;

    if (!foundCommand())
    {
        commandline::Help help(name(), m_version);
        help.outputHelp(std::cout, *this);
        return true;
    }
    if (foundCommand() == helpCommand)
    {
        commandline::Help help(name(), m_version);
        if (hasNext())
        {
            // Help on a specific command
            std::string command = next();
            if (Engine* e = this->command(command))
                help.outputHelp(std::cout, *e);
            else
                throw BadOption("unknown command " + command + "; run '" + argv[0] + " help' "
                                "for a list of all the available commands");
        } else {
            // General help
            help.outputHelp(std::cout, *this);
        }
        return true;
    }
    return false;
}

class HelpWriter
{
    // Width of the console
    std::ostream& out;
    int m_width;

public:
    HelpWriter(std::ostream& out);

    // Write 'size' spaces to out
    void pad(size_t size);

    // Output an item from a list.  The first bulletsize columns will be used to
    // output bullet, the rest will have text, wordwrapped and properly aligned
    void outlist(const std::string& bullet, size_t bulletsize, const std::string& text);

    void outstring(const std::string& str);
};

HelpWriter::HelpWriter(std::ostream& out) : out(out)
{
    char* columns = getenv("COLUMNS");
    m_width = columns ? atoi(columns) : 80;
}

void HelpWriter::pad(size_t size)
{
    for (size_t i = 0; i < size; i++) out << " ";
}

void HelpWriter::outlist(const std::string& bullet, size_t bulletsize, const std::string& text)
{
    string::WordWrap wrapper(text);
    size_t rightcol = m_width - bulletsize;

    out << bullet;
    pad(bulletsize - bullet.size());
    out << wrapper.get(rightcol);
    out << std::endl;

    while (wrapper.hasData())
    {
        pad(bulletsize);
        out << wrapper.get(rightcol);
        out << std::endl;
    }
}

void HelpWriter::outstring(const std::string& str)
{
    string::WordWrap wrapper(str);

    while (wrapper.hasData())
    {
        out << wrapper.get(m_width);
        out << std::endl;
    }
}

void Help::outputOptions(std::ostream& out, HelpWriter& writer, const Engine& p)
{
    // Compute size of option display
    size_t maxLeftCol = 0;
    for (std::vector<OptionGroup*>::const_iterator i = p.groups().begin();
         i != p.groups().end(); i++)
    {
        if ((*i)->hidden) continue;
        for (std::vector<Option*>::const_iterator j = (*i)->options.begin();
             j != (*i)->options.end(); j++)
        {
            if ((*j)->hidden) continue;
            size_t w = (*j)->fullUsage().size();
            if (w > maxLeftCol)
                maxLeftCol = w;
        }
    }
    for (std::vector<Option*>::const_iterator j = p.options().begin();
         j != p.options().end(); j++)
    {
        if ((*j)->hidden) continue;
        size_t w = (*j)->fullUsage().size();
        if (w > maxLeftCol)
            maxLeftCol = w;
    }

    if (maxLeftCol)
    {
        // Output the options
        out << std::endl;
        out << "Options are:" << std::endl;
        for (std::vector<OptionGroup*>::const_iterator i = p.groups().begin();
             i != p.groups().end(); i++)
        {
            if ((*i)->hidden) continue;
            if (!(*i)->description.empty())
            {
                out << std::endl;
                writer.outstring((*i)->description + ":");
                out << std::endl;
            }
            for (std::vector<Option*>::const_iterator j = (*i)->options.begin();
                 j != (*i)->options.end(); j++)
            {
                if ((*j)->hidden) continue;
                writer.outlist(" " + (*j)->fullUsage(), maxLeftCol + 3, (*j)->description);
            }
        }
        if (!p.options().empty())
        {
            out << std::endl;
            writer.outstring("Other options:");
            out << std::endl;
            for (std::vector<Option*>::const_iterator j = p.options().begin();
                 j != p.options().end(); j++)
            {
                if ((*j)->hidden) continue;
                writer.outlist(" " + (*j)->fullUsage(), maxLeftCol + 3, (*j)->description);
            }
        }
    }
}

void Help::outputVersion(std::ostream& out)
{
    out << m_app << " version " << m_ver << std::endl;
}

void Help::outputHelp(std::ostream& out, const Engine& p)
{
    HelpWriter writer(out);

    if (!p.commands().empty())
    {
        // Dig informations from p
        const std::vector<Engine*>& commands = p.commands();

        // Compute the maximum length of alias names
        size_t maxAliasSize = 0;
        for (std::vector<Engine*>::const_iterator i = commands.begin();
             i != commands.end(); i++)
        {
            if ((*i)->hidden) continue;
            const std::string& str = (*i)->primaryAlias;
            if (maxAliasSize < str.size())
                maxAliasSize = str.size();
        }

        out << "Usage: " << m_app << " [options] " << p.usage << std::endl;
        out << std::endl;
        writer.outstring("Description: " + p.description);
        out << std::endl;
        out << "Commands are:" << std::endl;
        out << std::endl;

        // Print the commands
        for (std::vector<Engine*>::const_iterator i = commands.begin();
             i != commands.end(); i++)
        {
            if ((*i)->hidden) continue;
            std::string aliases;
            const std::vector<std::string>& v = (*i)->aliases;
            if (!v.empty())
            {
                aliases += "  May also be invoked as ";
                for (std::vector<std::string>::const_iterator j = v.begin();
                     j != v.end(); j++)
                    if (j == v.begin())
                        aliases += *j;
                    else
                        aliases += " or " + *j;
                aliases += ".";
            }

            writer.outlist(" " + (*i)->primaryAlias, maxAliasSize + 3, (*i)->description + "." + aliases);
        }
    } else {
        // FIXME the || m_app == thing is a workaround...
        if (p.primaryAlias.empty() || m_app == p.primaryAlias)
            out << "Usage: " << m_app << " [options] " << p.usage << std::endl;
        else
            out << "Usage: " << m_app << " [options] " << p.primaryAlias << " [options] " << p.usage << std::endl;
        out << std::endl;

        if (!p.aliases.empty())
        {
            out << "Command aliases: ";
            for (std::vector<std::string>::const_iterator i = p.aliases.begin();
                 i != p.aliases.end(); i++)
                if (i == p.aliases.begin())
                    out << *i;
                else
                    out << ", " << *i;
            out << "." << std::endl;
            out << std::endl;
        }
        writer.outstring("Description: " + p.description);
    }

    if (p.hasOptions())
        outputOptions(out, writer, p);

    out << std::endl;
}

namespace {

std::string toupper(const std::string& str)
{
    std::string res;
    for (size_t i = 0; i < str.size(); i++)
        res += ::toupper(str[i]);
    return res;
}

std::string man_date()
{
    time_t tnow = time(0);
    struct tm* now = gmtime(&tnow);
    char buf[20];
    const char* oldlocale = setlocale(LC_TIME, "C");
    std::string res(buf, strftime(buf, 20, "%B %d, %Y", now));
    setlocale(LC_TIME, oldlocale);
    return res;
}

}

void Manpage::outputParagraph(std::ostream& out, const std::string& str)
{
    for (size_t i = 0; i < str.size(); i++)
        switch (str[i])
        {
            case '-':
                out << "\\-";
                break;
            case '\n':
                out << "\n.br\n";
                break;
            default:
                out << str[i];
        }
    out << '\n';
}

void Manpage::outputOption(std::ostream& out, const Option* o)
{
    out << ".TP" << std::endl;
    out << ".B " << o->fullUsageForMan() << std::endl;
    out << o->description << "." << std::endl;
}

void Manpage::runHooks(std::ostream& out, const std::string& section, where where)
{
    for (std::vector<Hook>::const_iterator i = hooks.begin();
         i != hooks.end(); i++)
        if (i->section == section && i->placement == where)
            out << i->text;
}

void Manpage::startSection(std::ostream& out, const std::string& name)
{
    runHooks(out, name, BEFORE);
    out << ".SH " << name << std::endl;
    runHooks(out, name, BEGINNING);
    lastSection = name;
}

void Manpage::endSection(std::ostream& out)
{
    runHooks(out, lastSection, END);
    lastSection.clear();
}

void Manpage::outputOptions(std::ostream& out, const Engine& p)
{
    for (std::vector<OptionGroup*>::const_iterator i = p.groups().begin();
         i != p.groups().end(); i++)
    {
        if ((*i)->hidden) continue;
        if (!(*i)->description.empty())
            out << std::endl << (*i)->description << ":" << std::endl;
        for (std::vector<Option*>::const_iterator j = (*i)->options.begin();
             j != (*i)->options.end(); ++j)
        {
            if ((*j)->hidden) continue;
            outputOption(out, *j);
        }
        out << ".PP" << std::endl;
    }

    if (!p.options().empty())
    {
        out << std::endl;
        out << "Other options:" << std::endl;
        for (std::vector<Option*>::const_iterator j = p.options().begin();
             j != p.options().end(); ++j)
        {
            if ((*j)->hidden) continue;
            outputOption(out, *j);
        }
    }
}

void Manpage::output(std::ostream& out, const Engine& p)
{
    // Manpage header
    out << ".TH " << toupper(m_app) << " " << m_section << " \"" << man_date() << "\" \"" << m_ver << "\"" << std::endl;

    startSection(out, "NAME");
    out << p.name() << " \\- " << p.description << std::endl;
    endSection(out);

    startSection(out, "SYNOPSIS");
    out << "\\fB" << p.name() << "\\fP [options] " << p.usage << std::endl;
    endSection(out);

    startSection(out, "DESCRIPTION");
    if (!p.longDescription.empty())
        outputParagraph(out, p.longDescription);
    endSection(out);

    if (!p.commands().empty())
    {
        const std::vector<Engine*>& commands = p.commands();

        startSection(out, "COMMANDS");
        out << "\\fB" << p.name() << "\\fP accepts a non-switch argument, that indicates what is the operation that should be performed:" << std::endl;
        for (std::vector<Engine*>::const_iterator i = commands.begin();
             i != commands.end(); i++)
        {
            if ((*i)->hidden) continue;
            out << ".TP" << std::endl;
            out << "\\fB" << (*i)->primaryAlias << "\\fP";

            const std::vector<std::string>& v = (*i)->aliases;
            for (std::vector<std::string>::const_iterator j = v.begin(); j != v.end(); j++)
                out << " or \\fB" << *j << "\\fP";

            out << " " << (*i)->usage << std::endl;
            out << ".br" << std::endl;
            if ((*i)->longDescription.empty())
                outputParagraph(out, (*i)->description);
            else
                outputParagraph(out, (*i)->longDescription);
        }
        endSection(out);
    }

    startSection(out, "OPTIONS");
    out << "This program follows the usual GNU command line syntax, with long options starting with two dashes (`\\-')." << std::endl << std::endl;
    if (!p.commands().empty())
        out << "Every one of the commands listed above has its own set of options.  To keep this manpage readable, all the options are presented together.  Please refer to \"\\fB" << p.name() << "\\fP help \\fIcommand\\fP\" to see which options are accepted by a given command." << std::endl;

    // Output the general options
    outputOptions(out, p);

    // Output group-specific options
    if (!p.commands().empty())
    {
        const std::vector<Engine*>& commands = p.commands();
        for (std::vector<Engine*>::const_iterator i = commands.begin();
             i != commands.end(); i++)
        {
            if ((*i)->hidden) continue;
            out << "\\fBOptions for command " << (*i)->primaryAlias << "\\fP" << std::endl;
            out << ".br" << std::endl;
            outputOptions(out, **i);
        }
    }
    endSection(out);

    startSection(out, "AUTHOR");
    out << "\\fB" << p.name() << "\\fP is maintained by " << m_author << "." << std::endl << std::endl;
    out << "This manpage has been automatically generated by the " << m_app << " program." << std::endl;
    endSection(out);
}

namespace {

std::string readline(FILE* in)
{
    std::string res;
    int c;
    while ((c = getc(in)) != EOF && c != '\n')
        res += c;
    return res;
}

}

void Manpage::readHooks(const std::string& file)
{
    FILE* in = fopen(file.c_str(), "r");
    if (!in) throw std::runtime_error("failed to open " + file + " for reading");
    std::string section;
    commandline::Manpage::where placement = commandline::Manpage::BEFORE;
    std::string text;
    while (!feof(in))
    {
        std::string line(readline(in));
        if (line.empty())
            continue;
        if (line[0] == '|')
        {
            text += line.substr(1) + "\n";
        }
        else if (isalpha(line[0]))
        {
            if (!section.empty())
            {
                addHook(section, placement, text);
                text.clear();
            }
            size_t sep = line.find(' ');
            if (sep == std::string::npos)
            {
                fclose(in);
                throw ApiError("expected two words in line: " + line);
            }
            section = line.substr(0, sep);
            std::string w(line, sep+1);
            if (w == "before")
            {
                placement = commandline::Manpage::BEFORE;
            } else if (w == "beginning") {
                placement = commandline::Manpage::BEGINNING;
            } else if (w == "end") {
                placement = commandline::Manpage::END;
            } else {
                fclose(in);
                throw ApiError("expected 'before', 'beginning' or 'end' in line: " + line);
            }
        }
    }
    if (!section.empty())
        addHook(section, placement, text);
    fclose(in);
}

}
}

namespace brick_test {
namespace commandline {

using namespace ::brick::commandline;

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

class Engine1 : public Public<Engine>
{
    MemoryManager mman;

public:
    Engine1() : Public<Engine>(&mman)
    {
        antani = add<BoolOption>("antani", 'a', "antani");
        blinda = add<StringOption>("blinda", 'b', "blinda");

        antani->addAlias("an-tani");
    }

    BoolOption* antani;
    StringOption* blinda;
};

class Engine2 : public Public<Engine>
{
    MemoryManager mman;

public:
    Engine2() : Public<Engine>(&mman)
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

using std::string;

struct EngineTest {

    TEST(optsAndArgs) {
        ArgList opts;
        opts.push_back("ciaps");
        opts.push_back("-b");
        opts.push_back("cippo");
        opts.push_back("foobar");

        Engine1 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.begin());
        ASSERT_EQ(opts.size(), 2u);
        ASSERT_EQ(string(*opts.begin()), string("ciaps"));
        ASSERT_EQ(string(*opts.rbegin()), string("foobar"));
        ASSERT_EQ(engine.antani->boolValue(), false);
        ASSERT_EQ(engine.blinda->stringValue(), "cippo");
    }

    TEST(noSwitchesAfterFirstArg) {
        ArgList opts;
        opts.push_back("-b");
        opts.push_back("cippo");
        opts.push_back("foobar");
        opts.push_back("--cabal");

        Engine1 engine;
	engine.no_switches_after_first_arg = true;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.begin());
        ASSERT_EQ(opts.size(), 2u);
        ASSERT_EQ(string(*opts.begin()), string("foobar"));
        ASSERT_EQ(string(*opts.rbegin()), string("--cabal"));
        ASSERT_EQ(engine.antani->boolValue(), false);
        ASSERT_EQ(engine.blinda->stringValue(), "cippo");
    }

    TEST(optsOnly) {
        ArgList opts;
        opts.push_back("-a");
        opts.push_back("foobar");

        Engine1 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.begin());
        ASSERT_EQ(opts.size(), 1u);
        ASSERT_EQ(string(*opts.begin()), string("foobar"));
        ASSERT_EQ(engine.antani->boolValue(), true);
        ASSERT_EQ(engine.blinda->boolValue(), false);
    }

    TEST(clusteredShortOpts) {
        ArgList opts;
        opts.push_back("-ab");
        opts.push_back("cippo");

        Engine1 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.end());
        ASSERT_EQ(opts.size(), 0u);
        ASSERT_EQ(engine.antani->boolValue(), true);
        ASSERT_EQ(engine.blinda->stringValue(), "cippo");
    }

    TEST(longOptsWithDashes) {
        ArgList opts;
        opts.push_back("--an-tani");
        opts.push_back("foobar");

        Engine1 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.begin());
        ASSERT_EQ(opts.size(), 1u);
        ASSERT_EQ(string(*opts.begin()), string("foobar"));
        ASSERT_EQ(engine.antani->boolValue(), true);
        ASSERT_EQ(engine.blinda->boolValue(), false);
    }

    TEST(longOptsWithArgs) {
        ArgList opts;
        opts.push_back("--blinda=cippo");
        opts.push_back("foobar");
        opts.push_back("--antani");

        Engine1 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.begin());
        ASSERT_EQ(opts.size(), 1u);
        ASSERT_EQ(string(*opts.begin()), string("foobar"));
        ASSERT_EQ(engine.antani->boolValue(), true);
        ASSERT_EQ(engine.blinda->stringValue(), "cippo");
    }

    TEST(commandWithArg) {
        ArgList opts;
        opts.push_back("--yell=foo");
        opts.push_back("mess");
        opts.push_back("-r");

        Engine2 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.end());
        ASSERT_EQ(opts.size(), 0u);
        ASSERT_EQ(engine.foundCommand(), engine.scramble);
        ASSERT_EQ(engine.scramble_yell->stringValue(), "foo");
        ASSERT_EQ(engine.scramble_random->boolValue(), true);
        ASSERT_EQ(engine.fix_yell->stringValue(), string());
        ASSERT_EQ(engine.fix_quick->boolValue(), false);
        ASSERT_EQ(engine.help->boolValue(), false);
    }

    TEST(commandsWithOverlappingArgs) {
        ArgList opts;
        opts.push_back("--yell=foo");
        opts.push_back("fix");
        opts.push_back("--help");
        opts.push_back("-Q");

        Engine2 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.end());
        ASSERT_EQ(opts.size(), 0u);
        ASSERT_EQ(engine.foundCommand(), engine.fix);
        ASSERT_EQ(engine.scramble_yell->stringValue(), string());
        ASSERT_EQ(engine.scramble_random->boolValue(), false);
        ASSERT_EQ(engine.fix_yell->stringValue(), "foo");
        ASSERT_EQ(engine.fix_quick->boolValue(), true);
        ASSERT_EQ(engine.help->boolValue(), true);
    }

    TEST(commandsWithoutCommand) {
        ArgList opts;
        opts.push_back("--help");

        Engine2 engine;
        ArgList::iterator i = engine.parseList(opts);
        ASSERT(i == opts.end());
        ASSERT_EQ(opts.size(), 0u);
        ASSERT_EQ(engine.foundCommand(), static_cast<Engine*>(0));
        ASSERT_EQ(engine.scramble_yell->stringValue(), string());
        ASSERT_EQ(engine.scramble_random->boolValue(), false);
        ASSERT_EQ(engine.fix_yell->stringValue(), string());
        ASSERT_EQ(engine.fix_quick->boolValue(), false);
        ASSERT_EQ(engine.help->boolValue(), true);
    }

    TEST(creationShortcuts) {
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
        ASSERT(i == opts.end());
        ASSERT_EQ(opts.size(), 0u);
        ASSERT_EQ(testBool->boolValue(), true);
        ASSERT_EQ(testInt->intValue(), 3);
        ASSERT_EQ(testString->stringValue(), "antani");
        ASSERT_EQ(testBool1->boolValue(), true);
        ASSERT_EQ(testInt1->intValue(), 5);
        ASSERT_EQ(testString1->stringValue(), "blinda");
    }

};

}
}

#endif

#ifdef BRICK_DEMO

using namespace brick::commandline;
using namespace std;

int withCommands(int argc, const char* argv[])
{
    // Main parser
    StandardParserWithMandatoryCommand parser(argv[0], "0.1", 1, "enrico@enricozini.org");
    parser.usage = "<command> [options and arguments]";
    parser.description = "Demo commandline parser";
    parser.longDescription = 
        "This program is a demo for the commandline parser.  It shows a parser with "
        "subcommands and various kinds of options.";

    // Grep subcommand
    Engine* grep = parser.addEngine("grep", "<pattern> [files...]",
                                    "Print lines matching the pattern",
                                    "Print all the lines of standard input or the given files that match "
                                    "the given pattern.");
    BoolOption* grep_invert = grep->add<BoolOption>("invert", 'v', "invert", "",
                                                    "invert the match");
    StringOption* grep_output = grep->add<StringOption>("output", 'o', "output", "<file>",
                                                        "write the output to the given file instead of standard output");

    // ls subcommand
    Engine* ls = parser.addEngine("ls", "[directory...]",
                                  "List files in a directory",
                                  "List all files found in the directories given as parameters to standard output.  "
                                  "if no directory is given, list the files in the current directory.");
    // sort option group
    OptionGroup* ls_sort = ls->addGroup("Options controlling the order of output");
    BoolOption* ls_sort_invert = ls_sort->add<BoolOption>("invert", 'r', "invert", "",
                                                          "sort in inverted order");
    IntOption* ls_sort_field = ls_sort->add<IntOption>("field", 0, "field", "",
                                                       "sort the given field (if the switch is omitted, 1 is assumed");
    // format option group
    OptionGroup* ls_format = ls->addGroup("Options controlling the format of output");
    BoolOption* ls_format_long = ls_format->add<BoolOption>("long", 'l', "long", "",
                                                            "long output format with all the details");
    BoolOption* ls_format_inode = ls_format->add<BoolOption>("inode", 'i', "inode", "",
                                                             "also output the file inode number");
    // other ls options
    BoolOption* ls_all = ls->add<BoolOption>("all", 'a', "all", "",
                                             "output all files, including the ones starting with a dot");

    try {
        if (parser.parse(argc, argv))
            cerr << "The parser handled the command for us." << endl;
        if (parser.foundCommand())
            cerr << "Selected command: " << parser.foundCommand()->name() << endl;
        else
            cerr << "No command selected." << endl;
        cerr << "Option values:" << endl;
        cerr << "  help: " << parser.help->boolValue() << endl;
        cerr << "  version: " << parser.version->boolValue() << endl;
        cerr << "  manpage: " << parser.manpage->boolValue() << endl;
        cerr << "  grep/invert: " << grep_invert->boolValue() << endl;
        cerr << "  grep/output: " << grep_output->stringValue() << endl;
        cerr << "  ls/sort/invert: " << ls_sort_invert->boolValue() << endl;
        cerr << "  ls/sort/field: " << ls_sort_field->intValue() << endl;
        cerr << "  ls/format/long: " << ls_format_long->boolValue() << endl;
        cerr << "  ls/format/inode: " << ls_format_inode->boolValue() << endl;
        cerr << "  ls/all: " << ls_all->boolValue() << endl;
        return 0;
    } catch (BadOption& e) {
        cerr << e.what() << endl;
        parser.outputHelp(cerr);
        return 1;
    }
    return 1;
}

void usage(ostream& out, const string& argv0)
{
    out << "Usage: " << argv0 << " {commands|switches}" << std::endl;
}

int main(int argc, const char* argv[])
{
    try {
        if (argc == 1)
        {
            usage(cout, argv[0]);
            return 0;
        }
        if (string(argv[1]) == "commands")
            return withCommands(argc - 1, argv + 1);
        //else if (string(argv[1]) == "switches")
        //return withoutCommands(argc - 1, argv + 1);
        else {
            usage(cerr, argv[0]);
            return 1;
        }
        return 0;
    } catch (std::exception& e) {
        cerr << e.what() << std::endl;
        return 1;
    }
}

#endif

// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
