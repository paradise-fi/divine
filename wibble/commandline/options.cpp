#include <wibble/sys/macros.h>
#include <wibble/commandline/options.h>
#include <wibble/string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <cstdlib>
#include <set>
#include <sstream>

using namespace std;

namespace wibble {
namespace commandline {

bool Bool::parse(const std::string& val)
{
	if (val == "true" || val == "t" || val == "1" || val == "yes" || val == "y")
		return true;
	if (val == "false" || val == "f" || val == "0" || val == "no" || val == "n")
		return false;
	throw exception::BadOption("invalid true/false value: \"" + val + "\"");
}
bool Bool::toBool(const bool& val) { return val; }
int Bool::toInt(const value_type& val) { return val ? 1 : 0; }
std::string Bool::toString(const value_type& val) { return val ? "true" : "false"; }
bool Bool::init_val = false;

int Int::parse(const std::string& val)
{
	// Ensure that we're all numeric
	for (string::const_iterator s = val.begin(); s != val.end(); ++s)
		if (!isdigit(*s))
			throw exception::BadOption("value " + val + " must be numeric");
	return strtoul(val.c_str(), NULL, 10);
}
bool Int::toBool(const int& val) { return static_cast<bool>(val); }
int Int::toInt(const int& val) { return val; }
std::string Int::toString(const int& val) { return str::fmt(val); }
int Int::init_val = 0;

std::string String::parse(const std::string& val)
{
	return val;
}
bool String::toBool(const std::string& val) { return !val.empty(); }
int String::toInt(const std::string& val) { return strtoul(val.c_str(), NULL, 10); }
std::string String::toString(const std::string& val) { return val; }
std::string String::init_val = std::string();

#ifdef POSIX
std::string ExistingFile::parse(const std::string& val)
{
	if (access(val.c_str(), F_OK) == -1)
		throw exception::BadOption("file " + val + " must exist");
	return val;
}
#endif
std::string ExistingFile::toString(const std::string& val) { return val; }
std::string ExistingFile::init_val = std::string();

static string fmtshort(char c, const std::string& usage)
{
	if (usage.empty())
		return string("-") + c;
	else
		return string("-") + c + " " + usage;
}

static string fmtlong(const std::string& c, const std::string& usage, bool optional=false)
{
    if (usage.empty())
        return string("--") + c;
    else if (optional)
        return string("--") + c + "[=" + usage + "]";
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

static string manfmtlong(const std::string& c, const std::string& usage, bool optional=false)
{
    if (usage.empty())
        return string("\\-\\-") + c;
    else if (optional)
        return string("\\-\\-") + c + "[=\\fI" + usage + "\\fP]";
    else
        return string("\\-\\-") + c + "=\\fI" + usage + "\\fP";
}

Option::Option() : hidden(false) {}

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
			m_fullUsage += fmtlong(*i, usage, arg_is_optional());
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
		res += manfmtlong(*i, usage, arg_is_optional());
	}

	return res;
}

}
}

// vim:set ts=4 sw=4:
