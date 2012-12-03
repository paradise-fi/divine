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
// Filename : array.h (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: array.h,v 1.6 2005/01/25 18:30:22 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_ARRAY_H
#define INCLUDE_BASE_ARRAY_H

#include "base/pointer.h"

namespace base
{
    /** This defines a simple array template that will work on scalar
     * types only. The idea is to be able to access the array randomly
     * even on non predefined position. The array will grow automatically.
     * This is not possible with std::vector: you have to define a loop and
     * use push_back() to fill the gaps. Besides, reading outside the
     * vector will seg-fault.
     *
     * Lower case name: basic template on basic scalar types only
     * this class is not supposed to have sub-classes
     */
    template<class T>
    class array_t : public pointer_t<T>
    {
    public:
        
        /** increase size step: at least 1
         */
        enum { INC = 8 };
        
        /** constructor: alloc and init the array to 0
         * always at least 1 element.
         * @param initSize: initial size
         */
        array_t(size_t initSize = 4)
            : pointer_t<T>(new T[initSize == 0 ? 1 : initSize],
                           initSize == 0 ? 1 : initSize)
        {
            assert(INC > 0);
            pointer_t<T>::reset();
        }
        
        /** destructor: free the array
         */
        ~array_t() { delete [] pointer_t<T>::data; }
        

        /** Copy constructor
         */
        array_t(const array_t<T> &arr)
            : pointer_t<T>(new T[arr.capa], arr.capa)
        {
            copyFrom(arr);
        }

        /** copy operator
         */
        array_t& operator = (array_t<T> &arr)
        {
            if (pointer_t<T>::capa != arr.capa)
            {
                delete [] pointer_t<T>::data;
                pointer_t<T>::data = new T[arr.capa];
                pointer_t<T>::capa = arr.capa;
            }
            copyFrom(arr);
        }
        
        /** data read: test if outside bounds
         * @param at: where to read.
         * if at is outside limits then return 0.
         */
        T get(size_t at) const
        {
            return at < pointer_t<T>::capa ? pointer_t<T>::data[at] : 0;
        }
        
        /** data write: may increase array
         * @param at: where to write.
         * @param value: what to write.
         * if at is outside limits then the array is
         * increased.
         */
        void set(size_t at, T value)
        {
            ensurePos(at);
            pointer_t<T>::data[at] = value;
        }
        
        /** data read/write: may increase array
         * @param at: where to read and write.
         * @param value: what to write.
         * @return original value at position at.
         */
        T replace(size_t at, T value)
        {
            ensurePos(at);
            T res = pointer_t<T>::data[at];
            pointer_t<T>::data[at] = value;
            return res;
        }
        
        /** addition with the given argument
         * @param at: where to add value
         * @param value: what to add
         */
        void add(size_t at, T value)
        {
            ensurePos(at);
            pointer_t<T>::data[at] += value;
        }

    private:
        
        /** make sure position at may be accessed
         * @param at: position to be able to read.
         * @post data[at] is valid.
         */
        void ensurePos(size_t at)
        {
            if (at >= pointer_t<T>::capa)
            {
                T* newData = new T[at+INC];
                base_copySmall(newData, pointer_t<T>::data, pointer_t<T>::capa*intsizeof(T));
                base_resetSmall(newData+pointer_t<T>::capa, (at+INC-pointer_t<T>::capa)*intsizeof(T));
                delete [] pointer_t<T>::data;
                pointer_t<T>::data = newData;
                pointer_t<T>::capa = at+INC;
            }
        }
        
    };

} // namespace base

#endif // INCLUDE_BASE_ARRAY_H
