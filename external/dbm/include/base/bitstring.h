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
 * Filename : bitstring.h (base)
 * C/C++ header.
 *
 * Basic operations on bits and strings of bits + BitString & Bit classes.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.1 reviewed.
 * $Id: bitstring.h,v 1.17 2005/05/14 16:57:34 behrmann Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_BASE_BIT_H
#define INCLUDE_BASE_BIT_H

#include <assert.h>
#include <stddef.h>

#include "base/intutils.h"

#ifdef __cplusplus
extern "C" {
#endif


/** Type to ensure correct arguments.
 */
typedef enum { ZERO = 0, ONE = 1 } bit_t;


/** High 16 bits of an int.
 * Useful in conjunction with hash values to store data.
 * @param x: the int to extract the high 16 bits.
 */
static inline
uint32_t base_high16(uint32_t x)
{
    return x & 0xffff0000;
}
    
/** Low 16 bits of an int.
 * Useful in conjunction with hash values to store data.
 * @param x: the int extract the low 16 bits.
 */
static inline
uint32_t base_low16(uint32_t x)
{
    return x & 0x0000ffff;
}

/** Bit counting function.
 * @return: the number of 1s in a given int
 * @param x: the int to examine.
 */
static inline
uint32_t base_countBits(uint32_t x)
{
    /* algorithm: count bits in parallel (hack) */
    x -= (x & 0xaaaaaaaa) >> 1;
    x = ((x >> 2) & 0x33333333L) + (x & 0x33333333L);
    x = ((x >> 4) + x) & 0x0f0f0f0fL;
    x = ((x >> 8) + x);
    x = ((x >> 16) + x) & 0xff;
    return x;
}

/** Bit counting function.
 * @return: the number of 1s in a given string of bits
 * @param bitString: the string of bits to examine
 * @param n: number of ints to read.
 */
static inline
uint32_t base_countBitsN(const uint32_t *bitString, size_t n)
{
    uint32_t cnt;
    assert(n == 0 || bitString);
    for(cnt = 0; n != 0; --n) cnt += base_countBits(*bitString++);
    return cnt;
}


/** Find most significant set bit (32-bit version).
 * Function that scans a 32-bit word for the most significant set bit.
 * For x86 this is implemented in assembler while for all other architectures
 * it is implemented by setting all bits below the max significant set bit to
 * 1 and counting the number of set bits.
 * 
 * @param w: the 32-bit word.
 * @return The index (0--31) of the most significant set bit.
 * 
 * @pre w != 0
 */
static inline
uint32_t base_fmsb32(uint32_t w)
{
    assert(w != 0);
#if defined(__GNUG__) && defined(i386)
    uint32_t r;
    asm("bsrl %1, %0" : "=r" (r) : "rm" (w) : "cc");
    return r;
#else
    w |= (w >> 1);                
    w |= (w >> 2);                
    w |= (w >> 4);                
    w |= (w >> 8);                
    w |= (w >> 16);                
    return base_countBits(w) - 1;    
#endif
}

/** Bitwise or ;
 * @param bits1: arg1 = destination
 * @param bits2: arg2 = source
 * @param n: number of ints to read
 * @return arg1 = arg1 | arg2
 */
static inline
void base_orBits(uint32_t *bits1, const uint32_t *bits2, size_t n)
{
    assert(n == 0 || (bits1 && bits2));
    for(; n != 0; --n) *bits1++ |= *bits2++;
}

/** Bitwise and ;
 * @param bits1: arg1 = destination
 * @param bits2: arg2 = source
 * @param n: number of ints to read
 * @return arg1 = arg1 & arg2
 */
static inline
void base_andBits(uint32_t *bits1, const uint32_t *bits2, size_t n)
{
    assert(n == 0 || (bits1 && bits2));
    for(; n != 0; --n) *bits1++ &= *bits2++;
}

/** Bitwise xor ;
 * @param bits1: arg1 = destination
 * @param bits2: arg2 = source
 * @param n: number of ints to read
 * @return arg1 = arg1 ^ arg2
 */
static inline
void base_xorBits(uint32_t *bits1, const uint32_t *bits2, size_t n)
{
    assert(n == 0 || (bits1 && bits2));
    for(; n != 0; --n) *bits1++ ^= *bits2++;
}

/** Reset of bits (to 0).
 * memset could be used but for strings of
 * bits of length 3-4 ints max, it is overkill.
 * Conversely, for larger arrays, don't use this
 * function.
 * @param bits: the string to reset
 * @param n: number of ints to write.
 */
static inline
void base_resetBits(uint32_t *bits, size_t n)
{
    assert(n == 0 || bits);
    for(; n != 0; --n) *bits++ = 0;
}

/** Set bits to 1.
 * memset could be used but for strings of
 * bits of length 3-4 ints max, it is overkill.
 * Conversely, for larger arrays, don't use this
 * function.
 * @param bits: the string to reset
 * @param n: number of ints to write.
 */
static inline
void base_setBits(uint32_t *bits, size_t n)
{
    assert(n == 0 || bits);
    for(; n != 0; --n) *bits++ = ~0u;
}

/** Set bits to something.
 * memset could be used but for strings of
 * bits of length 3-4 ints max, it is overkill.
 * Conversely, for larger arrays, don't use this
 * function.
 * @param bits: the string to reset
 * @param n: number of ints to write.
 * @param value: value to set.
 */
static inline
void base_setBitsWith(uint32_t *bits, size_t n, uint32_t value)
{
    assert(n == 0 || bits);
    for(; n != 0; --n) *bits++ = value;
}

/** Apply a xor mask to a bitstring.
 * @param bits: the string to xor
 * @param n: number of ints to change
 * @param mask: the mask for the xor
 */
static inline
void base_xorBitsWith(uint32_t *bits, size_t n, uint32_t mask)
{
    assert(n == 0 || bits);
    for(; n != 0; --n) *bits++ ^= mask;
}

/** Negate a bitstring.
 * @param bits: bit string to negate
 * @param n: number of ints to change
 */
static inline
void base_negBits(uint32_t *bits, size_t n)
{
    assert(n == 0 || bits);
    for(; n != 0; bits++, --n) *bits = ~*bits;
}

/** Reset of 2 strings of bits (to 0).
 * memset could be used but for strings of
 * bits of length 3-4 ints max, it is overkill.
 * Conversely, for larger arrays, don't use this
 * function.
 * @param bits1,bits2: the strings to reset
 * @param n: number of ints to write.
 */
static inline
void base_resetBits2(uint32_t *bits1, uint32_t *bits2, size_t n)
{
    assert(n == 0 || (bits1 && bits2));
    for(; n != 0; --n)
    {
        *bits1++ = 0;
        *bits2++ = 0;
    }
}

/** Set 2 strings of bits (to 1).
 * memset could be used but for strings of
 * bits of length 3-4 ints max, it is overkill.
 * Conversely, for larger arrays, don't use this
 * function.
 * @param bits1,bits2: the strings to reset
 * @param n: number of ints to write.
 */
static inline
void base_setBits2(uint32_t *bits1, uint32_t *bits2, size_t n)
{
    assert(n == 0 || (bits1 && bits2));
    for(; n != 0; --n)
    {
        *bits1++ = ~0u;
        *bits2++ = ~0u;
    }
}

/** Equality test to 0. Optimistic
 * implementation: works best when == 0.
 * @param bits: the string to test
 * @param n: n number of ints to test
 * @return true if all bits[0..n-1] == 0
 */
static inline
BOOL base_areBitsReset(const uint32_t *bits, size_t n)
{
    uint32_t diff = 0; /* accumulate the difference to 0 */
    assert(n == 0 || bits);
    for(; n != 0; --n) diff |= *bits++;
    return (BOOL)(diff == 0);
}

/** Wrapper to base_areBitsResets
 */
static inline
BOOL base_areIBitsReset(const int32_t *bits, size_t n)
{
    return base_areBitsReset((uint32_t*) bits, n);
}


/** Comparison of bits.
 * Same comment as for resetBits: better
 * to inline this simple code for small
 * arrays, ie, bit arrays. Optimistic implementation.
 * @param bits1,bits2: arrays to compare
 * @param n: number of ints to read
 * @return TRUE if array contents are the same
 */
static inline
BOOL base_areBitsEqual(const uint32_t *bits1,
                       const uint32_t *bits2, size_t n)
{
    uint32_t diff = 0;
    assert(n == 0 || (bits1 && bits2));
    for(; n != 0; --n) diff |= *bits1++ ^ *bits2++;
    return (BOOL)(diff == 0);
}

    
/** Copy of bits.
 * memcpy could be used but for strings of
 * bits of length 3-4 ints max, it is overkill.
 * Conversely, for larger arrays, don't use this
 * function.
 * @param bits: the string to reset
 * @param n: number of ints to read.
 * @pre n > 0 && valid pointer
 */ 
static inline
void base_copyBits(uint32_t *bits1, const uint32_t *bits2, size_t n)
{
    assert(n == 0 || (bits1 && bits2));
    for(; n != 0; --n) *bits1++ = *bits2++;
}

/** Set bit to 1 at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * Bit position in a table of int =
 * int position at i/32 == i >> 5
 * bit position at i%32 == i & 31
 * NOTE: we don't use setBit and resetBit
 * as names because they are too close to
 * resetBits that is already used.
 * @pre valid pointer and no out of bounds.
 */
static inline
void base_setOneBit(uint32_t *bits, size_t i)
{
    assert(bits);
    bits[i >> 5] |= 1 << (i & 31);
}


/** Add a bit at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * @param bit: the bit to add (0 or 1)
 * @pre valid pointer and no out of bounds.
 */
static inline
void base_addOneBit(uint32_t *bits, size_t i, bit_t bit)
{
    assert(bits && (bit == ONE || bit == ZERO));
    bits[i >> 5] |= bit << (i & 31);
}


/** Assign a bit at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * @param bit: the bit to assign.
 * @pre valid pointer and no out of bounds.
 */
static inline
void base_assignOneBit(uint32_t *bits, size_t i, bit_t bit)
{
    assert(bits && (bit == ONE || bit == ZERO));

    bits[i >> 5] = (bits[i >> 5] & ~(1 << (i & 31))) | (bit << (i & 31));
}


/** "and" operation with a bit at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * Bit position in a table of int =
 * int position at i/32 == i >> 5
 * bit position at i%32 == i & 31
 * @pre valid pointer and no out of bounds.
 */
static inline
void base_andWithOneBit(uint32_t *bits, size_t i, bit_t bit)
{
    assert(bits);
    /* check it is really a bit and nobody tried to cheat
     * with a bad cast.
     */
    assert(bit == ONE || bit == ZERO);
    bits[i >> 5] &= bit << (i & 31);
}


/** Reset bit to 0 at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * Bit position in a table of int =
 * int position at i/32 == i >> 5
 * bit position at i%32 == i & 31
 * NOTE: don't use setBit and resetBit
 * as names because they are too close to
 * resetBits that is already used.
 * @pre valid pointer and not out of bounds.
 */
static inline 
void base_resetOneBit(uint32_t *bits, size_t i)
{
    assert(bits);
    bits[i >> 5] &= ~(1 << (i & 31));
}


/** Substract a bit at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * Bit position in a table of int =
 * int position at i/32 == i >> 5
 * bit position at i%32 == i & 31
 * NOTE: don't use setBit and resetBit
 * as names because they are too close to
 * resetBits that is already used.
 * @pre valid pointer and not out of bounds.
 */
static inline 
void base_subOneBit(uint32_t *bits, size_t i, bit_t bit)
{
    assert(bits);
    /* check it is really a bit and nobody tried to cheat
     * with a bad cast.
     */
    assert(bit == ONE || bit == ZERO);
    bits[i >> 5] &= ~(bit << (i & 31));
}


/** Toggle bit at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * Bit position in a table of int =
 * int position at i/32 == i >> 5
 * bit position at i%32 == i & 31
 * @pre valid pointer and not out of bounds.
 */
static inline
void base_toggleOneBit(uint32_t *bits, size_t i)
{
    assert(bits);
    bits[i >> 5] ^= 1 << (i & 31);
}


/** Xor a bit at position i in a table of bits.
 * @param bits: the table of *int*
 * @param i: the position of the bit
 * Bit position in a table of int =
 * int position at i/32 == i >> 5
 * bit position at i%32 == i & 31
 * @pre valid pointer and not out of bounds.
 */
static inline
void base_xorOneBit(uint32_t *bits, size_t i, bit_t bit)
{
    assert(bits);
    /* check it is really a bit and nobody tried to cheat
     * with a bad cast.
     */
    assert(bit == ONE || bit == ZERO);
    bits[i >> 5] ^= bit << (i & 31);
}


/** Test for a set bit in a bit-string.
 * Can be used in the following way. Instead of: 
 * \code 
 * if (base_getOneBit(a,n)) c++; 
 * \endcode
 * you can write:
 * \code 
 * c += base_getOneBit(a,n);
 * \endcode
 * @returns ONE if \a i'th bit is set, ZERO otherwise.
 * @pre valid pointer and not out of bounds.
 */
static inline
bit_t base_getOneBit(const uint32_t *bits, size_t i)
{
    assert(bits);
    return (bit_t)((bits[i >> 5] >> (i & 31)) & 1);
}

/** Test for a set bit in a bit-string. This function is not the same
 * as \c base_getOneBit(). This function returns an integer with all
 * other bits except the \a i'th bit masked out. I.e. it returns 0 if
 * the bit is not set, and a power of 2 if it is set.
 * 
 * @returns the bit as it is in the int, ie, shifted.
 * @pre valid pointer and not out of bounds.
 */
static inline
uint32_t base_readOneBit(const uint32_t *bits, size_t i)
{
    assert(bits);
    return bits[i >> 5] & (1 << (i & 31));
}


/** Unpack a bit table to an index table.
 * Useful when loading a DBM and computing initial
 * redirection table. May be used for other things
 * than DBMs, e.g., variables.
 * @param table: index redirection table
 * @param bits: bit array
 * @param n: size in int of the bit array
 * @return number of bits in bits
 * @pre
 * - table is large enough: at least
 *   uint32_t[size] where size = index of the highest bit
 *   in the bitstring bits.
 * - bits is at least a uint32_t[n]
 * @post
 * - returned value == number of bits in bits
 * - for all bits set in bits at position i,
 *   then table[i] is defined and gives the
 *   proper indirection ; for other bits, table[i]
 *   is untouched.
 */
size_t base_bits2indexTable(const uint32_t *bits, size_t n, cindex_t *table);


#ifdef __cplusplus
}

