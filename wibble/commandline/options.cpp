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
	

bool StringOption::parse(const char* str)
{
	if (!str) throw exception::BadOption("no string argument found");
	m_value = str;
	return true;
}

std::string IntOption::stringValue() const
{
	stringstream str;
	str << m_value;
	return str.str();
}

bool IntOption::parse(const char* str)
{
	if (!str) throw exception::BadOption("no int argument found");
	// Ensure that we're all numeric
	for (const char* s = str; *s; ++s)
		if (!isdigit(*s))
			throw exception::BadOption(string("value ") + str + " must be numeric");
	m_value = strtoul(str, 0, 10);
	return true;
}

bool ExistingFileOption::parse(const char* str)
{
	if (!str) throw exception::BadOption("no file argument found");
	if (access(str, F_OK) == -1)
		throw exception::BadOption(string("file ") + str + " must exist");
	m_value = str;
	return true;
}

}
}


#ifdef COMPILE_TESTSUITE

#include <wibble/tests.h>

namespace tut {

struct wibble_commandline_options_shar {
};
TESTGRP(wibble_commandline_options);

using namespace wibble::commandline;

// Test BoolOption
template<> template<>
void to::test<1>()
{
	BoolOption opt("test");

	ensure_equals(opt.name(), string("test"));
	ensure_equals(opt.boolValue(), false);
	ensure_equals(opt.stringValue(), string("false"));

	ensure_equals(opt.parse(0), false);
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

#endif

// vim:set ts=4 sw=4:
