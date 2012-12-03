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
 * Filename : mingraph_load.c (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph_read.c,v 1.14 2005/06/24 15:32:52 adavid Exp $
 *
 *********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "base/bitstring.h"
#include "dbm/mingraph.h"
#include "mingraph_coding.h"
#include "debug/macros.h"

/*#define EXPERIMENTAL*/

/**
 * @file
 * Contains implementation of the read function
 * from the API: this is a switch between different
 * encoding specific functions.
 */


/**********************
 * Decoding functions *
 **********************/

/* For reading DBM from mingraph_t */
static
cindex_t mingraph_readFromCopy32(raw_t *dbm, const int32_t *mingraph);
static
cindex_t mingraph_readFromCopy16(raw_t *dbm, const int32_t *mingraph);
static
cindex_t mingraph_readFromMinBitMatrix32(raw_t *dbm, const int32_t *mingraph);
static
cindex_t mingraph_readFromMinBitMatrix16(raw_t *dbm, const int32_t *mingraph);
static
cindex_t mingraph_readFromMinCouplesij32(raw_t *dbm, const int32_t *mingraph);
static
cindex_t mingraph_readFromMinCouplesij16(raw_t *dbm, const int32_t *mingraph);
static
cindex_t mingraph_readError(raw_t *dbm, const int32_t *mingraph);

/* For reading the bit matrix from mingraph_t */
static
size_t mingraph_bitMatrixFromCopy32(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer);
static
size_t mingraph_bitMatrixFromCopy16(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer);
static
size_t mingraph_bitMatrixFromBitMatrix32(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer);
static
size_t mingraph_bitMatrixFromBitMatrix16(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer);
static
size_t mingraph_bitMatrixFromCouplesij32(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer);
static
size_t mingraph_bitMatrixFromCouplesij16(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer);
static
size_t mingraph_bitMatrixError(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer);

/*******************************************
 * Decoding/testing if zero is contained.
 * Can be extended later to test any point.
 *******************************************/

static BOOL mingraph_copy32HasZero(const int32_t*);
static BOOL mingraph_copy16HasZero(const int32_t*);
static BOOL mingraph_errorHasZero(const int32_t*);
static BOOL mingraph_c32HasZero(const int32_t*);
static BOOL mingraph_c16HasZero(const int32_t*);


#ifdef EXPERIMENTAL
/* Experimental close */
static
void mingraph_close(uint32_t *indices, size_t n, raw_t *dbm, cindex_t dim);
#endif

/***************************************
 ****** Implementation of the API ******
 ***************************************/

/* Type of the test functions.
 */
typedef BOOL (*hasZero_f)(const int32_t*);

BOOL dbm_mingraphHasZero(const int32_t* minDBM)
{
    static const hasZero_f hasZero[8] =
    {
        mingraph_copy32HasZero,
        mingraph_copy16HasZero,
        mingraph_errorHasZero,
        mingraph_errorHasZero,
        mingraph_c32HasZero,
        mingraph_c16HasZero,
        mingraph_c32HasZero,
        mingraph_c16HasZero
    };

    assert(minDBM);
    assert(mingraph_readDimFromPtr(minDBM) > 0);

    /* dim == 1 or
     * dim == 0 (forbidden but treated anyway)
     * cast compulsory to avoid negative numbers
     */
    if (((cindex_t)*minDBM) <= 1)
    {
        return FALSE;
    }
    
    /* dim > 1
     */
    return hasZero[mingraph_getTypeIndexFromPtr(minDBM)](minDBM);
}

/* Type of the read functions.
 */
typedef cindex_t (*readDBM_f)(raw_t*, const int32_t*);


/* Algorithm:
 * 1) check the format
 * 2) call the proper decoding function
 */
cindex_t dbm_readFromMinDBM(raw_t *dbm, const int32_t *minDBM)
{
    /* see mingraph_getTypeIndex comments
     */
    static const readDBM_f readFromMinDBM[8] =
    {
        mingraph_readFromCopy32,
        mingraph_readFromCopy16,
        mingraph_readError,
        mingraph_readError,
        mingraph_readFromMinBitMatrix32,
        mingraph_readFromMinBitMatrix16,
        mingraph_readFromMinCouplesij32,
        mingraph_readFromMinCouplesij16
    };

    assert(dbm && minDBM);
    assert(mingraph_readDimFromPtr(minDBM) > 0);

    /* dim == 1 or
     * dim == 0 (forbidden but treated anyway)
     * cast compulsory to avoid negative numbers
     */
    if (((cindex_t)*minDBM) <= 1)
    {
        *dbm = dbm_LE_ZERO;
        return (cindex_t) *minDBM;
    }
    
    /* dim > 1
     */
    return
        readFromMinDBM[mingraph_getTypeIndexFromPtr(minDBM)]
        (dbm, minDBM);
}


/* Type of the bit matrix decoding functions.
 */
typedef size_t (*bitMatrixDBM_f)(uint32_t*, const int32_t*, BOOL, raw_t*);


/* Algorithm:
 * 1) check the format
 * 2) call the proper decoding function
 */
size_t dbm_getBitMatrixFromMinDBM(uint32_t *bitMatrix, const int32_t* minDBM,
                                  BOOL isUnpacked, raw_t *buffer)
{
    /* see mingraph_getTypeIndex comments
     */
    static const bitMatrixDBM_f bitMatrixFromMinDBM[8] =
    {
        mingraph_bitMatrixFromCopy32,
        mingraph_bitMatrixFromCopy16,
        mingraph_bitMatrixError,
        mingraph_bitMatrixError,
        mingraph_bitMatrixFromBitMatrix32,
        mingraph_bitMatrixFromBitMatrix16,
        mingraph_bitMatrixFromCouplesij32,
        mingraph_bitMatrixFromCouplesij16
    };

    assert(buffer && minDBM && bitMatrix);
    assert(mingraph_readDimFromPtr(minDBM) > 0);

    /* dim == 1 or
     * dim == 0 (forbidden but treated anyway)
     */
    if (((cindex_t)*minDBM) <= 1)
    {
        *bitMatrix = 0;
        return 0;
    }

#ifndef NDEBUG
    if (isUnpacked) /* paranoid check */
    {
        cindex_t dim = dbm_getDimOfMinDBM(minDBM);
        raw_t *check = (raw_t*) malloc(dim*dim*sizeof(raw_t));
        if (check)
        {
            dbm_readFromMinDBM(check, minDBM);
            assert(dbm_areEqual(check, buffer, dim));
            free(check);
        }
    }
#endif
    
    /* dim > 1
     */
    return
        bitMatrixFromMinDBM[mingraph_getTypeIndexFromPtr(minDBM)]
        (bitMatrix, minDBM, isUnpacked, buffer);
}


/***************************************
 * Format dependent decoding functions *
 ***************************************/


static BOOL mingraph_copy32HasZero(const int32_t* mingraph)
{
    uint32_t info = mingraph_getInfo(mingraph); /* type info */
    cindex_t dim = mingraph_readDim(info);
    const raw_t *saved = (raw_t*) &mingraph[1]; /* constraints after info */

    assert(dim > 1);
    size_t nbCols = dim; /* between diagonal elements */
    do {
        if (*saved < dbm_LE_ZERO)
        {
            return FALSE;
        }
        saved++;
    } while(--nbCols);

    return TRUE;
}

static BOOL mingraph_copy16HasZero(const int32_t* mingraph)
{
    uint32_t info = mingraph_getInfo(mingraph); /* type info */
    cindex_t dim = mingraph_readDim(info);
    const int16_t *saved = (int16_t*) &mingraph[1]; /* constraints after info */

    assert(dim > 1);
    size_t nbCols = dim; /* between diagonal elements */
    do {
        if (mingraph_raw16to32(*saved) < dbm_LE_ZERO)
        {
            return FALSE;
        }
        saved++;
    } while(--nbCols);

    return TRUE;
}

static BOOL mingraph_errorHasZero(const int32_t* mingraph)
{
    fprintf(stderr,
            RED(BOLD) PACKAGE_STRING
            " fatal error: invalid encoded DBM to test"
            NORMAL "\n");
    exit(2);
    return FALSE;
}

static BOOL mingraph_c32HasZero(const int32_t* mingraph)
{
    const raw_t *c = mingraph_getCodedData(mingraph);
    const raw_t *end = c + mingraph_getNbConstraints(mingraph);

    for(; c < end; ++c)
    {
        if (dbm_LE_ZERO > *c)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL mingraph_c16HasZero(const int32_t* mingraph)
{
    const int16_t *c = (int16_t*) mingraph_getCodedData(mingraph);
    const int16_t *end = c + mingraph_getNbConstraints(mingraph);

    for(; c < end; ++c)
    {
        if (dbm_LE_ZERO > mingraph_raw16to32(*c))
        {
            return FALSE;
        }
    }

    return TRUE;
}


/* Read from a DBM without the diagonal and write to
 * a DBM with the diagonal.
 * @param dbm: DBM to write
 * @param mingraph: DBM copy without diagonal, with
 * constraints on 32 bits to read.
 * @pre dbm is at least a raw_t[dim*dim], with dim = saved dimension.
 */
static
cindex_t mingraph_readFromCopy32(raw_t *dbm, const int32_t *mingraph)
{
    uint32_t info = mingraph_getInfo(mingraph); /* type info */
    cindex_t dim = mingraph_readDim(info);
    const raw_t *saved = (raw_t*) &mingraph[1]; /* constraints after info */
    size_t nbLines = dim - 1;

    assert(dim > 1);

    do {
        size_t nbCols = dim; /* between diagonal elements */
        *dbm++ = dbm_LE_ZERO;  /* write diagonal            */
        do {
            *dbm++ = *saved++; /* write DBM */
        } while(--nbCols);
    } while(--nbLines);

    *dbm = dbm_LE_ZERO; /* last element of the diagonal */
    return dim;
}


/* Read from a DBM without the diagonal and write to
 * a DBM with the diagonal.
 * @param dbm: DBM to write
 * @param mingraph: DBM copy without diagonal, with
 * constraints on 16 bits to read.
 * @pre dbm is at least a raw_t[dim*dim], with dim = saved dimension.
 */
static
cindex_t mingraph_readFromCopy16(raw_t *dbm, const int32_t *mingraph)
{
    uint32_t info = mingraph_getInfo(mingraph); /* type info */
    cindex_t dim = mingraph_readDim(info);
    const int16_t *saved = (int16_t*) &mingraph[1]; /* constraints after info */
    size_t nbLines = dim - 1;

    assert(dim > 1);

    do {
        size_t nbCols = dim; /* between diagonal elements */
        *dbm++ = dbm_LE_ZERO;  /* write diagonal            */
        do {
            /* restore infinity and signed int
             */
            *dbm++ = mingraph_raw16to32(*saved++);
        } while(--nbCols);
    } while(--nbLines);

    *dbm = dbm_LE_ZERO; /* last element of the diagonal */
    return dim;
}


/* Read from a minimal graph with a bit matrix telling which
 * constraints are saved and write to a DBM.
 * @param dbm: DBM to write
 * @param mingraph: minimal graph with bit matrix and
 * constraints on 32 bits to read.
 * @pre dbm is at least a raw_t[dim*dim], with dim = saved dimension.
 */
static
cindex_t mingraph_readFromMinBitMatrix32(raw_t *dbm, const int32_t *mingraph)
{
    cindex_t dim = mingraph_readDimFromPtr(mingraph);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const raw_t *constraints = mingraph_getCodedData(mingraph);
    uint32_t *bitMatrix = (uint32_t*) &constraints[nbConstraints];
    raw_t *dst = dbm;
    cindex_t i = 0, j = 0;
    
#ifdef EXPERIMENTAL
    uint32_t indices[dim*(dim-1)];
    size_t ni = 0;
#else
    /* keep track of touched clocks */
    uint32_t touched[bits2intsize(dim)];
    base_resetBits(touched, bits2intsize(dim));
#endif

    assert(dbm && dim > 2);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim)) == nbConstraints);
    assert(nbConstraints); /* other coding for no constraint */

    dbm_init(dbm, dim);

    /* similar to save */
    for(;;)
    {
        uint32_t b, count;
        for(b = *bitMatrix++, count = 32; b != 0; ++j, ++dst, --count, b >>= 1)
        {
            /* could have if (b & 1) { .. but we would loop on 2 conditions
             * b != 0 and (b & 1) == 0 though we need only one condition!
             */
            for(; (b & 1) == 0; ++j, ++dst, --count, b >>= 1)
            {
                assert(count);
            }
            FIX_IJ();
            *dst = *constraints++;
#ifdef EXPERIMENTAL
            if (i)
            {
                indices[ni++] = i | (j << 16);
            }
            if (!--nbConstraints)
            {
                if (ni) mingraph_close(indices, ni, dbm, dim);
                return dim;
            }
#else
            /* see readFromMinCouplesij32 */
            if (i)
            {
                base_setOneBit(touched, i);
                base_setOneBit(touched, j);
            }
            if (!--nbConstraints) /* no constraint left */
            {
                dbm_closex(dbm, dim, touched);
                return dim;
            }
#endif
            assert(count);
        }
        /* jump unread elements */
        j += count;
        dst += count;
    }
}