#include "debug/utils.h"

namespace base
{
    /** Bit reference wrapper -- it is better to use directly
     * the previous functions when it is possible but this
     * may be useful. Called Bit for ease of use, e.g.,
     * Bit(a, 2) |= Bit::One;
     */
    class Bit
    {
    public:
        
        /******* Constants to make use of Bit coherent ******/

        static const bit_t One = ONE;
        static const bit_t Zero = ZERO;

        /** Constructor.
         * @param p: address of a bit string
         * @param i: index of the bit in that bit string
         */
        Bit(uint32_t *p, uint32_t i)
            : iptr(p + (i >> 5)), offset(i & 31)
        {}
        
        /************* Standard operators **********/
        
        Bit& operator |= (bit_t bit)
        {
            *iptr |= bit << offset;
            return *this;
        }
        Bit& operator |= (const Bit& bit)
        {
            return *this |= bit();
        }
        Bit& operator &= (bit_t bit)
        {
            *iptr &= bit << offset;
            return *this;
        }
        Bit& operator &= (const Bit& bit)
        {
            return *this &= bit();
        }
        Bit& operator ^= (bit_t bit)
        {
            *iptr ^= bit << offset;
            return *this;
        }
        Bit& operator ^= (const Bit& bit)
        {
            return *this ^= bit();
        }
        Bit& operator = (bit_t bit)
        {
            *iptr = (*iptr & ~(1 << offset)) | (bit << offset);
            return *this;
        }
        Bit& operator = (const Bit& bit)
        {
            return *this = bit();
        }
        Bit& operator += (bit_t bit)
        {
            return *this |= bit;
        }
        Bit& operator += (const Bit& bit)
        {
            return *this += bit();
        }
        Bit& operator -= (bit_t bit)
        {
            *iptr &= ~(bit << offset);
            return *this;
        }
        Bit& operator -= (const Bit& bit)
        {
            return *this -= bit();
        }
        bit_t operator () () const
        {
            return (bit_t) ((*iptr >> offset) & 1);
        }
        Bit& reset()
        {
            *iptr &= ~(1 << offset);
            return *this;
        }
        Bit& set()
        {
            *iptr |= (1 << offset);
            return *this;
        }
        Bit& neg()
        {
            *iptr ^= (1 << offset);
            return *this;
        }

