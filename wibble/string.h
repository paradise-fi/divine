// -*- C++ -*-
#ifndef WIBBLE_STRING_H
#define WIBBLE_STRING_H

/*
 * Various string functions
 *
 * Copyright (C) 2007,2008  Enrico Zini <enrico@debian.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <string>
#include <sstream>
#include <cctype>

namespace wibble {
namespace str {

/// Format any value into a string using a std::stringstream
template<typename T>
inline std::string fmt(const T& val)
{
    std::stringstream str;
    str << val;
    return str.str();
}
template<> inline std::string fmt<std::string>(const std::string& val) { return val; }
template<> inline std::string fmt<char*>(char * const & val) { return val; }

/// Given a pathname, return the file name without its path
inline std::string basename(const std::string& pathname)
{
	size_t pos = pathname.rfind("/");
	if (pos == std::string::npos)
		return pathname;
	else
		return pathname.substr(pos+1);
}

/// Given a pathname, return the directory name without the file name
inline std::string dirname(const std::string& pathname)
{
	size_t pos = pathname.rfind("/");
	if (pos == std::string::npos)
		return std::string();
	else if (pos == 0)
		// Handle the case of '/foo'
		return std::string("/");
	else
		return pathname.substr(0, pos);
}

/// Check if a string starts with the given substring
inline bool startsWith(const std::string& str, const std::string& part)
{
	if (str.size() < part.size())
		return false;
	return str.substr(0, part.size()) == part;
}

/// Check if a string ends with the given substring
inline bool endsWith(const std::string& str, const std::string& part)
{
	if (str.size() < part.size())
		return false;
	return str.substr(str.size() - part.size()) == part;
}

#if ! __GNUC__ || __GNUC__ >= 4
/**
 * Return the substring of 'str' without all leading and trailing characters
 * for which 'classifier' returns true.
 */
template<typename FUN>
inline std::string trim(const std::string& str, const FUN& classifier)
{
	if (str.empty())
		return str;

	size_t beg = 0;
	size_t end = str.size() - 1;
	while (beg < end && classifier(str[beg]))
		++beg;
	while (end >= beg && classifier(str[end]))
		--end;

	return str.substr(beg, end-beg+1);
}

/**
 * Return the substring of 'str' without all leading and trailing spaces.
 */
inline std::string trim(const std::string& str)
{
    return trim(str, ::isspace);
}
#else
/// Workaround version for older gcc
inline std::string trim(const std::string& str)
{
	if (str.empty())
		return str;

	size_t beg = 0;
	size_t end = str.size() - 1;
	while (beg < end && ::isspace(str[beg]))
		++beg;
	while (end >= beg && ::isspace(str[end]))
		--end;

	return str.substr(beg, end-beg+1);
}
#endif

/// Convert a string to uppercase
inline std::string toupper(const std::string& str)
{
	std::string res;
	res.reserve(str.size());
	for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
		res += ::toupper(*i);
	return res;
}

/// Convert a string to lowercase
inline std::string tolower(const std::string& str)
{
	std::string res;
	res.reserve(str.size());
	for (std::string::const_iterator i = str.begin(); i != str.end(); ++i)
		res += ::tolower(*i);
	return res;
}

/// Join two paths, adding slashes when appropriate
inline std::string joinpath(const std::string& path1, const std::string& path2)
{
	if (path1.empty())
		return path2;
	if (path2.empty())
		return path1;

	if (path1[path1.size() - 1] == '/')
		if (path2[0] == '/')
			return path1 + path2.substr(1);
		else
			return path1 + path2;
	else
		if (path2[0] == '/')
			return path1 + path2;
		else
			return path1 + '/' + path2;
}

/// Urlencode a string
std::string urlencode(const std::string& str);

/// Decode an urlencoded string
std::string urldecode(const std::string& str);

}
}

// vim:set ts=4 sw=4:
#endif
