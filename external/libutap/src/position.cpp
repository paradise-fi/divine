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

#include <stdexcept>
#include "utap/position.h"

using std::string;
using std::vector;

using namespace UTAP;

void Positions::add(uint32_t position, uint32_t offset, uint32_t line, string path)
{
    if (!elements.empty() && position < elements.back().position)
    {
        throw std::logic_error("Positions must be monotonically increasing");
    }
    elements.push_back(line_t(position, offset, line, path));
}

const Positions::line_t &Positions::find(
    uint32_t position, uint32_t first, uint32_t last) const
{
    while (first + 1 < last)
    {
        uint32_t i = (first + last) / 2;
        if (position < elements[i].position)
        {
            last = i;
        }
        else
        {
            first = i;
        }
    }
    return elements[first];
}

const Positions::line_t &Positions::find(uint32_t position) const
{
    if (elements.size() == 0)
    {
        throw std::logic_error("No positions have been added");
    }
    return find(position, 0, elements.size());
}

/** Dump table to stdout. */
void Positions::dump()
{
    for (size_t i = 0; i < elements.size(); i++)
    {
        std::cout << elements[i].position << " "
                  << elements[i].offset << " "
                  << elements[i].line << " "
                  << elements[i].path << std::endl;
    }
}

std::ostream &operator <<(std::ostream &out, const UTAP::error_t &e) 
{
    if (e.start.path.empty())
    {
        out << e.msg << " at line "
            << e.start.line << " column " << (e.position.start - e.start.position)
            << " to line " 
            << e.end.line << " column " << (e.position.end - e.end.position);
    }
    else
    {
        out << e.msg << " in " << e.start.path << " at line "
            << e.start.line << " column " << (e.position.start - e.start.position)
            << " to line " 
            << e.end.line << " column " << (e.position.end - e.end.position);
    }
    return out;
};