/* Read from a minimal graph with a bit matrix telling which
 * constraints are saved and write to a DBM.
 * @param dbm: DBM to write
 * @param mingraph: minimal graph with bit matrix and
 * constraints on 16 bits to read.
 * @pre dbm is at least a raw_t[dim*dim], with dim = saved dimension.
 */
static
cindex_t mingraph_readFromMinBitMatrix16(raw_t *dbm, const int32_t *mingraph)
{
    cindex_t dim = mingraph_readDimFromPtr(mingraph);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const int16_t *constraints = (int16_t*) mingraph_getCodedData(mingraph);
    const uint32_t *bitMatrix =        mingraph_jumpConstInt16(constraints, nbConstraints);
    raw_t *dst = dbm;
    cindex_t i = 0, j = 0;

#ifdef EXPERIMENTAL
    uint32_t indices[dim*(dim-1)];
    size_t ni = 0;
#else
    /* keep track of touched clocks */
    uint32_t touched[bits2intsize(dim)];
    base_resetBits(touched, bits2intsize(dim));
#endif

    assert(dbm && dim > 2);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim)) == nbConstraints);
    assert(nbConstraints); /* other coding for no constraint */

    dbm_init(dbm, dim);

    /* similar to save */
    for(;;)
    {
        uint32_t b, count;
        for(b = *bitMatrix++, count = 32; b != 0; ++j, ++dst, --count, b >>= 1)
        {
            /* could have if (b & 1) { .. but we would loop on 2 conditions
             * b != 0 and (b & 1) == 0 though we need only one condition!
             */
            for(; (b & 1) ==0; ++j, ++dst, --count, b >>= 1)
            {
                assert(count);
            }
            FIX_IJ();
            *dst = mingraph_finite16to32(*constraints++);
#ifdef EXPERIMENTAL
            if (i)
            {
                indices[ni++] = i | (j << 16);
            }
            if (!--nbConstraints)
            {
                if (ni) mingraph_close(indices, ni, dbm, dim);
                return dim;
            }
#else
            /* see readFromMinCouplesij32 */
            if (i)
            {
                base_setOneBit(touched, i);
                base_setOneBit(touched, j);
            }
            if (!--nbConstraints) /* no constraint left */
            {
                dbm_closex(dbm, dim, touched);
                return dim;
            }
#endif
            assert(count);
        }
        /* jump unread elements */
        j += count;
        dst += count;
    }
}

