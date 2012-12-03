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
 * Filename : mingraph_write.c (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph_write.c,v 1.19 2005/09/29 16:10:42 adavid Exp $
 *
 *********************************************************************/

#include "base/bitstring.h"
#include "dbm/mingraph.h"
#include "mingraph_coding.h"
#include "dbm.h"
#ifdef ENABLE_MINGRAPH_CACHE
#include "mingraph_cache.h"
#endif
#include "debug/macros.h"

/**
 * @file
 * Contains implementation of analysis of DBM to minimal
 * graph and its encoding.
 */


/*******************************
 * Functions used for encoding.
 *******************************/

/* Compute sizes and choose encoding, see below */
static
int32_t *mingraph_encode(const raw_t *dbm, cindex_t dim,
                         const uint32_t *bitMatrix, size_t cnt,
                         BOOL constraints16,
                         allocator_t c_alloc,
                         size_t offset);

/* Internal analysis function: compute minimal graph, see below */
static
size_t mingraph_analyzeForMinDBM(const raw_t *dbm, cindex_t dim,
                                 uint32_t *bitMatrix);

/* Trivial case for DBM of dimension 2, see below */
static
int32_t* mingraph_writeMinDBMDim2(const raw_t *dbm, cindex_t dim,
                                  allocator_t c_alloc,
                                  size_t offset);


/******************************************************
 * Specific encoding for different formats, see below.
 ******************************************************/

static
void mingraph_writeCopy32(int32_t *save, const raw_t *dbm, cindex_t dim);
static
void mingraph_writeCopy16(int32_t *save, const raw_t *dbm, cindex_t dim);
static
void mingraph_writeMinCouplesij32(int32_t *where,
                                  const raw_t *dbm, cindex_t dim,
                                  const uint32_t *bitMatrix, size_t cnt,
                                  uint32_t bitCode);
static
void mingraph_writeMinCouplesij16(int32_t *where,
                                  const raw_t *dbm, cindex_t dim,
                                  const uint32_t *bitMatrix, size_t cnt,
                                  uint32_t bitCode);
static
void mingraph_writeMinBitMatrix32(int32_t *where,
                                  const raw_t *dbm, cindex_t dim,
                                  const uint32_t *bitMatrix, size_t cnt);
static
void mingraph_writeMinBitMatrix16(int32_t *where,
                                  const raw_t *dbm, cindex_t dim,
                                  const uint32_t *bitMatrix, size_t cnt);


/************************************
 ********** Useful macros ***********
 ************************************/


/** Jump int16 integers, padded int32.
 * @param ints: int16 starting point (padded int32)
 * @param n: nb of int16 to jump
 * @return ints+n padded int32 as int32*
 */
static inline
uint32_t* mingraph_jumpInt16(int16_t *ints, size_t n)
{
    return ((uint32_t*)ints) + ((n + 1) >> 1);
}


/*****************************
 * Implementation of the API.
 *****************************/

/* Analyze a DBM:
 * - minimal graph reduction information
 * - maximal bits needed
 * - # of contraints needed to save
 * Callback to the real analysis function with a test
 * to bypass dim <= 2 (to respect the precondition).
 */
size_t dbm_analyzeForMinDBM(const raw_t *dbm, cindex_t dim,
                            uint32_t *bitMatrix)
{
    assert(dbm && dim && bitMatrix);
    assert(!dbm_isEmpty(dbm,dim));
    assert(dbm_isClosed(dbm, dim));

    /* Case where the analysis is useless since we
     * cannot reduce anything.
     */
    if (dim <= 2)
    {
        if (dim == 2)
        {
            if (dbm[2] == dbm_LS_INFINITY)
            {
                *bitMatrix = 2;
                return 1;
            }
            else
            {
                *bitMatrix = 2 | 4;
                return 2;
            }
        }
        else
        {
            *bitMatrix = 0;
            return 0;
        }
    }
    else
    {
        /* call internal function
         */
        return mingraph_analyzeForMinDBM(dbm, dim, bitMatrix);
    }
}


/* Analyze DBM and encode with cheapest (size) representation.
 * @see mingraph.h
 * Algorithm:
 * - case dim <= 2: little analysis, ignore the flags and
 *   try cheapest
 * - case minimize graph: analyze graph, maybe 16bits, and save
 * - otherwise: maybe analyze 16bits and save
 */
int32_t* dbm_writeToMinDBMWithOffset(const raw_t* dbm, cindex_t dim,
                                     BOOL minimizeGraph,
                                     BOOL tryConstraints16,
                                     allocator_t c_alloc,
                                     size_t offset)
{
    assert(dim <= 0xffff); /* fits on 16 bits */
    assert(dbm && dim);
    assert(c_alloc.allocFunction);
    assert(!dbm_isEmpty(dbm, dim));
    
    /* cannot reduce memory consumption
     */
    if (dim <= 2)
    {
        return mingraph_writeMinDBMDim2(dbm, dim, c_alloc, offset);
    }
    /* dim > 2 ; apply reduction */
    else if (minimizeGraph)
    {
        uint32_t bitMatrix[bits2intsize(dim*dim)];
        size_t cnt = mingraph_analyzeForMinDBM(dbm, dim, bitMatrix);
        cnt = dbm_cleanBitMatrix(dbm, dim, bitMatrix, cnt);

        /* check if there is any information at all
         */
        if (!cnt)
        {
            int32_t *mingraph = c_alloc.allocFunction(1 + offset, c_alloc.allocData);
            mingraph[offset] = dim |
                0x00040000 | /* minimal reduction */
                0x00020000;  /* couples i,j       */
                             /* and 0 constraint  */
            return mingraph;
        }
        else
        {
            /* choose the cheapest encoding
             */
            return mingraph_encode(dbm, dim, bitMatrix,
                                   cnt,
                                   tryConstraints16 &&
                                   (dbm_getMaxRange(dbm, dim) < dbm_LS_INF16),
                                   c_alloc, offset);
        }
    }
    else /* dim > 2 ; no reduction */
    {
        /* see which format for the constraints
         */
        BOOL constraints16 =
            tryConstraints16 &&
            (dbm_getMaxRange(dbm, dim) < dbm_LS_INF16);

        /* allocate
         */
        int32_t *mingraph =
            c_alloc.allocFunction(offset + 1 + /* + 1 : overhead for info */
                                  /* (x + bool) >> bool = conditional /2 rounded up   */
                                  ((dim*(dim-1) + constraints16) >> constraints16),
                                  c_alloc.allocData);

        (constraints16 ?
         mingraph_writeCopy16 :
         mingraph_writeCopy32)(mingraph + offset, dbm, dim);

        return mingraph;
    }
}


/* Switch function:
 * - no constraint -> trivial
 * - dim <= 2 -> easy
 * - otherwise encode normally
 */
int32_t* dbm_writeAnalyzedDBM(const raw_t *dbm, cindex_t dim,
                              uint32_t *bitMatrix,
                              size_t nbConstraints,
                              BOOL tryConstraints16,
                              allocator_t c_alloc,
                              size_t offset)
{
    assert(dim <= 0xffff); /* fits on 16 bits */
    assert(dbm && dim);
    assert(c_alloc.allocFunction);
    assert(!dbm_isEmpty(dbm, dim));
    
    if (dim >= 2)
    {
        nbConstraints = dbm_cleanBitMatrix(dbm, dim, bitMatrix, nbConstraints);
    }

    /* No constraint
     */
    if (!nbConstraints)
    {
        int32_t *mingraph = c_alloc.allocFunction(1 + offset, c_alloc.allocData);
        mingraph[offset] = dim |
            0x00040000 | /* minimal reduction */
            0x00020000;  /* couples i,j       */
                         /* and 0 constraint  */
        return mingraph;
    }

    /* cannot reduce memory consumption, ignore bitMatrix
     */
    if (dim <= 2)
    {
        return mingraph_writeMinDBMDim2(dbm, dim, c_alloc, offset);
    }

    return mingraph_encode(dbm, dim,
                           bitMatrix,
                           nbConstraints,
                           tryConstraints16 &&
                           (dbm_getMaxRange(dbm, dim) < dbm_LS_INF16),
                           c_alloc, offset);
}


/********************************************
 * Implementation of the encoding functions *
 ********************************************/


/** Estimate cheapest encoding and call the corresponding
 * encoding function.
 * @param dbm: dbm to save.
 * @param dim: dimension.
 * @param bitMatrix: bit matrix for the constraints to take.
 * @param cnt: number of constraints to save.
 * @param maxConstraint: maximal (!=infinity) constraint value.
 * @param allocFunction: allocation function.
 * @param allocData: custom data for the allocation function.
 * @param offset: offset to use for the allocation.
 * @return encoded minimal graph.
 * @pre dim > 2, otherwise always copy.
 */
static
int32_t *mingraph_encode(const raw_t *dbm, cindex_t dim,
                         const uint32_t *bitMatrix, size_t cnt,
                         BOOL constraints16,
                         allocator_t c_alloc,
                         size_t offset)
{
    size_t sizeForConstraints; /* to save the constraints (16/32 bits) */
    size_t sizeForInfo;        /* info may be on 1 or 2 ints           */
    size_t sizeForIndices;     /* to save the couples i,j              */

    size_t sizeIfCopy;         /* total size if copy                       */
    size_t sizeIfBitMatrix;    /* total size if min. red. with bit matrix  */
    size_t sizeIfCouplesij;    /* total size if min. red. with couples i,j */

    uint32_t bitCode = 0;        /* used to code bits of couples i,j */
    int32_t *mingraph;           /* result                           */

    assert(dim > 2);
    assert(cnt > 0);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim)) == cnt);

    /* Size to allocate for constraints
     */
    sizeForConstraints = (cnt + constraints16) >> constraints16;

    /* Test on maximal value we can save in 0xffc00000
     */
    sizeForInfo = cnt > 0x3ff ? 2 : 1;

    /* Size to allocate if copy is used
     */
    sizeIfCopy = 1 + /* overhead for info */
        /* conditional /2 rounded up */
        ((dim*(dim-1) + constraints16) >> constraints16);

    /* Size to allocate if bit matrix is used
     */
    sizeIfBitMatrix =
        sizeForInfo +          /* information */
        sizeForConstraints +   /* constraints */
        bits2intsize(dim*dim); /* bit matrix  */

    /* Representation of indices ij: 4, 8, 16 bits
     * 2* because we are saving couples (i,j).
     */
    if (dim <= 16)
    {
        /* 2 * cnt * 4bits = 8*cnt */
        bitCode = 0;
        sizeForIndices = bits2intsize(cnt << 3);
    }
    else if (dim <= 256)
    {
        /* 2 * cnt * 8bits = 16*cnt */
        bitCode = 1;
        sizeForIndices = bits2intsize(cnt << 4);
    }
    else
    {
        /* 2 * cnt * 16bits = 32*cnt */
        bitCode = 2;
        sizeForIndices = bits2intsize(cnt << 5);
    }

    /* Size to allocate if couples_ij are used
     */
    sizeIfCouplesij =
        sizeForInfo +          /* information */
        sizeForConstraints +   /* constraints */
        sizeForIndices;        /* indices i,j */

    /* Choose cheapest
     */
    if (sizeIfCopy <= sizeIfBitMatrix)
    {
        if (sizeIfCopy <= sizeIfCouplesij)
        {
            /* copy <= bit matrix and copy <= couplesij
             */
            mingraph = c_alloc.allocFunction(offset + sizeIfCopy,
                                             c_alloc.allocData);
            (constraints16 ?
             mingraph_writeCopy16 :
             mingraph_writeCopy32)(mingraph + offset, dbm, dim);
            
            return mingraph;
        }
        /* else we have
         * sizeIfCopy > sizeIfCouplesij &&
         * sizeIfCopy <= sizeIfBitMatrix
         * implies sizeIfCouplesij is the min
         */
    }
    else /* sizeIfCopy > sizeIfBitMatrix */
    {
        /* NOTE: we have to test >= and > to see
         * which one gives better performance.
         */
        if (sizeIfCouplesij >= sizeIfBitMatrix)
        {
            /* bit matrix < copy and bit matrix <= couplesij */
            mingraph = c_alloc.allocFunction(offset + sizeIfBitMatrix,
                                             c_alloc.allocData);
            (constraints16 ?
             mingraph_writeMinBitMatrix16 :
             mingraph_writeMinBitMatrix32)(mingraph + offset,
                                          dbm, dim,
                                          bitMatrix, cnt);
            return mingraph;
        }
        /* else we have
         * sizeIfCouplesij < sizeIfBitMatrix &&
         * sizeIfCopy > sizeIfBitMatrix
         * implies sizeIfCouplesij is the min
         */
    }

    /* couplesij cheapest */
    mingraph = c_alloc.allocFunction(offset + sizeIfCouplesij,
                                     c_alloc.allocData);
    (constraints16 ?
     mingraph_writeMinCouplesij16 :
     mingraph_writeMinCouplesij32)(mingraph + offset,
                                   dbm, dim,
                                   bitMatrix, cnt,
                                   bitCode);
    return mingraph;
}


