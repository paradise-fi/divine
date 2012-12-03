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
 * Filename : mingraph_equal.c (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph_equal.c,v 1.10 2005/09/29 16:10:42 adavid Exp $
 *
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "base/bitstring.h"
#include "dbm/mingraph.h"
#include "mingraph_coding.h"
#include "debug/macros.h"


/**
 * @file
 * Contains implementation of the equality
 * functions from the API: these are switch
 * functions to encoding specific functions.
 */


/***************************************
 * Equality tests without pre-analysis *
 ***************************************/

static
BOOL mingraph_isEqualToCopy32(const raw_t *dbm, cindex_t dim,
                              const int32_t *mingraph);
static
BOOL mingraph_isEqualToCopy16(const raw_t *dbm, cindex_t dim,
                              const int32_t *mingraph);
static
BOOL mingraph_isEqualToMinBitMatrix32(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph);
static
BOOL mingraph_isEqualToMinBitMatrix16(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph);
static
BOOL mingraph_isEqualToMinCouplesij32(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph);
static
BOOL mingraph_isEqualToMinCouplesij16(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph);
static
BOOL mingraph_isEqualError(const raw_t *dbm, cindex_t dim,
                           const int32_t *mingraph);


/************************************
 * Equality tests with pre-analysis *
 ************************************/

static
BOOL mingraph_isAnalyzedDBMEqualToCopy32(const raw_t *dbm, cindex_t dim,
                                         uint32_t *bitMatrix,
                                         size_t *nbConstraints,
                                         const int32_t *mingraph);
static
BOOL mingraph_isAnalyzedDBMEqualToCopy16(const raw_t *dbm, cindex_t dim,
                                         uint32_t *bitMatrix,
                                         size_t *nbConstraints,
                                         const int32_t *mingraph);
static
BOOL mingraph_isAnalyzedDBMEqualToMinBitMatrix32(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *bitMatrix,
                                                 size_t *nbConstraints,
                                                 const int32_t *mingraph);
static
BOOL mingraph_isAnalyzedDBMEqualToMinBitMatrix16(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *bitMatrix,
                                                 size_t *nbConstraints,
                                                 const int32_t *mingraph);
static
BOOL mingraph_isAnalyzedDBMEqualToMinCouplesij32(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *bitMatrix,
                                                 size_t *nbConstraints,
                                                 const int32_t *mingraph);
static
BOOL mingraph_isAnalyzedDBMEqualToMinCouplesij16(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *bitMatrix,
                                                 size_t *nbConstraints,
                                                 const int32_t *mingraph);
static
BOOL mingraph_isAnalyzedDBMEqualError(const raw_t *dbm, cindex_t dim,
                                      uint32_t *bitMatrix,
                                      size_t *nbConstraints,
                                      const int32_t *mingraph);


/*********************************************
 ****** Implementation of the API ************
 *********************************************/


/* Types of equality test functions.
 */
typedef BOOL (*equalDBM_f)(const raw_t*, cindex_t, const int32_t*);
typedef BOOL (*analyzedEqualDBM_f)(const raw_t*, cindex_t,
                                   uint32_t*, size_t*,
                                   const int32_t*);


/* Similar to read: decode the right format and use
 * the proper equality test function.
 */
BOOL dbm_isEqualToMinDBM(const raw_t *dbm, cindex_t dim,
                         const int32_t *minDBM)
{
    /* see mingraph_getTypeIndex comments
     */
    static const equalDBM_f isEqualTo[8] =
    {
        mingraph_isEqualToCopy32,
        mingraph_isEqualToCopy16,
        mingraph_isEqualError,
        mingraph_isEqualError,
        mingraph_isEqualToMinBitMatrix32,
        mingraph_isEqualToMinBitMatrix16,
        mingraph_isEqualToMinCouplesij32,
        mingraph_isEqualToMinCouplesij16
    };

    assert(dim <= 0xffff); /* fits on 16 bits */
    assert(dbm && dim);
    assert(minDBM && *minDBM);
    assert(dbm_isClosed(dbm, dim));

    return (*minDBM == 1) ?
        (dim == 1 && *dbm == dbm_LE_ZERO) :              /* trivial case or */
        (dim == mingraph_readDimFromPtr(minDBM) &&       /* same dimension */
         isEqualTo[mingraph_getTypeIndexFromPtr(minDBM)] /* and same DBM */
         (dbm, dim, minDBM));
}


/* Equality test in case the DBM was analyzed before.
 */
BOOL dbm_isAnalyzedDBMEqualToMinDBM(const raw_t *dbm, cindex_t dim,
                                    uint32_t *bitMatrix,
                                    size_t *nbConstraints,
                                    const int32_t* minDBM)
{
     /* see mingraph_getTypeIndex comments
     */
    static const analyzedEqualDBM_f isEqualTo[8] =
    {
        mingraph_isAnalyzedDBMEqualToCopy32,
        mingraph_isAnalyzedDBMEqualToCopy16,
        mingraph_isAnalyzedDBMEqualError,
        mingraph_isAnalyzedDBMEqualError,
        mingraph_isAnalyzedDBMEqualToMinBitMatrix32,
        mingraph_isAnalyzedDBMEqualToMinBitMatrix16,
        mingraph_isAnalyzedDBMEqualToMinCouplesij32,
        mingraph_isAnalyzedDBMEqualToMinCouplesij16
    };

    assert(dim <= 0xffff); /* fits on 16 bits */
    assert(dbm && dim);
    assert(minDBM && *minDBM);
    assert(dbm_isClosed(dbm, dim));
    assert(nbConstraints);

    return (*minDBM == 1) ?
        (dim == 1 && *dbm == dbm_LE_ZERO) :              /* trivial case or */
        (dim == mingraph_readDimFromPtr(minDBM) &&       /* same dimension */
         isEqualTo[mingraph_getTypeIndexFromPtr(minDBM)] /* and same DBM */
         (dbm, dim, bitMatrix, nbConstraints, minDBM));   
}


/* Unpack minDBM if needed only
 */
BOOL dbm_isUnpackedEqualToMinDBM(const raw_t *dbm, cindex_t dim,
                                 const int32_t* minDBM,
                                 raw_t *unpackBuffer)
{
    uint32_t info = mingraph_getInfo(minDBM);

    if (mingraph_isMinimal(info))
    {
        dbm_readFromMinDBM(unpackBuffer, minDBM);
        return dbm_areEqual(dbm, unpackBuffer, dim);
    }
    else
    {
        return (mingraph_isCoded16(info) ?
                mingraph_isEqualToCopy16 :
                mingraph_isEqualToCopy32)(dbm, dim, minDBM);
    }
}


/*******************************************************
 * Implementation of specific equality test functions. *
 *******************************************************/


/* Compare a DBM with a stored DBM in the format
 * copy without diagonal with constraints on 32 bits.
 * The algorithm is optimistic: it will perform well
 * for equal DBMs, which is mostly the case since this
 * is supposed to be called for equality checking when
 * sharing DBMs.
 * @param dbm: DBM to test.
 * @param dim: dimension
 * @param mingraph: stored DBM in specified format.
 */
static
BOOL mingraph_isEqualToCopy32(const raw_t *dbm, cindex_t dim,
                              const int32_t *mingraph)
{
    /* constraints after info */
    const raw_t *saved = (raw_t*) &mingraph[1];
    size_t nbLines = dim - 1;
    int32_t difference = 0;

    /* only DBMs of equal dimensions are comparable
     */
    assert(dim == mingraph_readDimFromPtr(mingraph));
    assert(dim > 1);

    do {
        size_t nbCols = dim;
        assert(*dbm == dbm_LE_ZERO); /* diagonal */
        dbm++;
        do {
            difference |= *dbm++ ^ *saved++;
        } while(--nbCols);

    } while(--nbLines);
    assert(*dbm == dbm_LE_ZERO); /* diagonal */

    return (BOOL)(difference == 0);
}


/** Wrapper function to the normal
 * copy test: ignore bitMatrix & nbConstraints.
 * @pre mingraph saved in copy32 format.
 * @see mingraph_isEqualToCopy32
 */
BOOL mingraph_isAnalyzedDBMEqualToCopy32(const raw_t *dbm, cindex_t dim,
                                         uint32_t *bitMatrix, size_t *nbConstraints,
                                         const int32_t *mingraph)
{
    return mingraph_isEqualToCopy32(dbm, dim, mingraph);
}


/* Compare a DBM with a stored DBM in the format
 * copy without diagonal with constraints on 16 bits.
 * The algorithm is optimistic: it will perform well
 * for equal DBMs, which is mostly the case since this
 * is supposed to be called for equality checking when
 * sharing DBMs.
 * @param dbm: DBM to test.
 * @param dim: dimension
 * @param mingraph: stored DBM in specified format.
 */
static
BOOL mingraph_isEqualToCopy16(const raw_t *dbm, cindex_t dim,
                              const int32_t *mingraph)
{
    /* constraints after info */
    const int16_t *saved = (int16_t*) &mingraph[1];
    size_t nbLines = dim - 1;
    int32_t difference = 0;
    
    /* only DBMs of equal dimensions are comparable
     */
    assert(dim == mingraph_readDimFromPtr(mingraph));
    assert(dim > 1);
        
    do {
        size_t nbCols = dim;
        assert(*dbm == dbm_LE_ZERO); /* diagonal */
        dbm++;
        do {
            /* restore infinity and signed int
             */
            difference |= *dbm++ ^ mingraph_raw16to32(*saved++);
        } while(--nbCols);
    } while(--nbLines);
    assert(*dbm == dbm_LE_ZERO); /* diagonal */
    
    return (BOOL)(difference == 0);
}


/** Wrapper function to the normal
 * copy test: ignore bitMatrix & nbConstraints.
 * @pre mingraph saved in copy16 format.
 * @see mingraph_isEqualToCopy16
 */
static
BOOL mingraph_isAnalyzedDBMEqualToCopy16(const raw_t *dbm, cindex_t dim,
                                         uint32_t *bitMatrix,
                                         size_t *nbConstraints,
                                         const int32_t *mingraph)
{
    return mingraph_isEqualToCopy16(dbm, dim, mingraph);
}


/* Compare a DBM with a stored DBM in the format
 * minimal graph with bit matrix telling which
 * constraints are saved, with constraints on 32 bits.
 * The algorithm is optimistic: it will perform well
 * for equal DBMs, which is mostly the case since this
 * is supposed to be called for equality checking when
 * sharing DBMs.
 * @param dbm: DBM to test.
 * @param dim: dimension
 * @param srcBitMatrix: bit matrix obtained from minimal graph
 * analysis function.
 * @param srcNbConstraints: number of constraints in the minimal graph
 * @param mingraph: stored DBM in specified format.
 */
static
BOOL mingraph_isAnalyzedDBMEqualToMinBitMatrix32(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *srcBitMatrix,
                                                 size_t *srcNbConstraints,
                                                 const int32_t *mingraph)
{
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const raw_t *constraints = mingraph_getCodedData(mingraph);
    uint32_t *bitMatrix = (uint32_t*) &constraints[nbConstraints];
    int32_t difference = 0;
    const raw_t *dbmBase;

    assert(dim == mingraph_readDimFromPtr(mingraph));
    assert(dbm && dim > 2 && nbConstraints);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim))
           == nbConstraints);

    *srcNbConstraints = dbm_cleanBitMatrix(dbm, dim, srcBitMatrix, *srcNbConstraints);
    
    if (nbConstraints != *srcNbConstraints ||
        !base_areEqual(bitMatrix, srcBitMatrix, bits2intsize(dim*dim)))
    {
        return FALSE;
    }

    /* Now it is safe to only check for equality
     * of the constraints in the minimal graph
     */

    for(dbmBase = dbm;; dbmBase += 32, dbm = dbmBase)
    {
        uint32_t b;
        for(b = *bitMatrix++; b != 0; ++dbm, b >>= 1)
        {
            for(; (b & 1) == 0; ++dbm, b >>= 1);
            difference |= *dbm ^ *constraints;
            if (!--nbConstraints)
            {
                return (BOOL)(difference == 0);
            }
            constraints++;
        }
    }
}


/* Expensive wrapper: analyze DBM and test for equality.
 * @see mingraph_isAnalyzedDBMEqualToMinBitMatrix32
 */
static
BOOL mingraph_isEqualToMinBitMatrix32(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph)
{
    uint32_t bitMatrix[bits2intsize(dim*dim)];
    size_t nbConstraints = dbm_analyzeForMinDBM(dbm, dim, bitMatrix);
    return mingraph_isAnalyzedDBMEqualToMinBitMatrix32(dbm, dim,
                                                       bitMatrix, &nbConstraints,
                                                       mingraph);
}


/* Compare a DBM with a stored DBM in the format
 * minimal graph with bit matrix telling which
 * constraints are saved, with constraints on 16 bits.
 * The algorithm is optimistic: it will perform well
 * for equal DBMs, which is mostly the case since this
 * is supposed to be called for equality checking when
 * sharing DBMs.
 * @param dbm: DBM to test.
 * @param dim: dimension
 * @param srcBitMatrix: bit matrix obtained from minimal graph
 * analysis function.
 * @param srcNbConstraints: number of constraints in the minimal graph
 * @param mingraph: stored DBM in specified format.
 */
static
BOOL mingraph_isAnalyzedDBMEqualToMinBitMatrix16(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *srcBitMatrix,
                                                 size_t *srcNbConstraints,
                                                 const int32_t *mingraph)
{
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const int16_t *constraints = (int16_t*) mingraph_getCodedData(mingraph);
    const uint32_t *bitMatrix =        mingraph_jumpConstInt16(constraints, nbConstraints);
    int32_t difference = 0;
    const raw_t *dbmBase;

    assert(dim == mingraph_readDimFromPtr(mingraph));
    assert(dbm && dim > 2);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim))
           == nbConstraints);

    *srcNbConstraints = dbm_cleanBitMatrix(dbm, dim, srcBitMatrix, *srcNbConstraints);

    if (nbConstraints != *srcNbConstraints ||
        !base_areEqual(bitMatrix, srcBitMatrix, bits2intsize(dim*dim)))
    {
        return FALSE;
    }

    /* Now it is safe to only check for equality
     * of the constraints in the minimal graph
     */

    /* similar to save */
    for(dbmBase = dbm;; dbmBase += 32, dbm = dbmBase)
    {
        uint32_t b;
        for(b = *bitMatrix++; b != 0; ++dbm, b >>= 1)
        {
            for(; (b & 1) == 0; ++dbm, b >>= 1);
            difference |= *dbm ^ mingraph_finite16to32(*constraints);
            if (!--nbConstraints)
            {
                return (BOOL)(difference == 0);
            }
            constraints++;
        }
    }
}


/* Expensive wrapper: analyze DBM and test for equality.
 * @see mingraph_isAnalyzedDBMEqualToMinBitMatrix16
 */
static
BOOL mingraph_isEqualToMinBitMatrix16(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph)
{
    uint32_t bitMatrix[bits2intsize(dim*dim)];
    size_t nbConstraints = dbm_analyzeForMinDBM(dbm, dim, bitMatrix);
    return mingraph_isAnalyzedDBMEqualToMinBitMatrix16(dbm, dim,
                                                       bitMatrix, &nbConstraints,
                                                       mingraph);
}


/* Compare a DBM with a stored DBM in the format
 * minimal graph with list of couples (i,j) telling which
 * constraints are saved, with constraints on 32 bits.
 * The algorithm is optimistic: it will perform well
 * for equal DBMs, which is mostly the case since this
 * is supposed to be called for equality checking when
 * sharing DBMs.
 * @param dbm: DBM to test.
 * @param dim: dimension
 * @param srcBitMatrix: bit matrix obtained from minimal graph
 * analysis function.
 * @param srcNbConstraints: number of constraints in the minimal graph
 * @param mingraph: stored DBM in specified format.
 */
static
BOOL mingraph_isAnalyzedDBMEqualToMinCouplesij32(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *srcBitMatrix,
                                                 size_t *srcNbConstraints,
                                                 const int32_t *mingraph)
{
    uint32_t info = mingraph_getInfo(mingraph);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
        
    *srcNbConstraints = dbm_cleanBitMatrix(dbm, dim, srcBitMatrix, *srcNbConstraints);

    if (*srcNbConstraints != nbConstraints)
    {
        return FALSE;
    }

    assert(dim == mingraph_readDim(info));
    assert(dbm && (dim > 2 || nbConstraints == 0));
    
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
        int32_t difference = 0;

        for(;;)
        {
            cindex_t i,j;

            /* integer decompression
             */
            i = val_ij & bitMask;
            val_ij >>= bitSize;
            j = val_ij & bitMask;
            val_ij >>= bitSize;
            consumed += bitSize + bitSize;
            
            /* Accumulate differences: constraints and bit matrix
             */
            difference |= (dbm[i*dim+j] ^ *constraints)
                | (base_getOneBit(srcBitMatrix, i*dim+j) ^ 1);

            if (!--nbConstraints)
            {
                return (BOOL)(difference == 0);
            }
            constraints++;

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
    }
    else
    {
        return dbm_isEqualToInit(dbm, dim);
    }
}


/* Expensive wrapper: analyze DBM and test for equality.
 * @see mingraph_isAnalyzedDBMEqualToMinCouplesij32
 */
static
BOOL mingraph_isEqualToMinCouplesij32(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph)
{
    uint32_t bitMatrix[bits2intsize(dim*dim)];
    size_t nbConstraints = dbm_analyzeForMinDBM(dbm, dim, bitMatrix);
    return mingraph_isAnalyzedDBMEqualToMinCouplesij32(dbm, dim,
                                                       bitMatrix, &nbConstraints,
                                                       mingraph);
}


/* Compare a DBM with a stored DBM in the format
 * minimal graph with list of couples (i,j) telling which
 * constraints are saved, with constraints on 16 bits.
 * The algorithm is optimistic: it will perform well
 * for equal DBMs, which is mostly the case since this
 * is supposed to be called for equality checking when
 * sharing DBMs.
 * @param dbm: DBM to test.
 * @param dim: dimension
 * @param srcBitMatrix: bit matrix obtained from minimal graph
 * analysis function.
 * @param srcNbConstraints: number of constraints in the minimal graph
 * @param mingraph: stored DBM in specified format.
 */
static
BOOL mingraph_isAnalyzedDBMEqualToMinCouplesij16(const raw_t *dbm, cindex_t dim,
                                                 uint32_t *srcBitMatrix,
                                                 size_t *srcNbConstraints,
                                                 const int32_t *mingraph)
{
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);

    *srcNbConstraints = dbm_cleanBitMatrix(dbm, dim, srcBitMatrix, *srcNbConstraints);

    if (*srcNbConstraints == nbConstraints)
    {
        uint32_t info = mingraph_getInfo(mingraph);

        /* reminder: size of indices of couples i,j = 4, 8, or 16 bits
         * the stored code is 0, 1, or 2, hence size = 2**(code + 2)
         * or 1 << (code + 2)
         */
        uint32_t bitSize = 1 << (mingraph_typeOfIJ(info) + 2);
        uint32_t bitMask = (1 << bitSize) - 1; /* standard mask */
        const int16_t *constraints = (int16_t*) mingraph_getCodedData(mingraph);
        const uint32_t *couplesij =
            mingraph_jumpConstInt16(constraints, nbConstraints);
    
        uint32_t consumed = 0; /* count consumed bits */
        uint32_t val_ij = *couplesij;
        int32_t difference = 0;

        assert(dim == mingraph_readDim(info));
        assert(nbConstraints); /* can't be = 0 */
        assert(dbm && dim > 2);
        
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
            
            /* Accumulate differences: constraints and bit matrix
             */
            difference |= (dbm[i*dim+j] ^ mingraph_finite16to32(*constraints))
                | (base_getOneBit(srcBitMatrix, i*dim+j) ^ 1);
            
            if (!--nbConstraints)
            {
                return (BOOL)(difference == 0);
            }
            constraints++;
            
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
    }

    return FALSE;
}


