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

#ifndef BRICK_STRING_H
#define BRICK_STRING_H

namespace brick {
namespace string {

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