/** Analyze a DBM (internal function):
 * - minimal graph reduction information
 * - maximal bits needed
 * - # of contraints needed to save
 * @param dbm: DBM to analyze
 * @param dim: dimension
 * @param bitMatrix: bit matrix to write results in
 * @pre
 * - dim > 2
 * - bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 */
static
size_t mingraph_analyzeForMinDBM(const raw_t *dbm, cindex_t dim, uint32_t *bitMatrix)
{
#ifdef ENABLE_MINGRAPH_CACHE
    uint32_t hashValue = dbm_hash(dbm, dim);
    size_t cnt = mingraph_getCachedResult(dbm, dim, bitMatrix, hashValue);
    if (cnt != 0xffffffff)
    {
        return cnt;
    }
    else
    {
#endif

    /* Data for analysis.
     * NOTE: in the original algorithm 'bitMatrix' was a bool[dim*dim].
     */
    cindex_t first[dim+dim];      /* cindex_t[dim] */
    cindex_t *next = first + dim; /* cindex_t[dim] */
    cindex_t *p, *q, *r, *end = first;

    /* pointer to avoid multiplication
     */
    const raw_t *dbm_idim = dbm; /* dbm[i*dim] */
    
    /* to go through the DBM
     */
    cindex_t i, j, k;
    
#ifndef ENABLE_MINGRAPH_CACHE
    size_t
#endif
        cnt = 0;
    
    assert(dbm && dim > 2 && bitMatrix);

    /* reset index tables and bit matrix
     */
    assert(sizeof(cindex_t) == 2 || sizeof(cindex_t) == 4);
    base_resetSmall(first, ((dim+dim)*sizeof(cindex_t))/sizeof(int));
    base_resetSmall(bitMatrix, bits2intsize(dim*dim));
    
    /* NOTE: do not try to compute max range
     * at the same time because we will mess up
     * with some unchecked rows, which may be wrong.
     */

    /* Identify equivalence classes: if xi-xj == constant then
     * xi and xj are in the same equivalence class.
     */
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
                    /* right pointer arithmetics
                     */
                    assert(dbm_idim == &dbm[i*dim]);
                    assert(dbm_jdim == &dbm[j*dim]);
                    
                    /* cij + cji == 0 => both constraints are <=
                     */
                    assert(dbm_raw2bound(dbm[i*dim+j]) +
                           dbm_raw2bound(dbm[j*dim+i]) != 0
                           || (dbm[i*dim+j] & dbm[j*dim+i] & 1) == 1);

                    /* same equivalence class ; don't
                     * test cij+cji==0 directly because may overflow
                     */
                    if (dbm_raw2bound(dbm_idim[j]) == -dbm_raw2bound(dbm_jdim[i]))
                    {
                        next[k] = j;
                        k = j;
                    }
                    dbm_jdim += dim;
                } while (++j < dim);
            }
            /* does not matter if next[] is made of uint16 or uint32 */
            next[k] = ~0; /* test bla != this bad value becomes ~bla */
            end++;
        }
        dbm_idim += dim;
    } while (++i < dim);
    
    /* original algorithm:
     * 
     * end = first;
     * for (i = 0; i < size; i++)
     *     if (!next[i]) {
     *         *end = i;
     *         k = i;
     *         for (j = i + 1; j < size; j++)
     *             if (getBound(i,j) + getBound(j,i) == 0)
     *             {
     *                 next[k] = j;
     *                 k = j;
     *             }
     *         next[k] = -1;
     *         end++;
     *     }
     */
    
    /* Eliminate redundant edges (by finding concise edges)
     * take one representant for every equivalence classe.
     */
    
    for (p = first; p < end; p++)
    {
        for (q = first; q < end; q++)
        {
            raw_t bij;
            assert(*p < dim);
            assert(*q < dim);
            bij = dbm[(*p)*dim+(*q)];
            if (p != q && bij < dbm_LS_INFINITY)
            {
                for (r = first; r < end; r++)
                {
                    assert(*r < dim);
                    if (r != p && r != q)
                    {
                        raw_t bik = dbm[(*p)*dim+(*r)];
                        raw_t bkj = dbm[(*r)*dim+(*q)];
                        if (bik < dbm_LS_INFINITY &&
                            bkj < dbm_LS_INFINITY &&
                            bij >= dbm_addFiniteFinite(bik, bkj))
                        {
                            goto continueEliminateEdges;
                        }
                    }
                }
                assert(*p != *q); /* not diagonal */
                cnt += mingraph_ngetAndSetBit(bitMatrix, (*p)*dim+(*q));
            }
        continueEliminateEdges: ;
        }
    }
    
    /* Original algorithm:
     *
     * for (p = first; p < end; p++)
     *     for (q = first; q < end; q++) {
     *            bij = getRawBnd(*p, *q);
     *            if (p != q && bij < (infinity << 1)) {
     *                for (r = first; r < end; r++) {
     *                    if (r != p && r != q)
     *                    {
     *                        bik = getRawBnd(*p, *r);
     *                        bkj = getRawBnd(*r, *q);
     *                        if (bik < (infinity << 1) && bkj < (infinity << 1))
     *                        {
     *                            if (bij >= rawAdd(bik, bkj))
     *                            {
     *                                goto cont;
     *                            }
     *                        }
     *                    }
     *                }
     *                if (!bitMatrix[*p * size + *q]) cnt++;
     *                bitMatrix[*p * size + *q] = 1;
     *            }
     *         cont: ;
     *     }
     */
    
    /* Mark concise edges in zero-cycles: graph reduction.
     */
    
    for (p = first; p < end; p++)
    {
        i = *p;
        assert(i < dim);
        if (~next[i])
        {
            do {
                assert(next[i] < dim);
                assert(i < dim);
                cnt += mingraph_ngetAndSetBit(bitMatrix, i*dim+next[i]);
                i = next[i];
            } while(~next[i]);
            assert(i != (*p)); /* not diagonal */
            cnt += mingraph_ngetAndSetBit(bitMatrix, i*dim+(*p));
        }
    }
    
    /* Original algorithm:
     *
     * for (p = first; p < end; p++) {
     *     i = *p;
     *     if (next[i] != -1) {
     *           do {
     *               if (!bitMatrix[i * size + next[i]]) cnt++;
     *               bitMatrix[i * size + next[i]] = 1;
     *               i = next[i];
     *           } while (next[i] != -1);
     *           if (!bitMatrix[i * size + *p]) cnt++;
     *           bitMatrix[i * size + *p] = 1;
     *     }
     * }
     */

