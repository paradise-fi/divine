// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * String utilities.
 */

/*
 * (c) 2007 Enrico Zini <enrico@enricozini.org>
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

#include <string>
#include <cctype>
#include <cstring>
#include <cstdarg>
#include <cstdlib>

#include <regex.h>

#include <brick-unittest.h>

#ifndef BRICK_STRING_H
#define BRICK_STRING_H

namespace brick {
namespace string {

#if _WIN32 || __xlC__
namespace {

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

}
#endif

namespace {

std::string fmtf( const char* f, ... ) {
    char *c;
    va_list ap;
    va_start( ap, f );
    vasprintf( &c, f, ap );
    std::string ret( c );
    free( c );
    return ret;
}

/// Format any value into a string using a std::stringstream
template< typename T >
inline std::string fmt(const T& val)
{
    std::stringstream str;
    str << val;
    return str.str();
}

template< typename C >
inline std::string fmt_container( const C &c, char f, char l )
{
    std::string s;
    s += f;
    if ( c.empty() )
        return s + l;

    s += ' ';
    for ( typename C::const_iterator i = c.begin(); i != c.end(); ++i ) {
        s += fmt( *i );
        if ( i != c.end() && i + 1 != c.end() )
            s += ", ";
    }
    s += ' ';
    s += l;
    return s;
}

}

/**
 * Simple string wrapper.
 *
 * wibble::text::Wrap takes a string and splits it in lines of the give length
 * with proper word wrapping.
 *
 * Example:
 * \code
 * WordWrap ww("The quick brown fox jumps over the lazy dog");
 * ww.get(5);  // Returns "The"
 * ww.get(14); // Returns "quick brown"
 * ww.get(3);  // Returns "fox"
 * // A line always gets some text, to avoid looping indefinitely in case of
 * // words that are too long.  Words that are too long are split crudely.
 * ww.get(2);  // Returns "ju"
 * ww.get(90); // Returns "mps over the lazy dog"
 * \endcode
 */
class WordWrap
{
    std::string s;
    size_t cursor;

public:
    /**
     * Creates a new word wrapper that takes data from the given string
     */
WordWrap(const std::string& s) : s(s), cursor(0) {}

    /**
     * Rewind the word wrapper, restarting the output from the beginning of the
     * string
     */
    void restart() { cursor = 0; }

    /**
     * Returns true if there is still data to be read in the string
     */
    bool hasData() const { return cursor < s.size(); }

    /**
     * Get a line of text from the string, wrapped to a maximum of \c width
     * characters
     */
    std::string get(unsigned int width) {

	if (cursor >= s.size())
            return "";

	// Find the last work break before `width'
	unsigned int brk = cursor;
	for (unsigned int j = cursor; j < s.size() && j < cursor + width; j++)
	{
            if (s[j] == '\n')
            {
                brk = j;
                break;
            } else if (!isspace(s[j]) && (j + 1 == s.size() || isspace(s[j + 1])))
                brk = j + 1;
	}
	if (brk == cursor)
            brk = cursor + width;

	std::string res;
	if (brk >= s.size())
	{
            res = std::string(s, cursor, std::string::npos);
            cursor = s.size();
	} else {
            res = std::string(s, cursor, brk - cursor);
            cursor = brk;
            while (cursor < s.size() && isspace(s[cursor]))
                cursor++;
	}
	return res;
    }
};

/// Given a pathname, return the file name without its path
inline std::string basename(const std::string& pathname)
{
    size_t pos = pathname.rfind("/");
    if (pos == std::string::npos)
        return pathname;
    else
        return pathname.substr(pos+1);
}

class Regexp
{
protected:
    regex_t re;
    regmatch_t* pmatch;
    int nmatch;
    std::string lastMatch;