    private:
        uint32_t *iptr, offset; //< bit address and offset
    };

    /** Encapsulation of a bitstring
     */
    class BitString
    {
    public:

        /** Constructor:
         * @param len: size in BITS of the bitstring.
         */
        BitString(uint32_t len)
            : n(bits2intsize(len)),
              bitString(new uint32_t[n])
        {
            base_resetSmall(bitString, n);
        }

        /** Copy constructor.
         * @param arg: original bit string to copy.
         */
        BitString(const BitString& arg)
            : n(arg.n), bitString(new uint32_t[arg.n])
        {
            base_copySmall(bitString, arg.bitString, arg.n);
        }

        /** Destructor, deallocate memory.
         */
        ~BitString() { delete [] bitString; }


        /** @return the number of bits set
         */
        size_t count() const
        {
            return base_countBitsN(bitString, n);
        }

        /** @return the index table corresponding to this
         * bit string and the number of bits set.
         * @param table: where to write the table.
         */
        size_t indexTable(cindex_t *table) const
        {
            return base_bits2indexTable(bitString, n, table);
        }

        /** @return the size in int of the bitstring.
         */
        size_t intSize() const
        {
            return n;
        }

        /** @return the size in bits of the bitstring.
         */
        size_t size() const
        {
            return n << 5;
        }