/* Read from a minimal graph with a list of couples (i,j)
 * telling which constraints are saved, and write to a DBM.
 * @param dbm: DBM to write
 * @param mingraph: minimal graph with couples (i,j) and
 * constraints on 32 bits to read.
 * @pre dbm is at least a raw_t[dim*dim], with dim = saved dimension.
 */
static
cindex_t mingraph_readFromMinCouplesij32(raw_t *dbm, const int32_t *mingraph)
{
    uint32_t info = mingraph_getInfo(mingraph);
    cindex_t dim = mingraph_readDim(info);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);

    assert(dbm && (dim > 2 || nbConstraints == 0));
    dbm_init(dbm, dim);

    if (nbConstraints) /* could be = 0 */
    {
        /* reminder: size of indices of couples i,j = 4, 8, or 16 bits
         * the stored code is 0, 1, or 2, hence size = 2**(code + 2)
         * or 1 << (code + 2)
         */
        uint32_t bitSize = 1 << (mingraph_typeOfIJ(info) + 2);
        uint32_t bitMask = (1 << bitSize) - 1; /* standard */
        const raw_t *constraints = mingraph_getCodedData(mingraph);
        uint32_t *couplesij = (uint32_t*) &constraints[nbConstraints];

        uint32_t consumed = 0; /* count consumed bits */
        uint32_t val_ij = *couplesij;
        
#ifdef EXPERIMENTAL
        uint32_t indices[dim*(dim-1)];
        size_t ni = 0;
#else
        /* keep track of touched clocks */
        uint32_t touched[bits2intsize(dim)];
        base_resetBits(touched, bits2intsize(dim));
#endif

        for(;;)
        {
            cindex_t i, j;

            /* integer decompression
             */
            i = val_ij & bitMask;
            val_ij >>= bitSize;
            j = val_ij & bitMask;
            val_ij >>= bitSize;
            consumed += bitSize + bitSize;

            /* write constraint
             */
            dbm[i*dim+j] = *constraints++;

#ifdef EXPERIMENTAL
            if (i)
            {
                indices[ni++] = i | (j << 16);
            }
#else
            /* Lower bounds on clock j do not cause it to be
             * invalidated, as the constraint needs to be combined
             * with some other constrained on clock j to be of use in
             * the closure operation. Therefore only touch i and j if
             * i is not zero (only when there are no upper bounds at
             * all does this imply that 0 is not touched - and in that
             * case 0 is of not use in the closure operation, so this
             * is fine).
             */
            if (i)
            {
                base_setOneBit(touched, i);
                base_setOneBit(touched, j);
            }
#endif
            if (!--nbConstraints) break;
            /* do not read new couples
             * if there is no constraint
             * left
             */
            assert(consumed <= 32);
            if (consumed == 32)
            {
                consumed = 0;
                val_ij = *++couplesij;
            }
        }
#ifdef EXPERIMENTAL
        if (ni) mingraph_close(indices, ni, dbm, dim);
#else
        dbm_closex(dbm, dim, touched);
#endif
    }
    return dim;
}