/* Expensive wrapper: analyze DBM and test for equality.
 * @see mingraph_isAnalyzedDBMEqualToMinCouplesij16
 */
static
BOOL mingraph_isEqualToMinCouplesij16(const raw_t *dbm, cindex_t dim,
                                      const int32_t *mingraph)
{
    uint32_t bitMatrix[bits2intsize(dim*dim)];
    size_t nbConstraints = dbm_analyzeForMinDBM(dbm, dim, bitMatrix);
    return mingraph_isAnalyzedDBMEqualToMinCouplesij16(dbm, dim,
                                                       bitMatrix, &nbConstraints,
                                                       mingraph);
}



/* Fatal error: should not be called.
 */
static
BOOL mingraph_isEqualError(const raw_t *dbm, cindex_t dim,
                           const int32_t *mingraph)
{
    fprintf(stderr,
            RED(BOLD) PACKAGE_STRING
            " fatal error: invalid encoded DBM to compare with"
            NORMAL "\n");
    exit(2);
    return FALSE; /* compiler happy */
}


/* Fatal error: should not be called.
 */
static
BOOL mingraph_isAnalyzedDBMEqualError(const raw_t *dbm, cindex_t dim,
                                      uint32_t *bitMatrix,
                                      size_t *nbConstraints,
                                      const int32_t *mingraph)
{
    fprintf(stderr,
            RED(BOLD) PACKAGE_STRING
            " fatal error: invalid encoded DBM to compare with"
            NORMAL "\n");
    exit(2);
    return FALSE; /* compiler happy */
}