#ifndef NDEBUG
    /* Check that the constraints on the diagonal are not marked. */
    for(i = 0; i < dim; ++i)
    {
        assert(!base_readOneBit(bitMatrix, i*dim+i));
    }
#endif
    
#ifdef ENABLE_MINGRAPH_CACHE
    mingraph_putCachedResult(dbm, dim, bitMatrix, hashValue, cnt);
    return cnt;
    }
#else
    return cnt; /* # of constraints */
#endif
}


size_t dbm_cleanBitMatrix(const raw_t *dbm, cindex_t dim,
                          uint32_t *bitMatrix, size_t nbConstraints)
{
    cindex_t j;
    assert(dbm && dim && bitMatrix && (*bitMatrix & 1) == 0);
    
    /* Remove edges stating the clocks are positive */

    for(j = 1; j < dim; ++j)
    {
        if (dbm[j] >= dbm_LE_ZERO && base_getOneBit(bitMatrix, j))
        {
            base_toggleOneBit(bitMatrix, j); /* better than resetBit */
            assert(nbConstraints > 0);
            nbConstraints--;
        }
    }
    
    /* Original algorithm:
     *
     * for (j = 1; j < size; j++) {
     *     if (bitMatrix[j] && getRawBnd(0, j) >= 1) {
     *         cnt--;
     *         bitMatrix[j] = 0;
     *     }
     * }
     */
    
    return nbConstraints;
}


