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
 * Filename : intutils.h (base)
 *
 * Utility functions for integers.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: intutils.h,v 1.13 2005/04/22 15:20:10 adavid Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_BASE_INTUTILS_H
#define INCLUDE_BASE_INTUTILS_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "base/inttypes.h"

/** Unsigned 32-bit subtraction with borrow. 
 * Function that subtracts two unsigned 32-bit numbers. 
 * For x86-processors this function is implemented in assembler.
 * This saves around 50 instructions of which several are branches.
 * @param a: the first term.
 * @param b: the second term.
 * @param cp: pointer to carry/borrow. (in/out).
 *
 * @return the lower 32-bits of a - (b + *cp).
 * 
 * @pre:
 * - *cp is either 0 or 1.
 *
 * @post:
 * - *cp is 1 if borrow was necessary, 0 otherwise.
 */
static inline
uint32_t base_subu32(uint32_t a, uint32_t b, uint32_t *cp)
{
#if defined(__GNUG__) && defined(i386)
    uint32_t c = *cp;
    asm("rcrl %1        # Move borrow to carry flag.\n\t"
        "sbbl %2, %0    # Perform subtraction.\n\t"
        "rcll %1        # Move borrow from carry flag.\n\t" 
        : "=r" (a), "=r" (c) : "rm" (b), "0" (a), "1" (c) : "cc");
    *cp = c;
    return a;
#else
    uint32_t c = *cp;   
    uint32_t v = (b + c);
    *cp = ((a < v) || (c > v) ? 1 : 0);
    return a - v;        
#endif
}


/** Best compromise for copy:
 * - for Intel, it is better to define our
 *   own copy function for small copies
 *   memcpy is better for sizes over 120-150
 * - for Sun, memcpy is always worse.
 *
 * As the cost of a function call is negligible
 * we prefer to reduce code size by not inlining
 * the code.
 *
 * @param src: source
 * @param dst: destination
 * @param intSize: size in int to copy
 * @pre src and dst are int32_t[intsize].
 */
void base_copySmall(void *dst, const void *src, size_t intSize);

static inline
void base_copyLarge(void *dst, const void *src, size_t intSize)
{
#if INTEL_ARCH
    memcpy(dst, src, intSize << 2);
#else
    base_copySmall(dst, src, intSize);
#endif
}

/** Try to choose best copy dynamically.
 */
static inline
void base_copyBest(void *dst, const void *src, size_t intSize)
{
#if INTEL_ARCH
    (intSize < 180 ? base_copySmall : base_copyLarge)(dst, src, intSize);
#else
    base_copySmall(dst, src, intSize);
#endif
}


/** Fill memory with a given value.
 * @param mem: memory to fill
 * @param intSize: size in int to fill
 * @param intValue: value to use to fill
 * @post int[0..intSize-1] at mem is filled with intValue
 */
void base_fill(void *mem, size_t intSize, uint32_t intValue);


/** Check if memory differs with a given value.
 * @param mem: memory to check
 * @param intSize: size in int to fill
 * @param intValue: value to check
 * @return 0 if mem[0..intSize-1] == intValue,
 * or !=0 otherwise.
 */
uint32_t base_diff(const void *mem, size_t intSize, uint32_t intValue);


/** Reset memory:
 * tests on Intel and Sun show that
 * memset is always slower on Sun and
 * it becomes faster than a custom function
 * on Intel after a threshold between 120 and 150
 * ints.
 * In practice reset is used for 2 very different
 * purposes:
 * - reset of small size for control vector or similar
 * - reset of large size for hash tables
 *
 * @param mem: memory to reset
 * @param intSize: size in int to reset
 * @pre mem is a int32_t[intSize]
 */
static inline
void base_resetSmall(void *mem, size_t intSize)
{
    base_fill(mem, intSize, 0);
}

static inline
void base_resetLarge(void *mem, size_t intSize)
{
#if INTEL_ARCH
    memset(mem, 0, intSize << 2);
#else
    base_fill(mem, intSize, 0);
#endif
}

/** Try to choose best reset dynamically.
 */
static inline
void base_resetBest(void *mem, size_t intSize)
{
#if INTEL_ARCH
    (intSize < 200 ? base_resetSmall : base_resetLarge)(mem, intSize);
#else
    base_fill(mem, intSize, 0);
#endif
}


/** Test equality. (optimistic implementation)
 * @param data1,data2: data to be compared.
 * @param intSize: size in int to compare.
 * @return TRUE if the contents are the same.
 * @pre data1 and data2 are of size int32_t[intSize]
 * null pointers are accepted as argument
 */
BOOL base_areEqual(const void *data1, const void *data2, size_t intSize);


/** @return abs(x) without a jump!
 * @param x: int to test.
 */
static inline
int32_t base_abs(int32_t x)
{
    /* Algo:
     * - if x >= 0, sign == 0
     *   and (x^sign)+sign == x
     * - if x < 0, sign == 1
     *   and -sign = 0xffffffff
     *   x^-sign == ~x
     *   return ~x+1 == -x
     */
    uint32_t sign = ((uint32_t) x) >> 31;
    return (x ^ -sign) + sign;
}


/** @return (x < 0 ? ~x : x) without a jump!
 * @param x: int to test.
 */
static inline
int32_t base_absNot(int32_t x)
{
    /* Algo: see base_abs
     */
    uint32_t sign = ((uint32_t) x) >> 31;
    return (x ^ -sign);
}


/** Quick sort: from lower to higher ints
 * @param values: values to sort
 * @param n: number of values
 * @pre values is a uint32_t[n]
 */
void base_sort(uint32_t *values, size_t n);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BASE_INTUTILS_H */