        /********** Standard operators ********/

        BitString& operator |= (const BitString& arg)
        {
            assert(arg.n <= n);
            base_orBits(bitString, arg.bitString, arg.n);
            return *this;
        }
        ///< @pre arg is a uint32_t[n]
        BitString& operator |= (const uint32_t *arg)
        {
            assert(n == 0 || arg);
            base_orBits(bitString, arg, n);
            return *this;
        }
        BitString& operator &= (const BitString& arg)
        {
            assert(arg.n <= n);
            base_andBits(bitString, arg.bitString, arg.n);
            return *this;
        }
        ///< @pre arg is a uint32_t[n]
        BitString& operator &= (const uint32_t *arg)
        {
            assert(n == 0 || arg);
            base_andBits(bitString, arg, n);
            return *this;
        }
        BitString& operator ^= (const BitString& arg)
        {
            assert(arg.n <= n);
            base_xorBits(bitString, arg.bitString, arg.n);
            return *this;
        }
        ///< @pre arg is a uint32_t[n]
        BitString& operator ^= (const uint32_t *arg)
        {
            assert(n == 0 || arg);
            base_xorBits(bitString, arg, n);
            return *this;
        }
        BitString& operator = (const BitString& arg)
        {
            assert(arg.n <= n);
            base_copySmall(bitString, arg.bitString, arg.n);
            return *this;
        }
        ///< @pre arg is a uint32_t[n]
        BitString& operator = (const uint32_t *arg)
        {
            assert(n == 0 || arg);
            base_copySmall(bitString, arg, n);
            return *this;
        }
        bool operator == (const BitString& arg)
        {
            assert(arg.n <= n);
            return (bool) base_areEqual(bitString, arg.bitString, arg.n);
        }
        ///< @pre arg is a uint32_t[n]
        bool operator == (const uint32_t *arg)
        {
            assert(n == 0 || arg);
            return (bool) base_areEqual(bitString, arg, n);
        }