/* Read from a minimal graph with a list of couples (i,j)
 * telling which constraints are saved, and write to a DBM.
 * @param dbm: DBM to write
 * @param mingraph: minimal graph with couples (i,j) and
 * constraints on 16 bits to read.
 * @pre dbm is at least a raw_t[dim*dim], with dim = saved dimension.
 */
static
cindex_t mingraph_readFromMinCouplesij16(raw_t *dbm, const int32_t *mingraph)
{
    uint32_t info = mingraph_getInfo(mingraph);
    cindex_t dim = mingraph_readDim(info);

    /* reminder: size of indices of couples i,j = 4, 8, or 16 bits
     * the stored code is 0, 1, or 2, hence size = 2**(code + 2)
     * or 1 << (code + 2)
     */
    uint32_t bitSize = 1 << (mingraph_typeOfIJ(info) + 2);
    uint32_t bitMask = (1 << bitSize) - 1; /* standard */
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const int16_t *constraints = (int16_t*) mingraph_getCodedData(mingraph);
    const uint32_t *couplesij =        mingraph_jumpConstInt16(constraints, nbConstraints);
    uint32_t consumed = 0; /* count consumed bits */
    uint32_t val_ij = *couplesij;

#ifdef EXPERIMENTAL
    uint32_t indices[dim*(dim-1)];
    size_t ni = 0;
#else
    /* keep track of touched clocks */
    uint32_t touched[bits2intsize(dim)];
    base_resetBits(touched, bits2intsize(dim));
#endif

    assert(nbConstraints); /* can't be = 0 */
    assert(dbm && dim > 2);

    dbm_init(dbm, dim);

    for(;;)
    {
        cindex_t i, j;

        /* integer decompression
         */
        i = val_ij & bitMask;
        val_ij >>= bitSize;
        j = val_ij & bitMask;
        val_ij >>= bitSize;
        consumed += bitSize + bitSize;

        /* write constraint
         */
        dbm[i*dim+j] = mingraph_finite16to32(*constraints++);

#ifdef EXPERIMENTAL
        if (i)
        {
            indices[ni++] = i | (j << 16);
        }
#else
        /* see readFromMinCouplesij32 */
        if (i)
        {
            base_setOneBit(touched, i);
            base_setOneBit(touched, j);
        }
#endif
        if (!--nbConstraints) break;
        /* do not read new couples
         * if there is no constraint
         * left
         */
        assert(consumed <= 32);
        if (consumed == 32)
        {
            consumed = 0;
            val_ij = *++couplesij;
        }
    }
#ifdef EXPERIMENTAL
    if (ni) mingraph_close(indices, ni, dbm, dim);
#else
    dbm_closex(dbm, dim, touched);
#endif
    return dim;
}


