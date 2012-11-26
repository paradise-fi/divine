// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-

/* libutap - Uppaal Timed Automata Parser.
   Copyright (C) 2002-2006 Uppsala University and Aalborg University.
   
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

#include <cstdlib>
#include <cassert>
#include <vector>
#include <map>
#include <stdexcept>

#include "utap/symbols.h"
#include "utap/expression.h"

using std::vector;
using std::map;
using std::ostream;
using std::pair;
using std::make_pair;
using std::max;
using std::min;
using std::string;

// The base types

using namespace UTAP;
using namespace Constants;

//////////////////////////////////////////////////////////////////////////

range_t::range_t()
{
    // Default is the empty range
    lower = INT_MAX;
    upper = INT_MIN;
}

range_t::range_t(int value) : lower(value), upper(value)
{

}

range_t::range_t(int l, int u) : lower(l), upper(u)
{

}

range_t::range_t(const pair<int,int> &r) : lower(r.first), upper(r.second)
{

}

bool range_t::operator == (const range_t &r) const
{
    return lower == r.lower && upper == r.upper;
}

bool range_t::operator != (const range_t &r) const
{
    return lower != r.lower || upper != r.upper;
}

range_t range_t::intersect(const range_t &r) const
{
    return range_t(max(lower, r.lower), min(upper, r.upper));
}

range_t range_t::join(const range_t &r) const
{
    return range_t(min(lower, r.lower), max(upper, r.upper));
}

range_t range_t::operator| (const range_t &r) const
{
    return range_t(min(lower, r.lower), max(upper, r.upper));
}

range_t range_t::operator& (const range_t &r) const
{
    return range_t(max(lower, r.lower), min(upper, r.upper));
}

bool range_t::contains(const range_t &r) const
{
    return lower <= r.lower && r.upper <= r.upper;
}

bool range_t::contains(int32_t value) const
{
    return lower <= value && value <= upper;
}

bool range_t::isEmpty() const
{
    return lower > upper;
}

uint32_t range_t::size() const
{
    return isEmpty() ? 0 : upper - lower + 1;
}

//////////////////////////////////////////////////////////////////////////

struct symbol_t::symbol_data
{
    int32_t count;        // Reference counter
    void *frame;        // Uncounted pointer to containing frame
    type_t type;        // The type of the symbol
    void *user;                // User data
    string name;        // The name of the symbol
};

symbol_t::symbol_t()
{
    data = NULL;
}

symbol_t::symbol_t(void *frame, type_t type, string name, void *user)
{
    data = new symbol_data;
    data->count = 1;
    data->frame = frame;
    data->user = user;
    data->type = type;
    data->name = name;
}

/* Copy constructor */
symbol_t::symbol_t(const symbol_t &symbol)
{
    data = symbol.data;
    if (data) 
    {
        data->count++;
    }
}

/* Destructor */
symbol_t::~symbol_t()
{
    if (data) 
    {
        data->count--;
        if (data->count == 0) 
        {
            delete data;
        }
    }
}

/* Assignment operator */
const symbol_t &symbol_t::operator = (const symbol_t &symbol)
{
    if (data) 
    {
        data->count--;
        if (data->count == 0) 
        {
            delete data;
        }
    }
    data = symbol.data;
    if (data) 
    {
        data->count++;
    }
    return *this;
}

bool symbol_t::operator == (const symbol_t &symbol) const
{
    return data == symbol.data;
}

/* Inequality operator */
bool symbol_t::operator != (const symbol_t &symbol) const
{
    return data != symbol.data;
}

bool symbol_t::operator < (const symbol_t &symbol) const
{
    return data < symbol.data;
}

/* Get frame this symbol belongs to */
frame_t symbol_t::getFrame()
{
    return frame_t(data->frame);
}

/* Returns the type of this symbol. */
type_t symbol_t::getType() const
{
    return data->type;
}

void symbol_t::setType(type_t type)
{
    data->type = type;
}

/* Returns the user data of this symbol */
void *symbol_t::getData()
{
    return data->user;
}

/* Returns the user data of this symbol */
const void *symbol_t::getData() const
{
    return data->user;
}

/* Returns the name (identifier) of this symbol */
string symbol_t::getName() const
{
    return data->name;
}
        
/* Sets the user data of this symbol */
void symbol_t::setData(void *value)
{
    data->user = value;
}

