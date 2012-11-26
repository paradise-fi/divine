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

#ifndef UTAP_SYMBOLS_HH
#define UTAP_SYMBOLS_HH

#include <inttypes.h>
#include <exception>

#include "utap/common.h"
#include "utap/position.h"
#include "utap/type.h"

namespace UTAP
{
    class frame_t;
    class expression_t;
    
    class NoParentException : public std::exception {};

    /** An integer range. 
     */
    class range_t 
    {
    public:
        int lower, upper;

        /** Constructs the empty range */
        range_t();

        /** Constructs a range containing a single value */
        range_t(int);

        /** Constructs an interval range */
        range_t(int,int);

        /** Constructs an internval range */
        range_t(const std::pair<int,int> &);

        /** Constructs the intersection of two ranges */
        range_t intersect(const range_t &) const;

        /** Constructs the union of two ranges */
        range_t join(const range_t &) const;

        /** Returns true if the argument is contained in the range */
        bool contains(const range_t &) const;

        /** Returns true if the argument is contained in the range */
        bool contains(int32_t) const;

        /** Equallity operator */
        bool operator == (const range_t &) const;

        /** Inequallity operator */
        bool operator != (const range_t &) const;

        /** Constructs the union of two ranges */
        range_t operator| (const range_t &) const;

        /** Constructs the intersection of two ranges */
        range_t operator& (const range_t &) const;

        /** Returns true if and only if the range is empty */
        bool isEmpty() const;

        uint32_t size() const;
    };

    /**
       A reference to a symbol.

       Symbols can only be accessed via instances of
       symbol_t. Internally, symbols are reference counted and do not
       need to be deallocated manually. Each symbol has a name (which
       might be NULL) a type and an uninterpreted optional void
       pointer.

       Symbols are members of a frame (see also frame_t). It is
       possible to access the frame of a symbol via the symbol (see
       getFrame()). However, a symbol does not contain a counted
       reference to its frame so you must maintain at least one
       reference to the frame to avoid to be deallocated.
       
       Notice that it is possible to add the same symbol to several
       frames. In this case, the symbol will only "point back" to the
       first frame it was added to.
    */
    class symbol_t
    {
    private:
        struct symbol_data;
        symbol_data *data;
    protected:
        friend class frame_t;
        symbol_t(void *frame, type_t type, std::string name, void *user);
    public:
        /** Default constructor */
        symbol_t();

        /** Copy constructor */
        symbol_t(const symbol_t &);
        
        /** Destructor */
        ~symbol_t();

        /** Assignment operator */
        const symbol_t &operator = (const symbol_t &);

        /** Equality operator */
        bool operator == (const symbol_t &) const;

        /** Inequality operator */
        bool operator != (const symbol_t &) const;

        /** Less-than operator */
        bool operator < (const symbol_t &) const;
        
        /** Get frame this symbol belongs to */
        frame_t getFrame();

        /** Returns the type of this symbol. */
        type_t getType() const;

        /** Alters the type of this symbol */
        void setType(type_t);
        
        /** Returns the user data of this symbol */
        void *getData();

        /** Return the user data of this symbol */
        const void *getData() const;

        /** Returns the name (identifier) of this symbol */
        std::string getName() const;
        
        /** Sets the user data of this symbol */
        void setData(void *);
    };

    /**
       A reference to a frame.

       A frame is an ordered collection of symbols (see also
       symbol_t). Frames can only be accessed via an instance of
       frame_t. Internally, frames are reference counted and do not
       need to be deallocated manually.

       A frame can either be a root-frame or a sub-frame. Sub-frames
       have a parent frame; root frames do not. When a symbol name
       cannot be resolved in the current frame, it is resolved
       recursively in the parent frame.

       Frames are constructed using one of the static factory methods
       of frame_t. 

       In order to avoid cyclic references no counted reference to the
       parent frame is maintained. Hence, the existence of the parent
       frame must be ensured by other means throughout the lifetime of
       the sub-frame.
    */
    class frame_t
    {
    private:
        struct frame_data;
        frame_data *data;
    protected:
        friend class symbol_t;
        frame_t(void *);
    public:
        /** Default constructor */
        frame_t();

        /** Copy constructor */
        frame_t(const frame_t &);

        /** Destructor */
        ~frame_t();

        /** Assignment operator */
        const frame_t &operator = (const frame_t &);

        /** Equality operator */
        bool operator == (const frame_t &) const;

        /** Inequality operator */
        bool operator != (const frame_t &) const;
        
        /** Returns the number of symbols in this frame */
        uint32_t getSize() const;

        /** Returns the Nth symbol in this frame. */
        symbol_t getSymbol(int32_t);

        /** Returns the index of the symbol with the give name. */
        int32_t getIndexOf(std::string name) const;

        /** Returns the Nth symbol in this frame. */
        symbol_t operator[] (int32_t);

        /** Returns the Nth symbol in this frame. */
        const symbol_t operator[] (int32_t) const;

        /** Adds a symbol of the given name and type to the frame */
        symbol_t addSymbol(std::string name, type_t, void *user = NULL);

        /** Add all symbols from the given frame */
        void add(symbol_t);

        /** Add all symbols from the given frame */
        void add(frame_t);
        
        /** Resolves a name in this frame or a parent frame. */
        bool resolve(std::string name, symbol_t &symbol);

        /** Returns the parent frame */
        frame_t getParent() throw (NoParentException);

        /** Returns true if this frame has a parent */
        bool hasParent() const;

        /** Creates and returns a new root-frame. */
        static frame_t createFrame();

        /** Creates and returns a new sub-frame. */
        static frame_t createFrame(const frame_t &parent);
    };
}

#endif