/** Fatal error: should not be called.
 */
static
cindex_t mingraph_readError(raw_t *dbm, const int32_t *mingraph)
{
    fprintf(stderr,
            RED(BOLD) PACKAGE_STRING
            " fatal error: invalid encoded DBM to read"
            NORMAL "\n");
    exit(2);
    return 0; /* compiler happy */
}


/**************************************************
 * Format dependent bit matrix decoding functions *
 **************************************************/


/* We need to copy the DBM to its full format since we have
 * no way to compute its minimal graph from its copied format
 * (without the diagonal). After that we can re-analyze the DBM.
 * Complexity: O(dim^3)
 */
static
size_t mingraph_bitMatrixFromCopy32(uint32_t *bitMatrix, const int32_t *mingraph,
                                    BOOL isUnpacked, raw_t *buffer)
{
    cindex_t dim = isUnpacked
        ? mingraph_readDimFromPtr(mingraph)
        : mingraph_readFromCopy32(buffer, mingraph);
    assertx(!isUnpacked || dbm_isEqualToMinDBM(buffer, dim, mingraph));
    return dbm_analyzeForMinDBM(buffer, dim, bitMatrix);
}

/* Similarly for Copy16 */
static
size_t mingraph_bitMatrixFromCopy16(uint32_t *bitMatrix, const int32_t *mingraph,
                                    BOOL isUnpacked, raw_t *buffer)
{
    cindex_t dim = isUnpacked
        ? mingraph_readDimFromPtr(mingraph)
        : mingraph_readFromCopy16(buffer, mingraph);
    assertx(!isUnpacked || dbm_isEqualToMinDBM(buffer, dim, mingraph));
    return dbm_analyzeForMinDBM(buffer, dim, bitMatrix);
}


