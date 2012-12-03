/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; -*-
 *
 * This file is part of the UPPAAL DBM library.
 *
 * The UPPAAL DBM library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * The UPPAAL DBM library is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with the UPPAAL DBM library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2006, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_ARRAY_H
#define INCLUDE_BASE_ARRAY_H

#include <new>
#include <string>
#include <string.h>
#include "base/Object.h"

namespace base
{
    /** Array of X where X is a scalar type (or struct of scalars
     * without constructor or destructor. Constructors and
     * destructors of X are not called.
     */
    template <class X>
    class Array : public Object
    {
    public:
        virtual ~Array() {}

        /** Creation & destruction. */
        static Array<X>* create(size_t size);
        virtual void destroy();

        /** Copy (also trunk and reallocate more). @post New elements = 0. */
        Array<X>* copy()            const { return copy(size()); }
        Array<X>* copy(size_t size) const;

        /** Simplified iterators. */
        const X* begin() const { return first; }
        const X* end()   const { return last; }
        X* begin()             { assert(isMutable()); return first; }
        X* end()               { assert(isMutable()); return last; }
        
        /** Standard operators. */
        const X& operator [] (size_t index) const;
        X& operator [] (size_t index);

        /** @return the size of the array. */
        size_t size() const { return last - first; }

        /** Set the array to zero. */
        Array<X>* zero();

    private:
        Array(size_t size)
            : last(first + size) {}

        X* last;
        X first[];
    };


    /****************************
     * Inlined implementations. *
     ****************************/

    template <class X> inline
    const X& Array<X>::operator [] (size_t index) const
    {
        assert(&first[index] < last);
        return first[index];
    }

    template <class X> inline
    X& Array<X>::operator [] (size_t index)
    {
        assert(isMutable());
        assert(&first[index] < last);
        return first[index];
    }

    template <class X> inline
    Array<X>* Array<X>::create(size_t size)
    {
        return new (new char[sizeof(Array<X>) + size*sizeof(X)]) Array<X>(size);
    }

    template <class X> inline
    void Array<X>::destroy()
    {
        delete [] (char*) this;
    }

    template <class X> inline
    Array<X>* Array<X>::copy(size_t copySize) const
    {
        Array<X>* a = create(copySize);
        size_t mySize = size();
        memcpy(a->first, first,
               copySize >= mySize
               ?(char*)last - (char*)first
               : copySize*sizeof(X));
        if (copySize > mySize)
        {
            memset(&a->first[mySize], 0,
                   (char*)a->last - (char*)&a->first[mySize]);
        }
        return a;
    }

    template <class X> inline
    Array<X>* Array<X>::zero()
    {
        memset(first, 0, (char*)last - (char*)first);
        return this;
    }
}

#endif // INCLUDE_BASE_ARRAY_H
