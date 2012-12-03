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
// Filename : xorptr.h (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: xorptr.h,v 1.3 2004/06/14 07:36:54 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_XORPTR_H
#define INCLUDE_BASE_XORPTR_H

#include "base/inttypes.h"
#include <assert.h>

namespace base
{
    /** xorptr_t encapsulate "safely"
     * the concept of swapping between
     * 2 pointers with a xor operation.
     *
     * Xor between a pointer and an int:
     * sounds scary but it is a very efficient
     * way to swap 2 pointers:
     * - init ptr to ptr1
     * - init xorPtr to ptr1 ^ ptr2
     * - swaping between ptr1 and ptr2
     *   is now equivalent to ptr ^= xorPtr,
     *   which is done without jump + we
     *   need to store only one xorPtr
     *   and not ptr1 and ptr2.
     *
     * The original idea of supperposing 2 states into one (here 2
     * pointers) in such a way that you get information on one state
     * only by observing the other state comes from quantum mechanics.
     */
    template<class T>
    class xorptr_t
    {
    public:

        /** Constructor: supperpose
         * internal values ptr1 and ptr2.
         * @param ptr1,ptr2: pointers to
         * swap between.
         */
        xorptr_t(T* ptr1, T* ptr2)
            : ptr1xor2(((uintptr_t)ptr1) ^
                       ((uintptr_t)ptr2))
        {
            assert(sizeof(T*) >= sizeof(uintptr_t));
        }


        /** Swap pointer (between ptr1 and
         * ptr2 used originally).
         * @return ptr1 if fromPtr == ptr2
         * or ptr2 if fromPtr == ptr1
         * @pre fromPtr == ptr1 or ptr2
         */
        T* swap(T* fromPtr) const
        {
            return (T*)(((uintptr_t)fromPtr) ^ ptr1xor2);
        }

    private:
        uintptr_t ptr1xor2; /**< supperposition of 2 pointers */
    };
}

#endif // INCLUDE_BASE_XORPTR_H