/** Save function for dim <= 2.
 * @param dbm,dim: DBM of dimension dim
 * @param allocFunction, allocData: allocation
 * function and its data
 * @param offset: offset to use for allocation
 * @pre dim <= 2
 * @return allocated memory with mingraph in it.
 */
static
int32_t* mingraph_writeMinDBMDim2(const raw_t *dbm, cindex_t dim,
                                  allocator_t c_alloc,
                                  size_t offset)
{
    int32_t *mingraph;
    raw_t maxBits = 0;

    assert(dbm && dim <= 2);
    assert(c_alloc.allocFunction);
    
    if (dim <= 1)
    {
        mingraph = c_alloc.allocFunction(1 + offset, c_alloc.allocData);
        mingraph[offset] = dim;
        return mingraph;
    }

    /* check for invalid values
     */
    assert(dbm_isValidRaw(dbm[1]));
    assert(dbm_isValidRaw(dbm[2]));
    
    /* trivial DBM?
     */
    if (dbm[1] == dbm_LE_ZERO &&
        dbm[2] == dbm_LS_INFINITY)
    {
        mingraph = c_alloc.allocFunction(1 + offset, c_alloc.allocData);
        mingraph[offset] =
            2 |          /* dimension         */
            0x00040000 | /* minimal reduction */
            0x00020000;  /* couples i,j       */
        /* and 0 constraint  */
        return mingraph;
    }

    /* for dim == 2, it is cheap and easy
     * to try to save on 16 bits: just do it.
     */
    
    /* compute maximal # of bits needed
     */
    ADD_BITS(maxBits, dbm[1]);
    ADD_BITS(maxBits, dbm[2]);
    
    if (maxBits >= dbm_LS_INF16)
    {
        /* 3: info + 2 constraints (no diagonal)
         */
        mingraph = c_alloc.allocFunction(3 + offset, c_alloc.allocData);
        mingraph[offset] = dim;
        mingraph[offset + 1] = dbm[1];
        mingraph[offset + 2] = dbm[2];
    }
    else
    {
        int16_t *data16;
        
        /* note: infinity on 32 bits is
         * automatically converted to infinity on 16 bits.
         */
        mingraph = c_alloc.allocFunction(2 + offset, c_alloc.allocData);
        data16 = (int16_t*) &mingraph[offset + 1];
        mingraph[offset] = dim | 0x00010000; /* 16 bits */
        data16[0] = mingraph_raw32to16(dbm[1]);
        data16[1] = mingraph_raw32to16(dbm[2]);
    }

    return mingraph;
}


