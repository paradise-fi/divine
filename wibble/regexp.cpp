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

#include <wibble/regexp.h>
#include <wibble/exception.tcc>

using namespace std;

namespace wibble {
namespace exception {

////// wibble::exception::Regexp

Regexp::Regexp(const regex_t& re, int code,
				const string& context) throw ()
			: Generic(context), m_code(code)
{
	int size = 64;
	char* msg = new char[size];
	int nsize = regerror(code, &re, msg, size);
	if (nsize > size)
	{
		delete[] msg;
		msg = new char[nsize];
		regerror(code, &re, msg, nsize);
	}
	m_message = msg;
	delete[] msg;
}

}

////// Regexp

Regexp::Regexp(const string& expr, int match_count, int flags)
	throw (wibble::exception::Regexp) : pmatch(0), nmatch(match_count)
{
	if (match_count == 0)
		flags |= REG_NOSUB;

	int res = regcomp(&re, expr.c_str(), flags);
	if (res)
		throw wibble::exception::Regexp(re, res, "Compiling regexp \"" + expr + "\"");

	if (match_count > 0)
		pmatch = new regmatch_t[match_count];
}

Regexp::~Regexp() throw ()
{
	regfree(&re);
	if (pmatch)
		delete[] pmatch;
}
	

bool Regexp::match(const string& str, int flags)
	throw(wibble::exception::Regexp)
{
	int res;
	
	if (nmatch)
	{
		res = regexec(&re, str.c_str(), nmatch, pmatch, flags);
		lastMatch = str;
	}
	else
		res = regexec(&re, str.c_str(), 0, 0, flags);

	switch (res)
	{
		case 0:	return true;
		case REG_NOMATCH: return false;
		default: throw wibble::exception::Regexp(re, res, "Matching string \"" + str + "\"");
	}
}

string Regexp::operator[](int idx) throw (wibble::exception::OutOfRange)
{
	if (idx > nmatch)
		throw wibble::exception::ValOutOfRange<int>("index", idx, 0, nmatch, "getting submatch of regexp");

	if (pmatch[idx].rm_so == -1)
		return string();
	
	return string(lastMatch, pmatch[idx].rm_so, pmatch[idx].rm_eo - pmatch[idx].rm_so);
}

size_t Regexp::matchStart(int idx) throw (wibble::exception::OutOfRange)
{
	if (idx > nmatch)
		throw wibble::exception::ValOutOfRange<int>("index", idx, 0, nmatch, "getting submatch of regexp");
	return pmatch[idx].rm_so;
}

size_t Regexp::matchEnd(int idx) throw (wibble::exception::OutOfRange)
{
	if (idx > nmatch)
		throw wibble::exception::ValOutOfRange<int>("index", idx, 0, nmatch, "getting submatch of regexp");
	return pmatch[idx].rm_eo;
}

size_t Regexp::matchLength(int idx) throw (wibble::exception::OutOfRange)
{
	if (idx > nmatch)
		throw wibble::exception::ValOutOfRange<int>("index", idx, 0, nmatch, "getting submatch of regexp");
	return pmatch[idx].rm_eo - pmatch[idx].rm_so;
}

Tokenizer::const_iterator& Tokenizer::const_iterator::operator++()
{
	// Skip past the last token
	beg = end;

	if (tok.re.match(tok.str.substr(beg)))
	{
		beg += tok.re.matchStart(0);
		end = beg + tok.re.matchLength(0);
	} 
	else
		beg = end = tok.str.size();

	return *this;
}

Splitter::const_iterator& Splitter::const_iterator::operator++()
{
	if (re.match(next))
	{
		if (re.matchLength(0))
		{
			cur = next.substr(0, re.matchStart(0));
			next = next.substr(re.matchStart(0) + re.matchLength(0));
		}
		else
		{
			if (!next.empty())
			{
				cur = next.substr(0, 1);
				next = next.substr(1);
			} else {
				cur = next;
			}
		}
	} else {
		cur = next;
		next = string();
	}
	return *this;
}

}

// vim:set ts=4 sw=4:
