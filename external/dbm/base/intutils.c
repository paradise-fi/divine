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
 * Filename : intutils.c (base)
 *
 * Utility functions for integers.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: intutils.c,v 1.8 2005/04/22 15:20:08 adavid Exp $
 *
 *********************************************************************/

#include <assert.h>
#include "base/intutils.h"
#include "debug/macros.h"


/* Unroll a copy loop.
 */
void base_copySmall(void *vdst, const void *vsrc, size_t n)
{
    uint32_t *dst = (uint32_t*) vdst;
    uint32_t *src = (uint32_t*) vsrc;
    DODEBUG(size_t intSize = n);
        
    switch(n & 7)
    {
    case 7: dst[6] = src[6];
    case 6: dst[5] = src[5];
    case 5: dst[4] = src[4];
    case 4: dst[3] = src[3];
    case 3: dst[2] = src[2];
    case 2: dst[1] = src[1];
    case 1: dst[0] = src[0];
    }

    for(dst += (n & 7), src += (n & 7), n >>= 3;
        n > 0;
        dst += 8, src += 8, --n)
    {
        dst[0] = src[0]; dst[1] = src[1];
        dst[2] = src[2]; dst[3] = src[3];
        dst[4] = src[4]; dst[5] = src[5];
        dst[6] = src[6]; dst[7] = src[7];
    }

    /* Make sure that the copying was successful */
    assert(memcmp(vsrc, vdst, intSize * sizeof(uint32_t)) == 0);
}


/* Similar to copy.
 * Note: memset may be faster but it works on char.
 * wmemset is not better.
 */
void base_fill(void *mem, size_t n, uint32_t value)
{
    uint32_t *dst = (uint32_t*) mem;
    
    switch(n & 7)
    {
    case 7: dst[6] = value;
    case 6: dst[5] = value;
    case 5: dst[4] = value;
    case 4: dst[3] = value;
    case 3: dst[2] = value;
    case 2: dst[1] = value;
    case 1: dst[0] = value;
    }

    for(dst += (n & 7), n >>= 3;
        n > 0;
        dst += 8, --n)
    {
        dst[0] = value; dst[1] = value;
        dst[2] = value; dst[3] = value;
        dst[4] = value; dst[5] = value;
        dst[6] = value; dst[7] = value;
    }
}


/* Optimistic implementation of equality: accumulate
 * a difference and test it at the end.
 */
uint32_t base_diff(const void *data, size_t n, uint32_t value)
{
    const uint32_t *a = (uint32_t*) data;
    uint32_t diff = 0;
    
    switch(n & 7)
    {
    case 7: diff  = a[6] ^ value;
    case 6: diff |= a[5] ^ value;
    case 5: diff |= a[4] ^ value;
    case 4: diff |= a[3] ^ value;
    case 3: diff |= a[2] ^ value;
    case 2: diff |= a[1] ^ value;
    case 1: diff |= a[0] ^ value;
    }

    if (diff) return diff;

    for(a += (n & 7), n >>= 3;
        n > 0;
        a += 8, --n)
    {
        diff |=
            (a[0] ^ value) | (a[1] ^ value) |
            (a[2] ^ value) | (a[3] ^ value) |
            (a[4] ^ value) | (a[5] ^ value) |
            (a[6] ^ value) | (a[7] ^ value);
    }

    return diff;
}


/* Semi-optimistic implementation of equality testing.
 */
BOOL base_areEqual(const void *data1, const void *data2, size_t n)
{
    if (data1 == data2)
    {
        return TRUE;
    }
    else if (data1 == NULL || data2 == NULL)
    {
        return FALSE; // since different pointers
    }
    else
    {
        const uint32_t *a = (uint32_t*) data1;
        const uint32_t *b = (uint32_t*) data2;
        uint32_t diff = 0;
        
        switch(n & 7)
        {
        case 7: diff  = a[6] ^ b[6];
        case 6: diff |= a[5] ^ b[5];
        case 5: diff |= a[4] ^ b[4];
        case 4: diff |= a[3] ^ b[3];
        case 3: diff |= a[2] ^ b[2];
        case 2: diff |= a[1] ^ b[1];
        case 1: diff |= a[0] ^ b[0];
        }
        
        if (diff) return FALSE;
        
        for(a += (n & 7), b += (n & 7), n >>= 3;
            n > 0;
            a += 8, b += 8, --n)
        {
            if (((a[0] ^ b[0]) | (a[1] ^ b[1]) |
                 (a[2] ^ b[2]) | (a[3] ^ b[3]) |
                 (a[4] ^ b[4]) | (a[5] ^ b[5]) |
                 (a[6] ^ b[6]) | (a[7] ^ b[7])) != 0)
            {
                return FALSE;
            }
        }
        return TRUE;
    }
}


#define SWAP(PTR1,PTR2) do { \
  uint32_t _tmp = *(PTR1);   \
  *(PTR1) = *(PTR2);         \
  *(PTR2) = _tmp;            \
} while(0)

/* Quick sort
 */
static
void intutils_quicksort(uint32_t *min, uint32_t *max)
{
    uint32_t *i = min;
    uint32_t *j = max;

    assert(j >= i);

    if (j - i > 1)
    {
        uint32_t pivot = *(i + ((j - i) >> 1));
        do {
            while(*i < pivot) i++;
            while(*j > pivot) j--;
            if (i <= j)
            {
                SWAP(i,j);
                i++;
                j--;
            }
        } while(i <= j);
        if (min < j) intutils_quicksort(min, j);
        if (i < max) intutils_quicksort(i, max);
    }
    else if (*i > *j)
    {
        SWAP(i,j);
    }
}


void base_sort(uint32_t *values, size_t n)
{
    if (n > 1) intutils_quicksort(values, values + n - 1);
}