/* Copy of DBM without diagonal, coded 32 bits.
 * @param save: where to write
 * @param dbm: DBM to copy
 * @param dim: dimension
 * @pre dim > 2
 */
static
void mingraph_writeCopy32(int32_t *save, const raw_t *dbm, cindex_t dim)
{
    size_t nbLines = dim - 1;
    assert(dim > 2);
    
    *save++ = (int32_t) dim; /* info ; see encoding format */

    do {
        size_t nbCols = dim; /* between diagonal elements */
        dbm++;                 /* jump diagonal */
        do {
            *save++ = *dbm++;
        } while(--nbCols);
    } while(--nbLines);
}


/* Copy of DBM without diagonal, coded 16 bits.
 * @param save: where to write
 * @param dbm: DBM to copy
 * @param dim: dimension
 * @pre dim > 2
 */
static
void mingraph_writeCopy16(int32_t *save, const raw_t *dbm, cindex_t dim)
{
    size_t nbLines = dim - 1;
    int16_t *save16;

    assert(dim > 2);
    
    *save++ = (int32_t) dim |
        0x00010000; /* 16 bits flag ; see encoding format */
    save16 = (int16_t*) save;

    do {
        size_t nbCols = dim;
        dbm++;
        do {
            *save16++ = mingraph_raw32to16(*dbm++);
        } while(--nbCols);
    } while(--nbLines);
}