        /** @return true if there is no bit set, false otherwise.
         */
        bool isEmpty() const 
        {
            return base_areBitsReset(bitString, n);
        }

        /** Negate all bits.
         */
        BitString& neg()
        {
            base_negBits(bitString, n);
            return *this;
        }

        /** Set all the bits with bit.
         * @param bit: value of the bit to set (0 or 1).
         */
        BitString& operator = (bit_t bit)
        {
            base_fill(bitString, n, bit ? ~0 : 0);
            return *this;
        }
        BitString& operator = (Bit &b)
        {
            return *this = b();
        }

        /** Set a given bit (to 1).
         * @param index: index of the bit to set.
         * @pre index is in range of the bit string.
         */
        BitString& operator += (cindex_t index)
        {
            assert(index < size());
            base_setOneBit(bitString, index);
            return *this;
        }

        /** Reset a given bit (to 0).
         * @param index: index of the bit to reset.
         * @pre index is in range of the bit string.
         */
        BitString& operator -= (cindex_t index)
        {
            assert(index < size());
            base_resetOneBit(bitString, index);
            return *this;
        }

        /** Toggle a given bit.
         * @param index: index of the bit to toggle.
         * @pre index is in range of the bit string.
         */         
        BitString& operator ^= (cindex_t index)
        {
            assert(index < size());
            base_toggleOneBit(bitString, index);
            return *this;
        }

