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
// Filename : H3HashFunction.h
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2004, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: H3HashFunction.h,v 1.4 2005/01/25 18:30:22 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_H3_HASH_FUNCTION_H
#define INCLUDE_H3_HASH_FUNCTION_H

#include "base/inttypes.h"
#include <cstddef>

namespace hash 
{
    /* Class for hashing generic streams of data using H3 hash functions.
     * This hasher produces arbitrty long hash values (multiple of 32-bits). 
     * The implemented hasher uses hash-functions from the H3-class which is a
     * universal_2 class of hash functions.
     */
    class H3HashFunction 
    {
    public:
        /** Constructor
         * @param function: Which hash function to use. 
         * @param width: The size of the hash value (in 32-bit words)
         * function.
         */
        H3HashFunction(uint32_t function, size_t width);
        
        /** Destructor
         */
        ~H3HashFunction();
        
        /** Get width of hash value
         * @return The width of the hash value. (In case you forgot)
         */
        size_t getWidth() const { return fWidth; }

        /** Compute the hash value for a given key.
         * @param vals: Pointer to the key.
         * @param length: The length of the key (in 32-bit words).
         * @param result: A buffer to store the hash value. The currently
         * stored value will be used as initial value.
         * @pre The buffer is large enough to hold the hash value.
         */
        void hash(const uint32_t *key, size_t lenght, uint32_t *result);
        void hash(const int32_t *key, size_t lenght, uint32_t *result);
        
    private:
        uint32_t **function;    ///< Hash function.
        size_t fWidth;        ///< Width of each hash value.
    };

    inline void H3HashFunction::hash(const int32_t *val,
                                     size_t len,
                                     uint32_t *res)
    {
        /* Wrapper only */
        hash(reinterpret_cast<const uint32_t *>(val), len, res);
    }

}

#endif /* INCLUDE_H3_HASH_FUNCTION_H */
