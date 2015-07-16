// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2006 Uppsala University and Aalborg University.
   
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

#ifndef UTAP_POSITION
#define UTAP_POSITION

#include <stdint.h>
#include <vector>
#include <string>
#include <iostream>
#include <climits>

#undef error_t // from llvm-3.3/include/llvm/Config/config.h

namespace UTAP
{
    struct position_t
    {
        uint32_t start, end;
        position_t() : start(0), end(UINT_MAX) {}
        position_t(uint32_t start, uint32_t end) : start(start), end(end) {}
    };

    /**
     * A container for information about lines and positions in the input
     * file.
     *
     * Elements in the container contain information about the
     * position in the input file of the first character of a line and
     * the XPath to the XML element containing the line. In general,
     * the line number refers to the line inside the XML element and
     * is not a global numbering of the lines in the input file. Also
     * the offset of the first character relative to the beginning of
     * the XML element is stored.
     *
     * If the input file is not an XML document, the path element of a
     * line is the empty string, the offset equals the position, and
     * the line numbers refer to the line number in the input file. In
     * essence, the whole input file is treated as if it were a single
     * XML element.
     */
    class Positions
    {
    public:
        struct line_t
        {
            uint32_t position;
            uint32_t offset;
            uint32_t line;    
            std::string path;
            line_t(uint32_t position, uint32_t offset, uint32_t line, std::string path)
                : position(position), offset(offset), line(line), path(path) {}
        };
        
    private:
        std::vector<line_t> elements;
        const line_t &find(uint32_t, uint32_t, uint32_t) const;
    public:
        /** Add information about a line to the container. */
        void add(uint32_t position, uint32_t offset, uint32_t line, std::string path);
        
        /**
         * Retrieves information about the line containing the given
         * position. The last line in the container is considered to
         * extend to inifinity (until another line is added).
         */
        const line_t &find(uint32_t position) const;
        
        /** Dump table to stdout. */
        void dump();
    };
    
    
    struct error_t
    {
        Positions::line_t start;
        Positions::line_t end;
        position_t position;
        std::string msg;
        error_t(Positions::line_t start, Positions::line_t end, 
                position_t position, std::string msg)
            : start(start), end(end), position(position), msg(msg) {}
    };
}

std::ostream &operator <<(std::ostream &out, const UTAP::error_t &);

#endif
