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

/* -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*********************************************************************
 *
 * Filename : compute.h (hash)
 * C header.
 *
 * Functions to compute hash values.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * Not reviewed yet.
 * $Id: compute.h,v 1.6 2005/04/22 15:20:10 adavid Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_HASH_COMPUTE_H
#define INCLUDE_HASH_COMPUTE_H

#include <assert.h>
#include <string.h>
#include "base/inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Hash mix found at http://burtleburtle.net/bob/hash/
 * Original comments from the author:
 * --------------------------------------------------------------------
 * mix -- mix 3 32-bit values reversibly.
 * For every delta with one or two bit set, and the deltas of all three
 * high bits or all three low bits, whether the original value of a,b,c
 * is almost all zero or is uniformly distributed,
 * * If mix() is run forward or backward, at least 32 bits in a,b,c
 *   have at least 1/4 probability of changing.
 * * If mix() is run forward, every bit of c will change between 1/3 and
 *   2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 * mix() takes 36 machine instructions, but only 18 cycles on a superscalar
 * machine (like a Pentium or a Sparc).  No faster mixer seems to work,
 * that's the result of my brute-force search.  There were about 2^^68
 *  hashes to choose from.  I only tested about a billion of those.
 * --------------------------------------------------------------------
 * NOTE: the hash function based on this hash mix is much faster than
 * MD5 hashing and gives exactly the same results.
 */
#define HASH_MIX(a,b,c)         \
{                               \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8);  \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12); \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5);  \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/** Compute hash value for different data types.
 * @param data: data to read.
 * @param length: number of types to read
 * @param initval: used to initialize the hash computation.
 * initval is useful when computing a composite hash
 * involving different data types. It is then possible
 * to chain the hash computation.
 * @pre pointers are 32 bits aligned.
 * @return: hash value.
 * @note the following calls may not necessarily return
 * the same result:
 * hash_computeU32((uint32_t*)data, 32, initval)
 * hash_computeU16((uint16_t*)data, 64, initval)
 * hash_computeU8((uint8_t*)data, 128, initval)
 */
uint32_t hash_computeU32(const uint32_t *data, size_t length, uint32_t initval);
uint32_t hash_computeU16(const uint16_t *data, size_t length, uint32_t initval);
uint32_t hash_computeU8( const uint8_t  *data, size_t length, uint32_t initval);


/** Wrapper functions for convenience.
 * Wrap types of input data: support for
 * int32_t => uint32_t
 * int16_t => uint16_t
 * int8_t => uint8_t
 * char[] => uint8_t
 */
static inline
uint32_t hash_computeI32(const int32_t *data, size_t length, uint32_t initval)
{
    return hash_computeU32((uint32_t*)data, length, initval);
}

static inline
uint32_t hash_computeI16(const int16_t *data, size_t length, uint32_t initval)
{
    return hash_computeU16((uint16_t*)data, length, initval);
}

static inline
uint32_t hash_computeI8(const int8_t  *data, size_t length, uint32_t initval)
{
    return hash_computeU8((uint8_t*)data, length, initval);
}

static inline
uint32_t hash_computeStr(const char *str, uint32_t initval)
{
    assert(sizeof(char) == sizeof(uint8_t));
    return hash_computeU8((uint8_t*)str, strlen(str), initval);
}

/** Compute a new hash from 3 previous hash values.
 * @param a,b,c: values to combine.
 * @return a mixed hashed value.
 */
static inline
uint32_t hash_compute(uint32_t a, uint32_t b, uint32_t c)
{
    HASH_MIX(a, b, c);
    return c;
}

#ifdef __cplusplus
}
#endif

#endif // INCLUDE_HASH_COMPUTE_H
