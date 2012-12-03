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
 * Filename : mingraph.c (dbm)
 *
 * Implementation of minimal graph reduction. Note: we choose dynamically
 * which format to use to reduce memory footprint of the allocated data.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph.c,v 1.19 2005/06/02 14:13:07 adavid Exp $
 *
 *********************************************************************/

#include "base/bitstring.h"
#include "dbm/mingraph.h"
#include "mingraph_coding.h"
#include "debug/macros.h"


/**
 * @file
 * Miscellanous functions of the mingraph.h
 * API: get dimension, get size, convex union,
 * get coding type.
 * @see mingraph.h
 */


/***************************************************
 * Convex union functions: for coding of the DBM on
 * 16/32 bits without the diagonal.
 ***************************************************/

static
void mingraph_convexUnion16(raw_t *dbm, const int32_t *minDBM, cindex_t dim);
static
void mingraph_convexUnion32(raw_t *dbm, const int32_t *minDBM, cindex_t dim);


/***************************************************
 ********** Implementation of the API **************
 ***************************************************/


/* Convert bit matrix to list of indices.
 */
void dbm_bitMatrix2indices(const uint32_t *bits, size_t nbConstraints, uint32_t *index, cindex_t dim)
{
    uint32_t i,j,k;
    DODEBUG(const uint32_t *bitMatrix = bits);
    DODEBUG(size_t nbC = nbConstraints);

    assert(bits && index && dim);
    assert(nbConstraints == base_countBitsN(bits, bits2intsize(dim*dim)));

    /* safeguard */
    if (nbConstraints == 0) return;

    for(k = 0, i = 0, j = 0; ;)
    {
        uint32_t count,b;
        /* jump efficiently unset bits in sparse matrices */
        for(b = *bits++, count = 32; b != 0; ++j, --count, b >>= 1)
        {
            /* b != 0 => the loop will terminate
             * look for one bit set */
            for( ; (b & 1) == 0; ++j, --count, b >>= 1)
            {
                assert(count);
            }
            FIX_IJ();

            /* write index */
            index[k++] = i | (j << 16);

            /* terminate */
            if (--nbConstraints == 0)
            {
#ifndef NDEBUG
                /* paranoid checks */
                assert(nbC == k);
                for(k = 0; k < nbC; ++k)
                {
                    i = index[k] & 0xffff;
                    j = index[k] >> 16;
                    assert(base_getOneBit(bitMatrix, i*dim+j));
                }
#endif
                return;
            }
            assert(count);
        }
        j += count; /* number of unread bits */
    }
}


/* Decode information.
 */
cindex_t dbm_getDimOfMinDBM(const int32_t *minDBM)
{
    return mingraph_readDimFromPtr(minDBM);
}


/* Decode the coded information and recompute the needed size
 * for allocation.
 */
size_t dbm_getSizeOfMinDBM(const int32_t *minDBM)
{
    uint32_t info;    /* type information                   */
    uint32_t coded16; /* if constraints on 16 bits (0 or 1) */
    
    assert(minDBM);

    info = mingraph_getInfo(minDBM);
    coded16 = (info & 0x00010000) >> 16;

    /* if minimal reduction is used
     */
    if (mingraph_isMinimal(info))
    {
        /* # of saved constraints
         */
        size_t nbConstraints = mingraph_getNbConstraints(minDBM);

        /* size of header and constraints:
         * 1 : informatin
         * & 0x00200000 >> 21 : use another int if bit is set
         * (x + coded16) >> coded16 : conditional /2 rounded up
         */
        uint32_t headerAndConstraints =
            1 +
            ((info & 0x00200000) >> 21) +
            ((nbConstraints + coded16) >> coded16);

        if (mingraph_isCodedIJ(info))
        {
            /* type (0,1,2) + 2 => gives the power of
             * 2 of the size in bits (4,8,16) but we save
             * couples so *2, hence +3
             */
            uint32_t bitCode = mingraph_typeOfIJ(info);
            return headerAndConstraints +
                bits2intsize(nbConstraints << (bitCode + 3));
        }
        else
        {
            /* add the bit matrix
             */
            cindex_t dim = mingraph_readDim(info);
            return headerAndConstraints +
                bits2intsize(dim*dim);
        }
    }
    else /* simply copy */
    {
        cindex_t dim = mingraph_readDim(info);
        assert(dim >= 1);

        /* 1 : size of info
         * dim*(dim-1) : # of constraints
         * (x + coded16) >> coded16 : conditional /2 rounded up
         */
        return 1 + ((dim*(dim-1) + coded16) >> coded16);
    }
}


/* Algorithm: unpack to the full DBM format and compute
 * the convex union.
 * If encoded as minimal graph and no constraints, then
 * init dbm.
 */
void dbm_convexUnionWithMinDBM(raw_t *dbm, cindex_t dim,
                               const int32_t *minDBM, raw_t *unpackBuffer)
{
    assert(dim <= 0xffff); /* fits on 16 bits */
    assert(dbm && dim);
    assert(dbm_getDimOfMinDBM(minDBM) == dim);
    assert(unpackBuffer);

    /* if something to do!
     */
    if (dim > 1)
    {
        uint32_t info = mingraph_getInfo(minDBM);

        if (mingraph_isMinimal(info))
        {
            /* avoid unpack for trivial DBMs
             */
            if (mingraph_getNbConstraints(minDBM))
            {
                dbm_readFromMinDBM(unpackBuffer, minDBM);
                dbm_convexUnion(dbm, unpackBuffer, dim);
            }
            else
            {
                /* anything union trivial DBM
                 * = trivial DBM
                 */
                dbm_init(dbm, dim);
            }
        }
        else
        {
            (mingraph_isCoded16(info) ?
             mingraph_convexUnion16 :
             mingraph_convexUnion32)(dbm, minDBM, dim);
        }
    }
}


/* Decode information and return the proper type (visible
 * from outside, not related to exact internal code).
 */
representationOfMinDBM_t dbm_getRepresentationType(const int32_t *minDBM)
{
    /* see mingraph_getTypeIndex comments
     */
    static const representationOfMinDBM_t codeTypes[8] =
    {
        dbm_MINDBM_COPY32,      /* 0x00000000 copy, 32 bits                  */
        dbm_MINDBM_COPY16,      /* 0x00010000 copy, 16 bits                  */
        dbm_MINDBM_ERROR,       /* 0x00020000 invalid                        */
        dbm_MINDBM_ERROR,       /* 0x00030000 invalid                        */
        dbm_MINDBM_BITMATRIX32, /* 0x00040000 min. red. bit matrix, 32 bits  */
        dbm_MINDBM_BITMATRIX16, /* 0x00050000 min. red. bit matrix, 16 bits  */
        dbm_MINDBM_TUPLES32,    /* 0x00060000 min. red. couples i,j, 32 bits */
        dbm_MINDBM_TUPLES16     /* 0x00070000 min. red. couples i,j, 16 bits */
    };

    assert(minDBM && *minDBM);

    return (*minDBM == 1) ?
        dbm_MINDBM_TRIVIAL :
        codeTypes[mingraph_getTypeIndexFromPtr(minDBM)];
}


/********************************************************
 ********* Format dependent functions *******************
 ********************************************************/


/* Particular convex union for copy encoding on 16 bits
 * similar to read/save with a test for the max constraint.
 * @param dbm: DBM to compute the union with.
 * @param minDBM: DBM representation on 16 bits without diagonal.
 * @param dim: dimension of DBM
 */
static
void mingraph_convexUnion16(raw_t *dbm, const int32_t *minDBM, cindex_t dim)
{
    const int16_t *saved16 = (int16_t*) &minDBM[1]; /* jump info */
    size_t nbLines = dim - 1;

    assert(dbm && minDBM);
    assert(dim > 1);
    
    do {
        size_t nbCols = dim; /* between diagonal elements */
        dbm++;                 /* jump diagonal */
        do {
            /* reconstruct the constraint on 32 bits:
             * special case for infinity and then
             * restore a signed int on 32 bits.
             */
            raw_t saved32 = mingraph_raw16to32(*saved16);
            if (saved32 > *dbm) *dbm = saved32;
            saved16++;
            dbm++;
        } while(--nbCols);
    } while(--nbLines);
}


/* Particular convex union for copy encoding on 32 bits
 * similar to read/save with a test for the max constraint.
 * @param dbm: DBM to compute the union with.
 * @param minDBM: DBM representation on 32 bits without diagonal.
 * @param dim: dimension of DBM
 */
static
void mingraph_convexUnion32(raw_t *dbm, const int32_t *minDBM, cindex_t dim)
{
    const raw_t *savedDBM = (raw_t*) &minDBM[1]; /* jump info */
    size_t nbLines = dim - 1;

    assert(dbm && minDBM);
    assert(dim > 1);
    
    do {
        size_t nbCols = dim; /* between diagonal elements */
        dbm++;                 /* jump diagonal */
        do {
            if (*savedDBM > *dbm) *dbm = *savedDBM;
            savedDBM++;
            dbm++;
        } while(--nbCols);
    } while(--nbLines);
}