/* This function adds missing constraints to the minimal graph:
 * the constraints dbm[0,i] == dbm_LE_ZERO that were removed.
 * The problem is that some of them really belong to the minimal
 * graph and some do not, so we need to check this.
 * The argument function is the right function to call to fill the buffer with
 * the constraints of the saved part of the minimal graph.
 */
static
size_t mingraph_addMissingConstraints(uint32_t *matrix, const int32_t *mingraph,
                                      raw_t *dbm, cindex_t dim,
                                      size_t nbConstraints)
{
    /* See analyseForMinDBM */
    cindex_t first[dim+dim];      /* cindex_t[dim] */
    cindex_t *next = first + dim; /* cindex_t[dim] */
    cindex_t *q, *r, *end = first;
    const raw_t *dbm_idim = dbm; /* dbm[i*dim] */
    cindex_t i, j, k;
    assert(sizeof(cindex_t) == 2 || sizeof(cindex_t) == 4);
    base_resetSmall(first, ((dim+dim)*sizeof(cindex_t))/sizeof(int));
    
    /* zero cycles */
    i = 0;
    do {
        if (!next[i])
        {
            *end = i;
            k = i;
            if (i + 1 < dim)
            {
                const raw_t *dbm_jdim = dbm_idim + dim;
                j = i + 1;
                do {
                    if (dbm_raw2bound(dbm_idim[j]) == -dbm_raw2bound(dbm_jdim[i]))
                    {
                        next[k] = j;
                        k = j;
                    }
                    dbm_jdim += dim;
                } while (++j < dim);
            }
            next[k] = ~0; /* test bla != this bad value becomes ~bla */
            end++;
        }
        dbm_idim += dim;
    } while (++i < dim);
    
    /* reduction */
    assert(first[0] == 0 && first < end);
    for (q = first+1; q < end; q++) if (dbm[*q] == dbm_LE_ZERO && !base_readOneBit(matrix, *q))
    {
        for (r = first+1; r < end; r++) if (r != q)
        {
            raw_t bik = dbm[*r];
            raw_t bkj = dbm[(*r)*dim+(*q)];
            if (bik < dbm_LS_INFINITY &&
                bkj < dbm_LS_INFINITY &&
                dbm_LE_ZERO >= dbm_addFiniteFinite(bik, bkj))
            {
                goto continueEliminateEdges;
            }
        }
        assert(first[0] != *q); /* not diagonal */
        nbConstraints++;
        base_setOneBit(matrix, *q);
    continueEliminateEdges: ;
    }
    if (~next[0])
    {
        nbConstraints += mingraph_ngetAndSetBit(matrix, next[0]);
    }

    return nbConstraints;
}


