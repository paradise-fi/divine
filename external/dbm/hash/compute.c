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
 * Filename : compute.c (hash)
 *
 * Implements hash functions.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * Not reviewed.
 * $Id: compute.c,v 1.3 2004/06/14 07:36:53 adavid Exp $
 *
 **********************************************************************/

#include "hash/compute.h"

/**********************************************************************
 * Algorithm of the hash functions implemented here:
 * read values 3 by 3, mix them and continue.
 * Return one of the mixed values as the result.
 **********************************************************************/

/** Hash for uint32_t[].
 * @param length: number of uint32_t
 * @param initval: initial value to start the hash computation.
 * @return the hash.
 */
uint32_t hash_computeU32(const uint32_t *data, size_t length, uint32_t initval)
{
    register uint32_t a, b, c, len;
    assert(data);

    /* Set up the internal state */
    a = 0x9e3779b9;  /* the golden ratio; an arbitrary value   */
    b = a + length;  /* combine with reasonable amount of bits */
    c = initval;     /* the previous hash value                */
    len = length;

    /* handle most of the key, partial unroll of the loop:
     * unroll to keep the processor ALU not disturbed by jumps.
     */
    while (len >= 9)
    {
        a += data[0];
        b += data[1];
        c += data[2];
        HASH_MIX(a,b,c);

        a += data[3];
        b += data[4];
        c += data[5];
        HASH_MIX(a,b,c);

        a += data[6];
        b += data[7];
        c += data[8];
        HASH_MIX(a,b,c);

        data += 9;
        len -= 9;
    }
    
    /* handle the last uint32_t's
     */
    switch(len)
    {
    case 8:
        b += data[7];
    case 7:
        a += data[6];
        HASH_MIX(a,b,c);

    case 6:
        c += data[5];
    case 5:
        b += data[4];
    case 4:
        a += data[3];
        HASH_MIX(a,b,c);

    case 3:
        c += data[2];
    case 2:
        b += data[1];
    case 1:
        a += data[0];
        HASH_MIX(a,b,c);

    case 0: /* Finished */
        ;   /* skip     */
    }
    
    /* report the result */
    return c;
}


/** This is to read specific # of bits at given addresses.
 * The cast is done to get rid of the endianness problem.
 */
#define READ16AT(adr) *((uint16_t*)(adr))
#define READ8AT(adr)  *((uint8_t*)(adr))


/** Hash for uint16_t[].
 * @param length: number of uint16_t
 * @param initval: initial value to start the hash computation.
 * @return the hash.
 */
uint32_t hash_computeU16(const uint16_t *data16, size_t length, uint32_t initval)
{
    register uint32_t a, b, c, len, *data;
    assert(data16);

    a = 0x9e3779b9;
    b = a + length;
    c = initval;
    len = length;
    data = (uint32_t*) data16; /* to read uint32_t */

    /* no unroll to keep the end switch case reasonable
     */
    while (len >= 6)
    {
        a += data[0];
        b += data[1];
        c += data[2];
        HASH_MIX(a,b,c);
        data += 3; /* read uint32_t: 3 uint32_t = 6 uint16_t */
        len -= 6;  /* len of uint16_t */
    }
    
    /* handle the last 2 uint32_t's and the last uint16_t = 5 uint16_t
     */
    switch(len)
    {
    case 5: /* 2 uint32 + 1 uint16 */
        c += READ16AT(data+2);

    case 4: /* 2 uint32 */
        a += data[0];
        b += data[1];
        break;

    case 3: /* 1 uint32 + 1 uint16 */
        b += READ16AT(data+1);

    case 2: /* 1 uint32 */
        a += data[0];
        break;

    case 1: /* 1 uint16 */
        a += READ16AT(data);
        break;

    case 0: /* Finished */
        return c;
    }
    
    HASH_MIX(a,b,c);
    return c;
}


/** Hash for uint8_t[].
 * @param length: number of uint16_t
 * @param initval: initial value to start the hash computation.
 * @return the hash.
 */
uint32_t hash_computeU8(const uint8_t  *data8, size_t length, uint32_t initval)
{
    register uint32_t a, b, c, len, *data;
    assert(data8);

    a = 0x9e3779b9;
    b = a + length;
    c = initval;
    len = length;
    data = (uint32_t*) data8; /* to read uint32_t */

    /* no unroll to keep the switch at a reasonable length
     */
    while (len >= 12)
    {
        a += data[0];
        b += data[1];
        c += data[2];
        HASH_MIX(a,b,c);
        data += 3; /* 3 uint32_t = 12 uint8_t */
        len -= 12; /* len in uint8_t */
    }
    
    /* handle the last (11) bytes
     */
    switch(len)
    {
    case 11: /* 2 uint32 + 1 uint16 + 1 uint8 */
        a += READ8AT(((uint16_t*)data)+5);
        HASH_MIX(a, b, c);

    case 10: /* 2 uint32 + 1 uint16 */
        a += data[0];
        b += data[1];
        c += READ16AT(data+2);
        break;

    case 9:  /* 2 uint32 + 1 uint8 */
        c += READ8AT(data+2);

    case 8:  /* 2 uint32 */
        a += data[0];
        b += data[1];
        break;

    case 7:  /* 1 uint32 + 1 uint16 + 1 uint8 */
        c += READ8AT(((uint16_t*)data)+3);

    case 6:  /* 1 uint32 + 1 uint16 */
        a += data[0];
        b += READ16AT(data+1);
        break;

    case 5:  /* 1 uint32 + 1 uint8 */
        b += READ8AT(data+1);

    case 4:  /* 1 uint32 */
        a += data[0];
        break;

    case 3:  /* 1 uint16 + 1 uint8 */
        b += READ8AT(((uint16_t*)data)+1);

    case 2:  /* 1 uint16 */
        a += READ16AT(data);
        break;

    case 1:  /* 1 uint8 */
        a += READ8AT(data);
        break;

    case 0: /* Finished */
        return c;
    }
    
    HASH_MIX(a,b,c);
    return c;
}