        /** Read a given bit.
         * @param index: index of the bit to read.
         * @pre index is in range of the bit string.
         */
        bit_t getBit(cindex_t index) const
        {
            assert(index < size());
            return base_getOneBit(bitString, index);
        }

        /** Assign one bit (=bit).
         * @param index: index of the bit to assign.
         * @param bit: the bit to assign.
         * @pre index is in range of the bit string.
         */
        BitString& assignBit(cindex_t index, bit_t bit)
        {
            assert(index < size());
            base_assignOneBit(bitString, index, bit);
            return *this;
        }
        BitString& assignBit(cindex_t index, Bit b)
        {
            return assignBit(index, b());
        }

        /** Add one bit (|=bit).
         * @param index: index of the bit to add.
         * @param bit: the bit to add.
         * @pre index is in range of the bit string.
         */
        BitString& addBit(cindex_t index, bit_t bit)
        {
            assert(index < size());
            base_addOneBit(bitString, index, bit);
            return *this;
        }
        BitString& addBit(cindex_t index, Bit b)
        {
            return addBit(index, b());
        }

        /** Subtract one bit (&=~bit).
         * @param index: index of the bit to add.
         * @param bit: the bit to subtract.
         * @pre index is in range of the bit string.
         */
        BitString& subBit(cindex_t index, bit_t bit)
        {
            assert(index < size());
            base_subOneBit(bitString, index, bit);
            return *this;
        }
        BitString& subBit(cindex_t index, Bit b)
        {
            return subBit(index, b());
        }

        /** Xor operation with one bit (^=bit).
         * @param index: index of the bit to "xor" with.
         * @param bit: argument bit for the xor.
         * @pre index is in range of the bit string.
         */
        BitString& xorBit(cindex_t index, bit_t bit)
        {
            assert(index < size());
            base_xorOneBit(bitString, index, bit);
            return *this;
        }
        BitString& xorBit(cindex_t index, Bit b)
        {
            return xorBit(index, b());
        }

        /** Easy access of bits:
         * if bs is a BitString, you can do: bs[3] |= Bit::One
         * @return a bit reference.
         * @param index: index of the bit to access.
         * @pre index is in range of the bit string.
         */
        Bit operator [] (cindex_t index)
        {
            assert(bitString && index < size());
            return Bit(bitString, index);
        }

        /** Pretty print.
         * @param os: where to print.
         */
        std::ostream& prettyPrint(std::ostream& os) const
        {
            return debug_cppPrintBitstring(os, bitString, n);
        }

        /** Access to the bit string.
         */
        uint32_t* operator () () { return bitString; }
        const uint32_t* operator () () const { return bitString; }

    private:
        size_t n;
        uint32_t *bitString;
    };

    //< Standard overload.
    static inline
    std::ostream& operator << (std::ostream& os, const BitString& bs)
    { return bs.prettyPrint(os); }

    //< Standard overload.
    static inline
    std::ostream& operator << (std::ostream& os, const Bit& b)
    { return os << b(); }
}

#endif /* __cplusplus */

#endif /* INCLUDE_BASE_BITSTRING_H */