/* Save only specified constraints on 32 bits and couples i,j
 * @param where: where to write the encoded data
 * @param dbm: DBM to copy
 * @param dim: dimension
 * @param bitMatrix: bit matrix obtained from the minimal
 * graph reduction analysis. Meaning is:
 * save constraint dbm(i,j) = dbm[i*dim+j] if bit i*dim+j
 * is set.
 * @param cnt: number of constraints == number of bits set
 * in bitMatrix
 * @param bitCode: how to code the indices for the couples
 * (i,j) that tell which constraints are saved. Meaning:
 * 0->4 bits, 1->8 bits, 2->16 bits per index.
 * @pre
 * - dim > 2
 * - cnt == base_countBitsN(bitMatrix, bits2intsize(dim*dim))
 * - bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 */
static
void mingraph_writeMinCouplesij32(int32_t *where,
                                 const raw_t *dbm, cindex_t dim,
                                 const uint32_t *bitMatrix, size_t cnt,
                                 uint32_t bitCode)
{
    size_t manyConstraints = (cnt > 0x3ff);
    int32_t *constraints; /* where to write the constraints */
    uint32_t *couplesij;  /* where to write the couples i,j */

    cindex_t i,j;     /* indices to save              */
    uint32_t val_ij;  /* encoded value of i,j         */
    uint32_t shift;   /* how much we have to shift    */
    uint32_t bitSize; /* size in bits of indices      */

    assert(dim > 2);
    assert(bitCode <= 2);
    assert(cnt > 0);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim)) == cnt);

    /* encode information : see encoding format
     */
    *where = dim |
        0x00040000 |      /* minimal graph     */
        0x00020000 |      /* couples i,j       */
        (bitCode << 19) | /* format of indices */
        (manyConstraints << 21);

    if (manyConstraints)
    {
        where[1] = cnt;
        constraints = where + 2;
    }
    else
    {
        *where |= cnt << 22;
        constraints = where + 1;
    }

    /* the couples i,j are written after the constraints
     * note: cast only because of unsigned int
     */
    couplesij = (uint32_t*) constraints + cnt;

    i = 0;
    j = 0;
    val_ij = 0;
    shift = 0;
    bitSize = 1 << (bitCode + 2); /* = 2**(bitCode + 2) */
    /* note: 2**(0+2) = 4, 2**(1+2) = 8, 2**(2+2) = 16  */

    for(;;)
    {
        uint32_t b, count;
        for(b = *bitMatrix++, count = 32; b != 0; ++j, ++dbm, --count, b >>= 1)
        {
            for(; (b & 1) == 0; ++j, ++dbm, --count, b >>= 1)
            {
                assert(count);
            }
            FIX_IJ();
            assert(i < dim && j < dim && i != j);
            /* integer compression */
            val_ij |= (i << shift);
            shift += bitSize;
            val_ij |= (j << shift);
            shift += bitSize;
            *constraints++ = *dbm;

            if (!--cnt) /* no constraint left */
            {
                *couplesij = val_ij;
                return;
            }
            assert(shift <= 32);
            if (shift == 32) /* integer full - flush */
            {
                shift = 0;
                *couplesij++ = val_ij;
                val_ij = 0;
            }
            assert(count);
        }
        j += count;     /* count: #of unread bits left */
        dbm += count;   /* so jump unread elements     */
    }   
}


