#include <wibble/config.h>
#include <wibble/commandline/options.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <set>
#include <sstream>

using namespace std;

namespace wibble {
namespace commandline {

int Option::intValue() const
{
	return strtoul(stringValue().c_str(), NULL, 10);
}

static string fmtshort(char c, const std::string& usage)
{
	if (usage.empty())
		return string("-") + c;
	else
		return string("-") + c + " " + usage;
}

static string fmtlong(const std::string& c, const std::string& usage)
{
	if (usage.empty())
		return string("--") + c;
	else
		return string("--") + c + "=" + usage;
}

static string manfmtshort(char c, const std::string& usage)
{
	if (usage.empty())
		return string("\\-") + c;
	else
		return string("\\-") + c + " \\fI" + usage + "\\fP";
}

static string manfmtlong(const std::string& c, const std::string& usage)
{
	if (usage.empty())
		return string("\\-\\-") + c;
	else
		return string("\\-\\-") + c + "=\\fI" + usage + "\\fP";
}

const std::string& Option::fullUsage() const
{
	if (m_fullUsage.empty())
	{
		for (vector<char>::const_iterator i = shortNames.begin();
				i != shortNames.end(); i++)
		{
			if (!m_fullUsage.empty())
				m_fullUsage += ", ";
			m_fullUsage += fmtshort(*i, usage);
		}

		for (vector<string>::const_iterator i = longNames.begin();
				i != longNames.end(); i++)
		{
			if (!m_fullUsage.empty())
				m_fullUsage += ", ";
			m_fullUsage += fmtlong(*i, usage);
		}
	}
	return m_fullUsage;
}
	
std::string Option::fullUsageForMan() const
{
	string res;

	for (vector<char>::const_iterator i = shortNames.begin();
			i != shortNames.end(); i++)
	{
		if (!res.empty()) res += ", ";
		res += manfmtshort(*i, usage);
	}

	for (vector<string>::const_iterator i = longNames.begin();
			i != longNames.end(); i++)
	{
		if (!res.empty()) res += ", ";
		res += manfmtlong(*i, usage);
	}

	return res;
}
	
ArgList::iterator StringOption::parse(ArgList& list, ArgList::iterator begin)
{
	if (begin == list.end())
		throw exception::BadOption("no string argument found");
	m_value = *begin;
	// Remove the parsed element
	return list.eraseAndAdvance(begin);
}
bool StringOption::parse(const std::string& param)
{
	m_value = param;
	return true;
}

std::string IntOption::stringValue() const
{
	stringstream str;
	str << m_value;
	return str.str();
}

ArgList::iterator IntOption::parse(ArgList& list, ArgList::iterator begin)
{
	if (begin == list.end())
		throw exception::BadOption("no numeric argument found");
	// Ensure that we're all numeric
	for (string::const_iterator s = begin->begin(); s != begin->end(); ++s)
		if (!isdigit(*s))
			throw exception::BadOption("value " + *begin + " must be numeric");
	m_value = strtoul(begin->c_str(), 0, 10);
	// Remove the parsed element
	return list.eraseAndAdvance(begin);
}
bool IntOption::parse(const std::string& param)
{
	m_value = strtoul(param.c_str(), 0, 10);
	return true;
}


ArgList::iterator ExistingFileOption::parse(ArgList& list, ArgList::iterator begin)
{
	if (begin == list.end())
		throw exception::BadOption("no file argument found");
	if (access(begin->c_str(), F_OK) == -1)
		throw exception::BadOption("file " + *begin + " must exist");
	m_value = *begin;

	// Remove the parsed element
	return list.eraseAndAdvance(begin);
}
bool ExistingFileOption::parse(const std::string& param)
{
	if (param.empty())
		throw exception::BadOption("no file argument found");
	if (access(param.c_str(), F_OK) == -1)
		throw exception::BadOption("file " + param + " must exist");
	m_value = param;
	return true;
}

}
}


#ifdef WIBBLE_COMPILE_TESTSUITE
#include <wibble/tests.h>

namespace wibble {
namespace tut {

struct commandline_options_shar {
};
TESTGRP( commandline_options );

using namespace commandline;

// Test BoolOption
template<> template<>
void to::test<1>()
{
	BoolOption opt("test");

	ensure_equals(opt.name(), string("test"));
	ensure_equals(opt.boolValue(), false);
	ensure_equals(opt.stringValue(), string("false"));

	ensure_equals(opt.parse(string()), false);
	ensure_equals(opt.boolValue(), true);
	ensure_equals(opt.stringValue(), string("true"));
}

// Test StringOption
template<> template<>
void to::test<2>()
{
	StringOption opt("test");

	ensure_equals(opt.name(), string("test"));
	ensure_equals(opt.boolValue(), false);
	ensure_equals(opt.stringValue(), string());

	ensure_equals(opt.parse("-a"), true);
	ensure_equals(opt.boolValue(), true);
	ensure_equals(opt.stringValue(), "-a");
}

}
}

#endif

// vim:set ts=4 sw=4:
