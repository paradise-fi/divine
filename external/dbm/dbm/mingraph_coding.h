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
 * Filename : mingraph_coding.h (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph_coding.h,v 1.9 2005/09/29 16:10:42 adavid Exp $
 *
 *********************************************************************/

#ifndef DBM_MINGRAPH_CODING_H
#define DBM_MINGRAPH_CODING_H

/**
 * @file
 * Contains format description
 * and coding/decoding primitives for mingraph.
 * The primitives are typed macros, ie, inlined
 * functions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Encoding of infinity on 16 bits.
 */
enum
{
    dbm_INF16 = SHRT_MAX >> 1,
    dbm_LS_INF16 = dbm_INF16 << 1
};


/***************************************************************************
 * Format of the encoding: information+data where information = uint32_t[2]
 * and data is variable.
 *
 * Type information (reserved bits) uint32_t[0]:
 * 0x0000ffff : dimension
 * 0xffff0000 : encoding information
 * 0x00010000 : set if encoding of constraints on 16 bits
 * 0x00040000 : set if minimal reduction used, otherwise just copy
 *              without the diagonal
 * 0x00020000 : set if bit couples (i,j) are used, otherwise use
 *              a bit matrix to mark the saved (i,j) constraints saved.
 * 0x00180000 : bits to control the # of bits used to encode
 *              the couples (i,j). Used only if 0x00010000 is set.
 *              if 0x00000000 then 4 bits/index
 *              if 0x00080000 then 8 bits/index
 *              if 0x00100000 then 16 bits/index
 *              the value 0x00180000 is not used.
 * 0x00200000 : set if there are more than 0x3ff constraints saved,
 *              to avoid to use a full int to code the number of constraints.
 * 0xffc00000 : number of constraints if 0x00200000 is NOT set = nsaved
 *              The maximal number of constraints we can store there
 *              is 0x3ff = 0xffc00000 >> 22
 *
 * Size information uint32_t[1] : number of saved constraints = nsaved,
 * if 0x00200000 is set.
 *
 * Copy DBM data type:
 * int32_t[dim*(dim-1)] or int16_t[dim*(dim-1)]
 *
 * Bit matrix data type:
 * int32_t[nsaved] or int16_t[nsaved] +
 * uint32_t[bits2intsize(dim*dim)]
 *
 * Couple(i,j) data type:
 * int32_t[nsaved] or int16_t[nsaved] +
 * (i,j)of variable size * nsaved padded within int32_t
 ***************************************************************************/


/* Basic information decoding from the type information.
 */
static inline
cindex_t mingraph_readDim(uint32_t info)
{
    return 0x0000ffff & info;
}

static inline
uint32_t mingraph_isCoded16(uint32_t info)
{
    return 0x00010000 & info;
}

static inline
uint32_t mingraph_isMinimal(uint32_t info)
{
    return 0x00040000 & info;
}

static inline
uint32_t mingraph_isCodedIJ(uint32_t info)
{
    return 0x00020000 & info;
}


/* We need to test types of the encoding
 * based on the bits
 * 0x00010000 codes if constraints are on 16 bits
 * 0x00020000 codes couples i,j
 * 0x00040000 marks if minimal reduction used
 * There are 6 valid combination out of 8
 * possible:
 * 0x00000000 copy, 32 bits
 * 0x00010000 copy, 16 bits
 * 0x00020000 invalid
 * 0x00030000 invalid
 * 0x00040000 min. red. bit matrix, 32 bits
 * 0x00050000 min. red. bit matrix, 16 bits
 * 0x00060000 min. red. couples i,j, 32 bits
 * 0x00070000 min. red. couples i,j, 16 bits
 *
 * It is then possible to simplify the code
 * by making a jump to the right function
 * directly with the index being defined
 * as (info & 0x00070000) >> 16
 */
static inline
uint32_t mingraph_getTypeIndex(uint32_t info)
{
    return (info & 0x00070000) >> 16;
}


/* Decode # of bits used for indices:
 * 0 -> 4 bits
 * 1 -> 8 bits
 * 2 -> 16 bits
 * 3 -> unused
 * shift the 19 precedent bits.
 */
static inline
uint32_t mingraph_typeOfIJ(uint32_t info)
{
    return (0x00180000 & info) >> 19;
}


/* Getting the type information.
 */
static inline
uint32_t mingraph_getInfo(const int32_t *mingraph)
{
    assert(mingraph);
    return (uint32_t) mingraph[0];
}


/* Simple wrapper.
 */
static inline
uint32_t mingraph_getTypeIndexFromPtr(const int32_t *mingraph)
{
    assert(mingraph);
    return mingraph_getTypeIndex(mingraph_getInfo(mingraph));
}


/* simple wrapper
 */
static inline
cindex_t mingraph_readDimFromPtr(const int32_t *mingraph)
{
    assert(mingraph);
    return mingraph_readDim(mingraph_getInfo(mingraph));
}


/* Getting number of constraints.
 */
static inline
size_t mingraph_getNbConstraints(const int32_t *mingraph)
{
    assert(mingraph);
    return (mingraph[0] & 0x00200000) ? /* long format */
        (size_t) mingraph[1] :        /* next int    */
        ((size_t) mingraph[0]) >> 22; /* higher bits */
}


/* Getting the coded data =
 * 1 + dynamic offset
 */
static inline
const int32_t* mingraph_getCodedData(const int32_t *mingraph)
{
    assert(mingraph);
    return mingraph + 1 +
        /* bit marking "many" constraints */
        ((mingraph[0] & 0x00200000) >> 21);
}


/* Restore a constraint on 32 bits:
 * - special detection for infinity
 * - restore signed int on 32 bits
 */
static inline
raw_t mingraph_raw16to32(int16_t raw16)
{
    return (raw16 == dbm_LS_INF16) ?
        dbm_LS_INFINITY :
        (((int32_t)raw16 & 0x7fff) -
         ((int32_t)raw16 & 0x8000));
}


/* Restore a constraint on 32 bits:
 * - special detection for infinity
 * - restore signed int on 32 bits
 * @pre raw16 is not infinity!
 */
static inline
raw_t mingraph_finite16to32(int16_t raw16)
{
    assert(raw16 != dbm_LS_INF16);
    return
        ((int32_t)raw16 & 0x7fff) -
        ((int32_t)raw16 & 0x8000);
}


/* Cut a constraint to 16 bits
 */
static inline
int16_t mingraph_raw32to16(raw_t raw32)
{
    /* check that the range is correct
     */
    assert(raw32 == dbm_LS_INFINITY ||
           (raw32  < dbm_LS_INF16 &&
            -raw32 < dbm_LS_INF16));
    
    /* convert infinity 32 -> 16 bits
     * or simple type convertion
     */
    if (raw32 == dbm_LS_INFINITY)
    {
        return dbm_LS_INF16;
    }
    else
    {
        return (int16_t) raw32;
    }
}


/* Cut a finite constraint to 16 bits
 */
static inline
int16_t mingraph_finite32to16(raw_t raw32)
{
    /* check that the range is correct
     */
    assert(raw32 != dbm_LS_INFINITY &&
           raw32  < dbm_LS_INF16 &&
           -raw32 < dbm_LS_INF16);
    
    return (int16_t) raw32;
}


/** Jump int16 integers, padded int32.
 * @param ints: int16 starting point (padded int32)
 * @param n: nb of int16 to jump
 * @return ints+n padded int32 as int32*
 */
static inline
const uint32_t* mingraph_jumpConstInt16(const int16_t *ints, size_t n)
{
    return ((uint32_t*)ints) + ((n + 1) >> 1);
}


/* Remove constraints of the form xi >= 0 from the bit matrix.
 * @param dbm,dim: DBM of dimension dim
 * @param bitMatrix: bit matrix representing the minimal graph
 * @param nbConstraints: current number of constraints
 * @return new number of constraints
 * @post bitMatrix has the constraints xi>=0 removed
 */
size_t dbm_cleanBitMatrix(const raw_t *dbm, cindex_t dim,
                          uint32_t *bitMatrix, size_t nbConstraints);


/** Useful function for bit manipulation
 * return a negated bit and set it afterwards.
 * instead of having
 * if !base_getOneBit(bitMatrix, index) cnt++;
 * base_setOneBit(bitMatrix, index);
 * we use
 * cnt += mingraph_ngetAndSetBit(bitMatrix, index)
 * @param bits: bit array.
 * @param index: bit index.
 * Reminder: index >> 5 == index/32
 * index & 31 == index%32
 * index & 31 is the position of the bit for the
 * right integer bits[index >> 5].
 */
static inline
bit_t mingraph_ngetAndSetBit(uint32_t *bits, size_t index)
{
    uint32_t nbit = /* negated bit */
        ((bits[index >> 5] >> (index & 31)) & 1) ^ 1;
    bits[index >> 5] |= (1 << (index & 31)); /* set the bit we just read */
    return (bit_t) nbit;
}


/* In loops reading constraints i,j out of a bit matrix
 * j is incremented everytime a bit is skipped so it is
 * necessary to fix i and j. It seems that for higher
 * dimensions, having a simple loop is better than the
 * arithmetic operations (potentially more expensive anyway).
 */
#define FIX_IJ() while(j >= dim) { j -= dim; ++i; }
/* #define FIX_IJ() do { i += j/dim; j %= dim; } while(0) */


#ifdef __cplusplus
}
#endif

#endif /* DBM_MINGRAPH_CODING_H */