/* Save only specified constraints on 16 bits and couples i,j
 * @param where: where to write the encoded data
 * @param dbm: DBM to copy
 * @param dim: dimension
 * @param bitMatrix: bit matrix obtained from the minimal
 * graph reduction analysis. Meaning is:
 * save constraint dbm(i,j) = dbm[i*dim+j] if bit i*dim+j
 * is set.
 * @param cnt: number of constraints == number of bits set
 * in bitMatrix
 * @param bitCode: how to code the indices for the couples
 * (i,j) that tell which constraints are saved. Meaning:
 * 0->4 bits, 1->8 bits, 2->16 bits per index.
 * @pre
 * - dim > 2
 * - cnt == base_countBitsN(bitMatrix, bits2intsize(dim*dim))
 * - bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 */
static
void mingraph_writeMinCouplesij16(int32_t *where,
                                 const raw_t *dbm, cindex_t dim,
                                 const uint32_t *bitMatrix, size_t cnt,
                                 uint32_t bitCode)
{
    size_t manyConstraints = (cnt > 0x3ff);
    int16_t *constraints; /* where to write the constraints */
    uint32_t *couplesij;  /* where to write the couples i,j */

    cindex_t i,j;     /* indices to save              */
    uint32_t val_ij;  /* encoded value of i,j         */
    uint32_t shift;   /* how much we have to shift    */
    uint32_t bitSize; /* size in bits of indices      */

    /* As everything is 32 bits aligned, we want to be sure
     * to have a known full int32_t. If the number of constraints
     * is odd, then we reset the next int16_t to enforce a non
     * random value. This also allows for correct equality
     * testing between 2 minimal representations.
     */
    BOOL resetLast16 = (BOOL)(cnt & 1);

    assert(dim > 2);
    assert(bitCode <= 2);
    assert(cnt > 0);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim)) == cnt);

    /* encode information : see encoding format
     */
    *where = dim |
        0x00040000 |      /* minimal graph     */
        0x00020000 |      /* couples i,j       */
        0x00010000 |      /* 16 bits           */
        (bitCode << 19) | /* format of indices */
        (manyConstraints << 21);

    if (manyConstraints)
    {
        where[1] = cnt;
        constraints = (int16_t*) &where[2];
    }
    else
    {
        *where |= cnt << 22;
        constraints = (int16_t*) &where[1];
    }

    /* the couples i,j are written after the constraints
     */
    couplesij = mingraph_jumpInt16(constraints, cnt);

    i = 0;
    j = 0;
    val_ij = 0;
    shift = 0;
    bitSize = 1 << (bitCode + 2); /* = 2**(bitCode + 2) */
    /* note: 2**(0+2) = 4, 2**(1+2) = 8, 2**(2+2) = 16  */

    for(;;)
    {
        uint32_t b, count;
        for(b = *bitMatrix++, count = 32; b != 0; ++j, ++dbm, --count, b >>= 1)
        {
            for(; (b & 1) == 0; ++j, ++dbm, --count, b >>= 1)
            {
                assert(count);
            }
            FIX_IJ();
            assert(i < dim &&  j < dim && i != j);
            /* integer compression */
            val_ij |= (i << shift);
            shift += bitSize;
            val_ij |= (j << shift);
            shift += bitSize;
            assert(*dbm < dbm_LS_INF16 && -*dbm < dbm_LS_INF16);
            *constraints++ = mingraph_finite32to16(*dbm);
            
            if (!--cnt) /* no constraint left */
            {
                *couplesij = val_ij; /* flush */
                if (resetLast16) *constraints = 0;
                return;
            }
            assert(shift <= 32);
            if (shift == 32) /* integer full - flush */
            {
                shift = 0;
                *couplesij++ = val_ij;
                val_ij = 0;
            }
            assert(count);
        }
        j += count;     /* count: #of unread bits left */
        dbm += count;   /* so jump unread elements     */
    }
}