/* It's easy to read the bit matrix since we have it
 * but we need to complete it with constraints that we
 * removed for saving purposes but that belong to the
 * minimal graph anyway.
 * Complexity: O(dim^2)
 */
static
size_t mingraph_bitMatrixFromBitMatrix32(uint32_t *matrix, const int32_t *mingraph,
                                         BOOL isUnpacked, raw_t *buffer)
{
    cindex_t dim = mingraph_readDimFromPtr(mingraph);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const raw_t *constraints = mingraph_getCodedData(mingraph);
    const uint32_t *minMatrix = (uint32_t*) &constraints[nbConstraints];

    /* copy bit matrix */
    base_copySmall(matrix, minMatrix, bits2intsize(dim*dim));

    /* read constraints if necessary */
    if (!isUnpacked)
    {
        mingraph_readFromMinBitMatrix32(buffer, mingraph);
    }

    /* complete the bit matrix */
    return mingraph_addMissingConstraints(matrix, mingraph, buffer, dim, nbConstraints);
}

/* Similar to previous */
static
size_t mingraph_bitMatrixFromBitMatrix16(uint32_t *matrix, const int32_t *mingraph,
                                         BOOL isUnpacked, raw_t *buffer)
{
    cindex_t dim = mingraph_readDimFromPtr(mingraph);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const int16_t *constraints = (int16_t*) mingraph_getCodedData(mingraph);
    const uint32_t *minMatrix =        mingraph_jumpConstInt16(constraints, nbConstraints);

    /* copy bit matrix */
    base_copySmall(matrix, minMatrix, bits2intsize(dim*dim));

    /* read constraints if necessary */
    if (!isUnpacked)
    {
        mingraph_readFromMinBitMatrix16(buffer, mingraph);
    }

    /* complete the bit matrix */
    return mingraph_addMissingConstraints(matrix, mingraph, buffer, dim, nbConstraints);
}

