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

static const char* base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

template<typename T>
static const char invbase64(const T& idx)
{
	static const char data[] = {62,0,0,0,63,52,53,54,55,56,57,58,59,60,61,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
	if (idx < 43) return 0;
	if (idx > 43 + (sizeof(data)/sizeof(data[0]))) return 0;
	return data[idx - 43];
}

std::string encodeBase64(const std::string& str)
{
	std::string res;

	for (size_t i = 0; i < str.size(); i += 3)
	{
		// Pack every triplet into 24 bits
		unsigned int enc;
		if (i + 3 < str.size())
			enc = (str[i] << 16) + (str[i+1] << 8) + (str[i+2]);
		else
		{
			enc = (str[i] << 16);
			if (i + 1 < str.size())
				enc += str[i+1] << 8;
			if (i + 2 < str.size())
				enc += str[i+2];
		}

		// Divide in 4 6-bit values and use them as indexes in the base64 char
		// array
		for (int j = 3; j >= 0; --j)
			res += base64[(enc >> (j*6)) & 63];
	}

	// Replace padding characters with '='
	if (str.size() % 3)
		for (size_t i = 0; i < 3 - (str.size() % 3); ++i)
			res[res.size() - i - 1] = '=';
	
	return res;
}

std::string decodeBase64(const std::string& str)
{
	std::string res;

	for (size_t i = 0; i < str.size(); i += 4)
	{
		// Pack every quadruplet into 24 bits
		unsigned int enc;
		if (i+4 < str.size())
		{
			enc = (invbase64(str[i]) << 18)
			    + (invbase64(str[i+1]) << 12)
				+ (invbase64(str[i+2]) << 6)
				+ (invbase64(str[i+3]));
		} else {
			enc = (invbase64(str[i]) << 18);
			if (i+1 < str.size())
				enc += (invbase64(str[i+1]) << 12);
			if (i+2 < str.size())
				enc += (invbase64(str[i+2]) << 6);
			if (i+3 < str.size())
				enc += (invbase64(str[i+3]));
		}

		// Divide in 3 8-bit values and append them to the result
		res += enc >> 16 & 0xff;
		res += enc >> 8 & 0xff;
		res += enc & 0xff;
	}

	// Remove trailing padding
	for (size_t i = str.size() - 1; i >= 0 && str[i] == '='; --i)
		res.resize(res.size() - 1);

	return res;
}


}
}

// vim:set ts=4 sw=4:
