/*
 * OO wrapper for regular expression functions
 *
 * Copyright (C) 2003--2006  Enrico Zini <enrico@debian.org>
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

#include <wibble/string.h>
#include <stack>
#include <cstdio>
#include <cstdlib>

using namespace std;

namespace wibble {
namespace str {

std::string normpath(const std::string& pathname)
{
	stack<string> st;
	if (pathname[0] == '/')
		st.push("/");
	Split splitter("/", pathname);
	for (Split::const_iterator i = splitter.begin(); i != splitter.end(); ++i)
	{
		if (*i == "." || i->empty()) continue;
		if (*i == "..")
			if (st.top() == "..")
				st.push(*i);
			else if (st.top() == "/")	
				continue;
			else
				st.pop();
		else
			st.push(*i);
	}
	if (st.empty())
		return ".";
	string res = st.top();
	for (st.pop(); !st.empty(); st.pop())
		res = joinpath(st.top(), res);
	return res;
}

std::string urlencode(const std::string& str)
{
	string res;
	for (string::const_iterator i = str.begin(); i != str.end(); ++i)
	{
		if ( (*i >= '0' && *i <= '9') || (*i >= 'A' && *i <= 'Z')
		  || (*i >= 'a' && *i <= 'z') || *i == '-' || *i == '_'
		  || *i == '!' || *i == '*' || *i == '\'' || *i == '(' || *i == ')')
			res += *i;
		else {
			char buf[4];
			snprintf(buf, 4, "%%%02x", (unsigned)(unsigned char)*i);
			res += buf;
		}
	}
	return res;
}

std::string urldecode(const std::string& str)
{
	string res;
	for (size_t i = 0; i < str.size(); ++i)
	{
		if (str[i] == '%')
		{
			// If there's a partial %something at the end, ignore it
			if (i >= str.size() - 2)
				return res;
			res += (char)strtoul(str.substr(i+1, 2).c_str(), 0, 16);
			i += 2;
		}
		else
			res += str[i];
	}
	return res;
}

}
}

// vim:set ts=4 sw=4:
