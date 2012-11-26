// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002 Uppsala University and Aalborg University.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA
*/

#ifndef UTAP_LIBPARSER_HH
#define UTAP_LIBPARSER_HH

#include <functional>
    
#include "utap/builder.h"

#define MAXLEN 64

enum syntax_t { SYNTAX_OLD      = (1 << 0),
                SYNTAX_NEW      = (1 << 1),
                SYNTAX_PROPERTY = (1 << 2),
                SYNTAX_GUIDING  = (1 << 3),
                SYNTAX_TIGA     = (1 << 4),
                SYNTAX_SSQL     = (1 << 5)
};

// Defined in keywords.cc
extern bool isKeyword(const char *id, uint32_t syntax);

namespace UTAP
{
    /**
     * Help class used by the lexer, parser and xmlreader to keep
     * track of the current position. 
     */
    class PositionTracker
    {
    public:
        static uint32_t line;
        static uint32_t offset;
        static uint32_t position;
        static std::string path;
        
        /** Resets position tracker to position 0. */
        static void reset();

        /**
         * Sets the current path to \a s, offset to 0 and line to 1.
         * Sets the position of \a builder to [position, position + 1)
         * (a one character dummy position; this is useful when
         * assigning error messages to XML elements without a text
         * content). Adds position to \a builder and increments it by
         * 1.
         */
        static void setPath(ParserBuilder *builder, std::string s);

        /**
         * Sets the position of \a builder to [position, position + n)
         * and increments position and offset by \a n.
         */
        static int  increment(ParserBuilder *builder, int n);

        /**
         * Increments line by \a n and adds the position to \a
         * builder.
         */
        static void newline(ParserBuilder *builder, int n);
    };
}

#endif
