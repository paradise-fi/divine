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
// Filename : StreamHasher.h (hash)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: StreamHasher.h,v 1.3 2004/02/16 14:09:12 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_HASH_STREAMHASHER_H
#define INCLUDE_HASH_STREAMHASHER_H

#include "base/inttypes.h"

namespace hash
{

    /** Hasher class to hash on generic streams of data.
     * WARNING: approx 5x slower than the hash_compute functions.
     */
    class StreamHasher
    {
    public:

        /** Constructor.
         * @param initVal: initial value
         * for the hasher.
         */
        StreamHasher(uint32_t initVal)
            : hashValue(initVal)
        {}


        /** Add a value to the hash.
         * @param value: value to hash.
         */
        void addValue(uint32_t value)
        {
            hashValue = (hashValue ^ value) +
                ((hashValue << 26) + (hashValue >> 6));
        }

        
        /** Wrapper for addValue.
         * @param value: int to wrap to uint.
         */
        void addValue(int32_t value)
        {
            addValue((uint32_t)value);
        }


        /** Computes hash.
         * @return computed hash value.
         */
        uint32_t hash()
        {
            return hashValue;
        }


    private:
        uint32_t hashValue; /**< current hash value */
    };

} // namespace hash

#endif // INCLUDE_HASH_STREAMHASHER_H