    void checkIndex( int idx ) {
        if (idx > nmatch)
            throw 0; // wibble::exception::ValOutOfRange<int>("index", idx, 0, nmatch, "getting submatch of regexp");
    }

public:
    /* Note that match_count is required to be >1 to enable
       sub-regexp capture. The maximum *INCLUDES* the whole-regexp
       match (indexed 0). [TODO we may want to fix this to be more
       friendly?] */
    Regexp(const std::string& expr, int match_count = 0, int flags = 0)
    {
        if (match_count == 0)
            flags |= REG_NOSUB;

        int res = regcomp(&re, expr.c_str(), flags);
        if (res)
            throw 0; // wibble::exception::Regexp(re, res, "Compiling regexp \"" + expr + "\"");

        if (match_count > 0)
            pmatch = new regmatch_t[match_count];
    }

    ~Regexp() throw ()
    {
	regfree(&re);
	if (pmatch)
            delete[] pmatch;
    }

    bool match(const std::string& str, int flags = 0)
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
            default: throw 0; // XXX // wibble::exception::Regexp(re, res, "Matching string \"" + str + "\"");
        }
    }


    /* Indexing is from 1 for capture matches, like perl's $0,
       $1... 0 is whole-regexp match, not a capture. TODO
       the range is miscalculated (an off-by-one, wrt. the
       counterintuitive match counting). */
    std::string operator[](int idx) {
        checkIndex( idx );

        if (pmatch[idx].rm_so == -1)
            return std::string();

        return std::string(lastMatch, pmatch[idx].rm_so, pmatch[idx].rm_eo - pmatch[idx].rm_so);
    }

    size_t matchStart(int idx) {
        checkIndex( idx );
        return pmatch[idx].rm_so;
    }

    size_t matchEnd(int idx) {
        checkIndex( idx );
        return pmatch[idx].rm_eo;
    }

    size_t matchLength(int idx) {
        checkIndex( idx );
	return pmatch[idx].rm_eo - pmatch[idx].rm_so;
    }
};

class ERegexp : public Regexp
{
public:
	ERegexp(const std::string& expr, int match_count = 0, int flags = 0)
		: Regexp(expr, match_count, flags | REG_EXTENDED) {}
};

/**
 * Split a string using a regular expression to match the token separators.
 *
 * This does a similar work to the split functions of perl, python and ruby.
 *
 * Example code:
 * \code
 *   utils::Splitter splitter("[ \t]*,[ \t]*", REG_EXTENDED);
 *   vector<string> split;
 *   std::copy(splitter.begin(myString), splitter.end(), back_inserter(split));
 * \endcode
 *
 */
class Splitter
{
    Regexp re;

public:
    /**
     * Warning: the various iterators reuse the Regexps and therefore only one
     * iteration of a Splitter can be done at a given time.
     */
    // TODO: add iterator_traits
    class const_iterator
    {
        Regexp *re;
        std::string cur;
        std::string next;

    public:
        typedef std::string value_type;
        typedef ptrdiff_t difference_type;
        typedef value_type *pointer;
        typedef value_type &reference;
        typedef std::forward_iterator_tag iterator_category;

        const_iterator(Regexp& re, const std::string& str) : re(&re), next(str) { ++*this; }
        const_iterator(Regexp& re) : re(&re) {}

        const_iterator& operator++()
        {
            ASSERT( re );
            if (re->match(next)) {
                if (re->matchLength(0)) {
                    cur = next.substr(0, re->matchStart(0));
                    next = next.substr(re->matchStart(0) + re->matchLength(0));
                } else {
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
                next = std::string();
            }
            return *this;
        }

        const std::string& operator*() const
        {
            return cur;
        }
        const std::string* operator->() const
        {
            return &cur;
        }
        bool operator==(const const_iterator& ti) const
        {
            return cur == ti.cur && next == ti.next;
        }
        bool operator!=(const const_iterator& ti) const
        {
            return cur != ti.cur || next != ti.next;
        }
    };

    /**
     * Create a splitter that uses the given regular expression to find tokens.
     */
    Splitter(const std::string& re, int flags)
        : re(re, 1, flags) {}

    /**
     * Split the string and iterate the resulting tokens
     */
    const_iterator begin(const std::string& str) { return const_iterator(re, str); }
    const_iterator end() { return const_iterator(re); }
};

}
}

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
