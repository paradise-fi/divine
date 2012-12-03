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

#ifndef INCLUDE_HASH_POINTERTABLE_H
#define INCLUDE_HASH_POINTERTABLE_H

#include <stdlib.h>

namespace hash
{
    /// Store non-NULL & non 0xffffffff pointers.
    class PointerTable
    {
    public:
        PointerTable();
        ~PointerTable();

        /// Clear the table.
        void clear();

        /// @return true if the pointer is in the table.
        bool has(const void*) const;

        /// @return true if the pointer is newly added in the table.
        bool add(const void*);

        /// @return true if the pointer was removed from the table.
        bool del(const void*);

        /// @return the number of pointer.
        size_t size() const { return nbPointers; }

        /// @return true if the tables contain the same set of pointers.
        bool operator == (const PointerTable&) const;

    private:
        void rehash();

        size_t getIndex(const void* ptr) const {
            return (((size_t) ptr) >> 3) & (tableSize-1);
        }

        const void **table;
        size_t tableSize, nbPointers;
    };
}

#endif // INCLUDE_HASH_POINTERTABLE_H
