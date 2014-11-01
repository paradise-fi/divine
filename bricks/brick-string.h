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

}
}

#endif
// vim: syntax=cpp tabstop=4 shiftwidth=4 expandtab
