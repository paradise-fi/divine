/*
 * OO wrapper for regular expression functions
 *
 * Copyright (C) 2003--2008  Enrico Zini <enrico@debian.org>
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
#include <wibble/exception.h>
#include <stack>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

using namespace std;

namespace wibble {
namespace str {

#ifdef _WIN32
int vasprintf (char **result, const char *format, va_list args)
{
  const char *p = format;
  /* Add one to make sure that it is never zero, which might cause malloc
     to return NULL.  */
  int total_width = strlen (format) + 1;
  va_list ap;

  memcpy ((void *)&ap, (void *)&args, sizeof (va_list));

  while (*p != '\0')
    {
      if (*p++ == '%')
	{
	  while (strchr ("-+ #0", *p))
	    ++p;
	  if (*p == '*')
	    {
	      ++p;
	      total_width += abs (va_arg (ap, int));
	    }
	  else
	    total_width += strtoul (p, (char **) &p, 10);
	  if (*p == '.')
	    {
	      ++p;
	      if (*p == '*')
		{
		  ++p;
		  total_width += abs (va_arg (ap, int));
		}
	      else
	      total_width += strtoul (p, (char **) &p, 10);
	    }
	  while (strchr ("hlL", *p))
	    ++p;
	  /* Should be big enough for any format specifier except %s and floats.  */
	  total_width += 30;
	  switch (*p)
	    {
	    case 'd':
	    case 'i':
	    case 'o':
	    case 'u':
	    case 'x':
	    case 'X':
	    case 'c':
	      (void) va_arg (ap, int);
	      break;
	    case 'f':
	    case 'e':
	    case 'E':
	    case 'g':
	    case 'G':
	      (void) va_arg (ap, double);
	      /* Since an ieee double can have an exponent of 307, we'll
		 make the buffer wide enough to cover the gross case. */
	      total_width += 307;
	      break;
	    case 's':
	      total_width += strlen (va_arg (ap, char *));
	      break;
	    case 'p':
	    case 'n':
	      (void) va_arg (ap, char *);
	      break;
	    }
	  p++;
	}
    }
  *result = (char*)malloc (total_width);
  if (*result != NULL) {
    return vsprintf (*result, format, args);}
  else {
    return 0;
  }
}
#endif

std::string fmtf( const char* f, ... ) {
    char *c;
    va_list ap;
    va_start( ap, f );
    vasprintf( &c, f, ap );
    std::string ret( c );
    free( c );
    return ret;
}

std::string fmt( const char* f, ... ) {
    char *c;
    va_list ap;
    va_start( ap, f );
    vasprintf( &c, f, ap );
    std::string ret( c );
    free( c );
    return ret;
}

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
	if ((unsigned)idx > 43 + (sizeof(data)/sizeof(data[0]))) return 0;
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

static std::string stripYamlComment(const std::string& str)
{
	std::string res;
	for (string::const_iterator i = str.begin(); i != str.end(); ++i)
	{
		if (*i == '#')
			break;
		res += *i;
	}
	// Remove trailing spaces
	while (!res.empty() && ::isspace(res[res.size() - 1]))
		res.resize(res.size() - 1);
	return res;
}

YamlStream::const_iterator::const_iterator(std::istream& sin)
	: in(&sin)
{
	// Read the next line to parse, skipping leading empty lines
	while (getline(*in, line))
	{
		line = stripYamlComment(line);
		if (!line.empty())
			break;
	}

	if (line.empty() && in->eof())
		// If we reached EOF without reading anything, become the end iterator
		in = 0;
	else
		// Else do the parsing
		++*this;
}

YamlStream::const_iterator& YamlStream::const_iterator::operator++()
{
	// Reset the data
	value.first.clear();
	value.second.clear();

	// If the lookahead line is empty, then we've reached the end of the
	// record, and we become the end iterator
	if (line.empty())
	{
		in = 0;
		return *this;
	}

	if (line[0] == ' ')
		throw wibble::exception::Consistency("parsing yaml line \"" + line + "\"",
				"field continuation found, but no field has started");

	// Field start
	size_t pos = line.find(':');
	if (pos == string::npos)
		throw wibble::exception::Consistency("parsing Yaml line \"" + line + "\"",
				"every line that does not start with spaces must have a semicolon");

	// Get the field name
	value.first = line.substr(0, pos);

	// Skip leading spaces in the value
	for (++pos; pos < line.size() && line[pos] == ' '; ++pos)
		;

	// Get the (start of the) field value
	value.second = line.substr(pos);

	// Look for continuation lines, also preparing the lookahead line
	size_t indent = 0;
	while (true)
	{
		line.clear();
		if (in->eof()) break;
		if (!getline(*in, line)) break;
		// End of record
		if (line.empty()) break;
		// Full comment line: ignore it
		if (line[0] == '#') continue;
		// New field or empty line with comment
		if (line[0] != ' ')
		{
			line = stripYamlComment(line);
			break;
		}

		// Continuation line

		// See how much we are indented
		size_t this_indent;
		for (this_indent = 0; this_indent < line.size() && line[this_indent] == ' '; ++this_indent)
			;

		if (indent == 0)
		{
			indent = this_indent;
			// If it's the first continuation line, and there was content right
			// after the field name, add a \n to it
			if (!value.second.empty())
				value.second += '\n';
		}

		if (this_indent > indent)
			// If we're indented the same or more than the first line, deindent
			// by the amount of indentation found in the first line
			value.second += line.substr(indent);
		else
			// Else, the line is indented less than the first line, just remove
			// all leading spaces.  Ugly, but it's been encoded in an ugly way.
			value.second += line.substr(this_indent);
		value.second += '\n';
	}

	return *this;
}


}
}

// vim:set ts=4 sw=4:
