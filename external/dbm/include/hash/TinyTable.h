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
// Filename : TinyTable.h
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: TinyTable.h,v 1.1 2005/04/22 15:20:10 adavid Exp $
//
////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_HASH_TINYTABLE_H
#define INCLUDE_HASH_TINYTABLE_H

#include "base/intutils.h"
#include "base/bitstring.h"

/** @file
 * This is a template for tiny hash tables that are:
 * - tiny
 * - allocated on the stack
 * - very fast
 * - not resizable
 * - bounded in the number of elements they can accept
 *   when instantiated
 * - able to store only void* or uint32_t
 * The trick is to allocate on the stack a int[something]
 * and instantiate TinyTable with this int[..]
 * The tiny hash table is a uint32_t[] managed by a few
 * simple functions.
 */

namespace hash
{
    class TinyTable
    {
    public:
        /** Constructor:
         * @param max: size of the table
         * @param mem: the table itself.
         * @pre max is a power of 2
         */
        TinyTable(uint32_t *mem, size_t max)
            : table(mem), mask(max-1)
        {
            base_resetSmall(mem, max);
        }

        /** @return size needed in uint32_t to store n items.
         * Use a power of 2 for speed, and use larger table to
         * reduce collisions.
         * @pre n != 0
         */
        static size_t intsizeFor(size_t n)
        {
            assert(n);
            n = base_fmsb32(n);
            return (1 << (n+3));
        }

        /** Add a pointer = wrap for int. */
        bool add(const void* ptr)
        {
            // 32 bits pointers -> lower bits == 0 so >>2 gives a good hash
            return add(((uintptr_t)ptr) >> 2);
        }

        /** Add an int. You should add at max n items as 
         * used in hash_sizeofTinyTable(n).
         * @return true if it was accepted, or false
         * if it was refused, ie, already present in the table.
         * @param val: int to add.
         * @pre val != 0 -- special value.
         */
        bool add(uint32_t val)
        {
            assert(val);
            // no collision list -> use next entry in table
            for(uint32_t index = val; table[index & mask] != val; ++index)
            {
                if (table[index & mask] == 0)
                {
                    table[index & mask] = val;
                    return true;
                }
            }
            return false;
        }

    private:
        uint32_t *table; ///< hash table
        uint32_t mask;   ///< binary mask = size-1 where size is a power of 2
    };
}

#endif // INCLUDE_HASH_TINYTABLE_H