/* Save only specified constraints on 32 bits and the bit
 * matrix that tells which constraints they are.
 * @param where: where to write the encoded data
 * @param dbm: DBM to copy
 * @param dim: dimension
 * @param bitMatrix: bit matrix obtained from the minimal
 * graph reduction analysis. Meaning is:
 * save constraint dbm(i,j) = dbm[i*dim+j] if bit i*dim+j
 * is set.
 * @param cnt: number of constraints == number of bits set
 * in bitMatrix
 * @pre
 * - dim > 2
 * - cnt == base_countBitsN(bitMatrix, bits2intsize(dim*dim))
 * - bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 */
static
void mingraph_writeMinBitMatrix32(int32_t *where,
                                 const raw_t *dbm, cindex_t dim,
                                 const uint32_t *bitMatrix, size_t cnt)
{
    size_t manyConstraints = (cnt > 0x3ff);
    int32_t *constraints; /* where to write the constraints */
    const raw_t *dbmBase;

    assert(dim > 2);
    assert(cnt > 0);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim)) == cnt);

    /* encode information
     */
    *where = dim |
        0x00040000 | /* minimal graph */
        (manyConstraints << 21);

    if (manyConstraints)
    {
        where[1] = cnt;
        constraints = where + 2;
    }
    else
    {
        *where |= cnt << 22;
        constraints = where + 1;
    }

    /* the bit matrix is written after the constraints
     */
    base_copySmall(constraints + cnt,
                   bitMatrix, bits2intsize(dim*dim));

    for(dbmBase = dbm;; dbmBase += 32, dbm = dbmBase)
    {
        uint32_t b;
        for(b = *bitMatrix++; b != 0; ++dbm, b >>= 1)
        {
            for(; (b & 1) == 0; ++dbm, b >>= 1);
            *constraints++ = *dbm;
            if (!--cnt) /* no constraint left */
            {
                return;
            }
        }
    }
}


/* Save only specified constraints on 16 bits and the bit
 * matrix that tells which constraints they are.
 * @param where: where to write the encoded data
 * @param dbm: DBM to copy
 * @param dim: dimension
 * @param bitMatrix: bit matrix obtained from the minimal
 * graph reduction analysis. Meaning is:
 * save constraint dbm(i,j) = dbm[i*dim+j] if bit i*dim+j
 * is set.
 * @param cnt: number of constraints == number of bits set
 * in bitMatrix
 * @pre
 * - dim > 2
 * - cnt == base_countBitsN(bitMatrix, bits2intsize(dim*dim))
 * - bitMatrix is a uint32_t[bits2intsize(dim*dim)]
 */
static
void mingraph_writeMinBitMatrix16(int32_t *where,
                                 const raw_t *dbm, cindex_t dim,
                                 const uint32_t *bitMatrix, size_t cnt)
{
    size_t manyConstraints = (cnt > 0x3ff);
    int16_t *constraints; /* where to write the constraints */
    const raw_t *dbmBase;

    /* As everything is 32 bits aligned, we want to be sure
     * to have a known full int32_t. If the number of constraints
     * is odd, then we reset the next int16_t to enforce a non
     * random value. This also allows for correct equality
     * testing between 2 minimal representations.
     */
    BOOL resetLast16 = (BOOL)(cnt & 1);

    assert(dim > 2);
    assert(cnt > 0);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim)) == cnt);

    /* encode information
     */
    *where = dim |
        0x00040000 | /* minimal graph */
        0x00010000 | /* 16 bits       */
        (manyConstraints << 21);

    if (manyConstraints)
    {
        where[1] = cnt;
        constraints = (int16_t*) &where[2];
    }
    else
    {
        *where |= cnt << 22;
        constraints = (int16_t*) &where[1];
    }

    /* the bit matrix is written after the constraints
     * warning: align on 32 bits
     */
    base_copySmall(mingraph_jumpInt16(constraints, cnt),
                   bitMatrix, bits2intsize(dim*dim));

    for(dbmBase = dbm;; dbmBase += 32, dbm = dbmBase)
    {
        uint32_t b;
        for(b = *bitMatrix++; b != 0; ++dbm, b >>= 1)
        {
            for(; (b & 1) == 0; ++dbm, b >>= 1);
            *constraints++ = mingraph_finite32to16(*dbm);
            if (!--cnt) /* no constraint left */
            {
                if (resetLast16) *constraints = 0;
                return;
            }
        }
    }
}