//////////////////////////////////////////////////////////////////////////

struct frame_t::frame_data
{
    int32_t count;                        // Reference count
    bool hasParent;                        // True if there is a parent
    frame_data *parent;                        // The parent frame data
    vector<symbol_t> symbols;                // The symbols in the frame
    map<string, int32_t> mapping;       // Mapping from names to indices
};

frame_t::frame_t()
{
    data = NULL;
}

frame_t::frame_t(void *p)
{
    data = (frame_data*)p;
    if (data)
    {
        data->count++;
    }
}

/* Copy constructor */
frame_t::frame_t(const frame_t &frame)
{
    data = frame.data;
    if (data)
    {
        data->count++;
    }
}

/* Destructor */
frame_t::~frame_t()
{
    if (data) 
    {
        data->count--;
        if (data->count == 0)
        {
            delete data;
        }
    }
}

const frame_t &frame_t::operator = (const frame_t &frame)
{        
    if (data) 
    {
        data->count--;
        if (data->count == 0)
        {
            delete data;
        }
    }
    data = frame.data;
    if (data)
    {
        data->count++;
    }
    return *this;
}

/* Equality operator */
bool frame_t::operator == (const frame_t &frame) const
{
    return data == frame.data;
}

/* Inequality operator */ 
bool frame_t::operator != (const frame_t &frame) const
{
    return data != frame.data;
}

/* Returns the number of symbols in this frame */
uint32_t frame_t::getSize() const
{
    return data->symbols.size();
}

/* Returns the Nth symbol in this frame (counting from 0) */
symbol_t frame_t::getSymbol(int32_t n)
{
    return data->symbols[n];
}

/* Returns the Nth symbol in this frame (counting from 0) */
symbol_t frame_t::operator[](int32_t n)
{
    return data->symbols[n];
}

/* Returns the Nth symbol in this frame (counting from 0) */
const symbol_t frame_t::operator[](int32_t n) const
{
    return data->symbols[n];
}

/* Adds a symbol of the given name and type to the frame */
symbol_t frame_t::addSymbol(string name, type_t type, void *user)
{
    symbol_t symbol(data, type, name, user);
    data->symbols.push_back(symbol);
    if (!name.empty())
    {
        data->mapping[symbol.getName()] =  data->symbols.size() - 1;
    }
    return symbol;
}

/** Add symbol. Notice that the symbol will be in two frames at the
    same time, but the symbol will only "point back" to the first
    frame it was added to.
*/
void frame_t::add(symbol_t symbol)
{
    data->symbols.push_back(symbol);
    if (!symbol.getName().empty())
    {
        data->mapping[symbol.getName()] =  data->symbols.size() - 1;
    }
}

/** Add all symbols in the given frame. Notice that the symbols will
    be in two frames at the same time, but the symbol will only "point
    back" to the first frame it was added to.
*/
void frame_t::add(frame_t frame)
{
    for (uint32_t i = 0; i < frame.getSize(); i++) 
    {
        add(frame[i]);
    }
}

int32_t frame_t::getIndexOf(string name) const
{
    map<string, int32_t>::const_iterator i = data->mapping.find(name);
    return (i == data->mapping.end() ? -1 : i->second);
}

/**
   Resolves the name in this frame or the parent frame and
   returns the corresponding symbol. 
*/
bool frame_t::resolve(string name, symbol_t &symbol)
{
    int32_t idx = getIndexOf(name);
    if (idx == -1)
    {
        return (data->hasParent ? getParent().resolve(name, symbol) : false);
    }
    symbol = data->symbols[idx];
    return true;
}

/* Returns the parent frame */
frame_t frame_t::getParent() throw (NoParentException)
{
    if (!data->hasParent)
    {
        throw NoParentException();
    }
    return frame_t(data->parent);
}

/* Returns true if this frame has a parent */
bool frame_t::hasParent() const
{
    return data->hasParent;
}

/* Creates and returns a new frame without a parent */
frame_t frame_t::createFrame()
{
    frame_data *data = new frame_data;
    data->count = 0;
    data->hasParent = false;
    data->parent = 0;
    return frame_t(data);
}

/* Creates and returns new frame with the given parent */
frame_t frame_t::createFrame(const frame_t &parent)
{
    frame_data *data = new frame_data;
    data->count = 0;
    data->hasParent = true;
    data->parent = parent.data;
    return frame_t(data);  
}