/* Similar to readFromCouplesij32 but write the bit matrix as well! */
static
size_t mingraph_bitMatrixFromCouplesij32(uint32_t *matrix, const int32_t *mingraph,
                                         BOOL isUnpacked, raw_t *dbm)
{
    uint32_t info = mingraph_getInfo(mingraph);
    cindex_t dim = mingraph_readDim(info);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);

    assert(dbm && (dim > 2 || nbConstraints == 0));
    base_resetSmall(matrix, bits2intsize(dim*dim));

    /* post-condition: valid DBM */
    if (!isUnpacked)
    {
        dbm_init(dbm, dim);
    }

    if (nbConstraints == 0)
    {
        /* then all lower bounds are part of the minimal graph */
        cindex_t i;
        for(i = 1; i < dim; ++i)
        {
            base_setOneBit(matrix, i);
        }
        return dim-1;
    }
    else
    {
        /* like readFromCouplesij32 but
         * - no dbm_init
         * - no dbm_close
         * - and write matrix.
         */
        uint32_t bitSize = 1 << (mingraph_typeOfIJ(info) + 2);
        uint32_t bitMask = (1 << bitSize) - 1; /* standard */
        const raw_t *constraints = mingraph_getCodedData(mingraph);
        uint32_t *couplesij = (uint32_t*) &constraints[nbConstraints];
        uint32_t consumed = 0; /* count consumed bits */
        uint32_t val_ij = *couplesij;
        size_t nb = nbConstraints;

        for(;;)
        {
            cindex_t i, j;

            /* integer decompression
             */
            i = val_ij & bitMask;
            val_ij >>= bitSize;
            j = val_ij & bitMask;
            val_ij >>= bitSize;
            consumed += bitSize + bitSize;

            /* write constraint + matrix
             */
            assert(!isUnpacked || dbm[i*dim+j] == *constraints);
            dbm[i*dim+j] = *constraints++;
            base_setOneBit(matrix, i*dim+j);

            if (!--nb) break;
            /* do not read new couples
             * if there is no constraint
             * left
             */
            assert(consumed <= 32);
            if (consumed == 32)
            {
                consumed = 0;
                val_ij = *++couplesij;
            }
        }

        if (!isUnpacked)
        {
            dbm_close(dbm, dim);
        }

        /* complete the bit matrix */
        return mingraph_addMissingConstraints(matrix, mingraph, dbm, dim, nbConstraints);
    }
}

/* Similar to previous */
static
size_t mingraph_bitMatrixFromCouplesij16(uint32_t *matrix, const int32_t *mingraph,
                                         BOOL isUnpacked, raw_t *dbm)
{
    uint32_t info = mingraph_getInfo(mingraph);
    cindex_t dim = mingraph_readDim(info);
    uint32_t bitSize = 1 << (mingraph_typeOfIJ(info) + 2);
    uint32_t bitMask = (1 << bitSize) - 1; /* standard */
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const int16_t *constraints = (int16_t*) mingraph_getCodedData(mingraph);
    const uint32_t *couplesij =        mingraph_jumpConstInt16(constraints, nbConstraints);
    uint32_t consumed = 0; /* count consumed bits */
    uint32_t val_ij = *couplesij;
    size_t nb = nbConstraints;

    assert(nbConstraints); /* can't be = 0 */
    assert(dbm && dim > 2);

    base_resetSmall(matrix, bits2intsize(dim*dim));
    if (!isUnpacked)
    {
        dbm_init(dbm, dim);
    }

    for(;;)
    {
        cindex_t i, j;

        /* integer decompression
         */
        i = val_ij & bitMask;
        val_ij >>= bitSize;
        j = val_ij & bitMask;
        val_ij >>= bitSize;
        consumed += bitSize + bitSize;

        /* write constraint + matrix
         */
        assert(!isUnpacked || dbm[i*dim+j] == mingraph_finite16to32(*constraints));
        dbm[i*dim+j] = mingraph_finite16to32(*constraints++);
        base_setOneBit(matrix, i*dim+j);

        if (!--nb) break;
        /* do not read new couples
         * if there is no constraint
         * left
         */
        assert(consumed <= 32);
        if (consumed == 32)
        {
            consumed = 0;
            val_ij = *++couplesij;
        }
    }

    if (!isUnpacked)
    {
        dbm_close(dbm, dim);
    }

    return mingraph_addMissingConstraints(matrix, mingraph, dbm, dim, nbConstraints);
}

/** Fatal error: should not be called.
 */
static
size_t mingraph_bitMatrixError(uint32_t *bitMatrix, const int32_t *mingraph, BOOL isUnpacked, raw_t *buffer)
{
    fprintf(stderr,
            RED(BOLD) PACKAGE_STRING
            " fatal error: invalid encoded DBM to read"
            NORMAL "\n");
    exit(2);
    return 0; /* compiler happy */
}

#ifdef EXPERIMENTAL
/* Experimental close */
static
void mingraph_close(uint32_t *indices1, size_t n, raw_t *dbm, cindex_t dim)
{
    /* Replace this with something experimental later */
    dbm_close(dbm, dim);
}
#endif
