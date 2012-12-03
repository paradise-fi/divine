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
 * Filename : mingraph_relation.c (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph_relation.c,v 1.11 2005/06/20 12:42:45 behrmann Exp $
 *
 *********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "base/bitstring.h"
#include "dbm/mingraph.h"
#include "mingraph_coding.h"
#include "debug/macros.h"

/**
 * @file
 * Contains implementation of the relation function:
 * it is a switch to functions depending on the encoding.
 */


/*********************************************************
 * Relation (<, >, ==, !=) functions, depend on encoding *
 *********************************************************/

static
relation_t mingraph_relationWithCopy32(const raw_t *dbm, cindex_t dim,
                                       const int32_t *mingraph,
                                       raw_t *unpackBuffer, BOOL *isExact);
static
relation_t mingraph_relationWithCopy16(const raw_t *dbm, cindex_t dim,
                                       const int32_t *mingraph,
                                       raw_t *unpackBuffer, BOOL *isExact);
static
relation_t mingraph_relationWithMinBitMatrix32(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *unpackBuffer, BOOL *isExact);
static
relation_t mingraph_relationWithMinBitMatrix16(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *unpackBuffer, BOOL *isExact);
static
relation_t mingraph_relationWithMinCouplesij32(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *unpackBuffer, BOOL *isExact);
static
relation_t mingraph_relationWithMinCouplesij16(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *unpackBuffer, BOOL *isExact);
static
relation_t mingraph_relationError(const raw_t *dbm, cindex_t dim,
                                  const int32_t *mingraph,
                                  raw_t *unpackBuffer, BOOL *isExact);


/****************************
 * Implementation of the API
 ****************************/


/* Type of the relation functions.
 */
typedef relation_t (*relation_f)(const raw_t*, cindex_t, const int32_t*, raw_t*, BOOL*);

/* only relations with min. red. representations,
 * not exact relations.
 */
static const relation_f relationWithMinDBM[8] = 
{
    mingraph_relationWithCopy32,
    mingraph_relationWithCopy16,
    mingraph_relationError,
    mingraph_relationError,
    mingraph_relationWithMinBitMatrix32,
    mingraph_relationWithMinBitMatrix16,
    mingraph_relationWithMinCouplesij32,
    mingraph_relationWithMinCouplesij16
};


/* Try to check for subset, if fails and if exactRelation
 * is set then unpack and use the exact relation on it.
 * In this implementation we try not to unpack if possible.
 */
relation_t dbm_relationWithMinDBM(const raw_t *dbm, cindex_t dim,
                                  const int32_t *minDBM,
                                  raw_t *unpackBuffer)
{
    uint32_t info;
    BOOL isExact = FALSE; /* by default */
    relation_t rel;

    assert(dim <= 0xffff); /* fits on 16 bits */
    assert(dbm && dim);
    assert(minDBM && *minDBM);
    assert(dbm_isClosed(dbm, dim));

    /* trivial case if only ref clock
     */
    if (*minDBM == 1)
    {
        return
            (dim == 1 && *dbm == dbm_LE_ZERO) ?
            (unpackBuffer ? base_EQUAL : base_SUBSET) :
            base_DIFFERENT;
    }

    /* check dimension
     */
    info = mingraph_getInfo(minDBM);
    if (mingraph_readDim(info) != dim)
    {
        return base_DIFFERENT;
    }

    /* unpackBuffer is not necessary for the
     * minimal graph functions but it is used for
     * the copy cases to be sure we give the
     * same coherent answer, ie, not exact
     * when asked not to be exact.
     */
    rel = relationWithMinDBM[mingraph_getTypeIndex(info)]
        (dbm, dim, minDBM, unpackBuffer, &isExact);

    /* not minimal (no mingraph) && unpackBuffer not null => isExact */
    assert(mingraph_isMinimal(info) || unpackBuffer == NULL || isExact);

    /* return rel if the result can be trusted, specific implementations
     * know about it and it may depends on the result.
     */
    if (!unpackBuffer || isExact)
    {
        return rel;
    }
    else /* expensive unpack and full relation */
    {
        dbm_readFromMinDBM(unpackBuffer, minDBM);
        return dbm_relation(dbm, unpackBuffer, dim);
    }
}


/* similar to previous relation but
 * may avoid to unpack in half the cases
 */
relation_t dbm_approxRelationWithMinDBM(const raw_t *dbm, cindex_t dim,
                                        const int32_t *minDBM,
                                        raw_t *unpackBuffer)
{
    uint32_t info;
    BOOL isExact = FALSE; /* by default */
    relation_t rel;

    assert(dim <= 0xffff); /* fits on 16 bits */
    assert(dbm && dim);
    assert(minDBM && *minDBM);
    assert(dbm_isClosed(dbm, dim));

    /* trivial case if only ref clock
     */
    if (*minDBM == 1)
    {
        return
            (dim == 1 && *dbm == dbm_LE_ZERO) ?
            (unpackBuffer ? base_EQUAL : base_SUBSET) :
            base_DIFFERENT;
    }

    /* check dimension
     */
    info = mingraph_getInfo(minDBM);
    if (mingraph_readDim(info) != dim)
    {
        return base_DIFFERENT;
    }

    rel = relationWithMinDBM[mingraph_getTypeIndex(info)]
        (dbm, dim, minDBM, unpackBuffer, &isExact);

    assert(mingraph_isMinimal(info) || unpackBuffer == NULL || isExact);

    /* satisfied if base_SUBSET, don't care if exact or not in that case */
    if (!unpackBuffer || rel == base_SUBSET || isExact)
    {
        return rel;
    }
    else /* exact relation, isMinimal, result == base_DIFFERENT */
    {
        dbm_readFromMinDBM(unpackBuffer, minDBM);
        return dbm_relation(dbm, unpackBuffer, dim);
    }
}


/******************************************************
 * Implementation of the different relation functions *
 ******************************************************/


/* Relation between a DBM and a stored DBM in format
 * copy without diagonal on 32 bits.
 * @param dbm: DBM to test
 * @param dim: dimension
 * @param mingraph: stored DBM format
 * @param unpackBuffer: if NULL then enough with approximate
 * answer, if != NULL, exact answer required.
 * @return 
 * - exact relation of dbm (<=,>,!=) mingraph
 *   if unpackBuffer != NULL
 * - approximate relation of dbm (<=,!=) mingraph
 *   if unpackBuffer == NULL
 * Algorithm:
 * - check ==
 * - if fails, check for >= or <=
 */
static
relation_t mingraph_relationWithCopy32(const raw_t *dbm, cindex_t dim,
                                       const int32_t *mingraph,
                                       raw_t *unpackBuffer, BOOL *isExact)
{
    size_t nbLines = dim - 1;
    size_t nbCols;
    const raw_t *saved = (raw_t*) mingraph;

#ifdef NLONGER_CODE /* seems slower after all */

    /* We may want an approximate relation */
    uint32_t superset = unpackBuffer != NULL;
    uint32_t subset = 1;
    if (superset)
    {
        *isExact = TRUE;
    }

    assert(dbm && dim > 1);
    assert(mingraph);
    assert(dim == mingraph_readDimFromPtr(mingraph));

    do {
        nbCols = dim;
        assert(*dbm == dbm_LE_ZERO); /* diagonal */

        do {
            ++dbm;
            ++saved;
            superset &= *dbm >= *saved;
            subset &= *dbm <= *saved;
        } while(--nbCols);

        if (!(superset | subset))
        {
            return base_DIFFERENT;
        }
        dbm++; /* diagonal */
        assert(*dbm == dbm_LE_ZERO); /* diagonal */
    } while(--nbLines);    

    assert(base_SUPERSET == 1 && base_SUBSET == 2);
    return (relation_t)((subset << 1) | superset);

#else
    assert(dbm && dim > 1);
    assert(mingraph);
    assert(dim == mingraph_readDimFromPtr(mingraph));

    /* We use a kind of partial evaluation of the comparison
     * algorithm. Basically the same code is repeated three times:
     *
     * 1. Assuming that the two DBMs are equal. If this turns out
     *    not to be the case, we jump to the same point in copy 2 
     *    or 3.
     * 2. Assuming that the first DBM is a subset of the second.
     * 3. Assuming that the first DBM is a superset of the second.
     *
     * Special case: If an unpackBuffer is not provided, we are only
     * interesting in distinguishing between SUBSET and DIFFERENT.
     */

    if (unpackBuffer)
    {
        *isExact = TRUE;

        /* check dbm == mingraph
         */
        do {
            nbCols = dim;
            assert(*dbm == dbm_LE_ZERO); /* diagonal */
            do {
                dbm++;
                saved++;
                if (*dbm != *saved)
                {
                    if (*dbm > *saved)
                    {
                        goto TrySuperSet32;
                    }
                    else
                    {
                        goto TrySubSet32;
                    }
                }
            } while (--nbCols);
            dbm++; /* diagonal */
            assert(*dbm == dbm_LE_ZERO); /* diagonal */
        } while (--nbLines);
        
        return base_EQUAL;
    }

    /* check dbm < mingraph
     */
    do
    {
        nbCols = dim;
        do {
            dbm++;
            saved++;
            if (*dbm > *saved)
            {
                return base_DIFFERENT;
            }
        TrySubSet32: 
            ;
        } while (--nbCols);
        dbm++; /* diagonal */
        assert(*dbm == dbm_LE_ZERO);
    } while (--nbLines);
    return base_SUBSET;

    /* check dbm > mingraph
     */
    do
    {
        nbCols = dim;
        do {
            dbm++;
            saved++;
            if (*dbm < *saved)
            {
                return base_DIFFERENT;
            }
        TrySuperSet32:
            ;
        } while (--nbCols);
        dbm++; /* diagonal */
        assert(*dbm == dbm_LE_ZERO);
    } while (--nbLines);
    return base_SUPERSET;
#endif /* NLONGER_CODE */
}


/* Relation between a DBM and a stored DBM in format
 * copy without diagonal on 16 bits.
 * @param dbm: DBM to test
 * @param dim: dimension
 * @param mingraph: stored DBM format
 * @param unpackBuffer: if NULL then enough with approximate
 * answer, if != NULL, exact answer required.
 * @return 
 * - exact relation of dbm (<=,>,!=) mingraph
 *   if unpackBuffer != NULL
 * - approximate relation of dbm (<=,!=) mingraph
 *   if unpackBuffer == NULL
 * Algorithm:
 * - check ==
 * - if fails, check for >= or <=
 */
static
relation_t mingraph_relationWithCopy16(const raw_t *dbm, cindex_t dim,
                                       const int32_t *mingraph,
                                       raw_t *unpackBuffer, BOOL *isExact)
{
    const int16_t *saved = (int16_t*) &mingraph[1];
    size_t nbLines = dim - 1;
    size_t nbCols;

#ifdef NLONGER_CODE /* seems slower after all */

    /* We may want an approximate relation */
    uint32_t superset = unpackBuffer != NULL;
    uint32_t subset = 1;
    if (superset)
    {
        *isExact = TRUE;
    }

    assert(dbm && dim > 1);
    assert(mingraph);
    assert(dim == mingraph_readDimFromPtr(mingraph));

    do {
        nbCols = dim;
        assert(*dbm == dbm_LE_ZERO); /* diagonal */

        do {
            raw_t saved32 = mingraph_raw16to32(*saved);
            ++dbm;
            ++saved; /* read 1st, increment 2nd */
            superset &= *dbm >= saved32;
            subset &= *dbm <= saved32;
        } while(--nbCols);

        if (!(superset | subset))
        {
            return base_DIFFERENT;
        }
        dbm++; /* diagonal */
        assert(*dbm == dbm_LE_ZERO); /* diagonal */
    } while(--nbLines);    

    assert(base_SUPERSET == 1 && base_SUBSET == 2);
    return (relation_t)((subset << 1) | superset);

#else

    assert(dbm && dim > 1);
    assert(mingraph);
    assert(dim == mingraph_readDimFromPtr(mingraph));

    /* We use a kind of partial evaluation of the comparison
     * algorithm. Basically the same code is repeated three times:
     *
     * 1. Assuming that the two DBMs are equal. If this turns out
     *    not to be the case, we jump to the same point in copy 2 
     *    or 3.
     * 2. Assuming that the first DBM is a subset of the second.
     * 3. Assuming that the first DBM is a superset of the second.
     *
     * Special case: If an unpackBuffer is not provided, we are only
     * interesting in distinguishing between SUBSET and DIFFERENT.
     */

    if (unpackBuffer != NULL)
    {
        *isExact = TRUE;
        
        /* check dbm == mingraph
         */
        do {
            nbCols = dim;
            assert(*dbm == dbm_LE_ZERO); /* diagonal */
            do {
                dbm++;
                if (*dbm != mingraph_raw16to32(*saved))
                {
                    if (*dbm > mingraph_raw16to32(*saved))
                    {
                        goto TrySuperSet16;
                    }
                    else
                    {
                        goto TrySubSet16;
                    }                    
                }
                saved++;
            } while(--nbCols);
            dbm++; /* diagonal */
            assert(*dbm == dbm_LE_ZERO);
        } while(--nbLines);

        return base_EQUAL;
    }

    /* check dbm < mingraph
     */
    do
    {
        nbCols = dim;
        do {
            dbm++;
            if (*dbm > mingraph_raw16to32(*saved))
            {
                return base_DIFFERENT;
            }
        TrySubSet16:
            saved++;
            ;
        } while(--nbCols);
        dbm++; /* diagonal */
        assert(*dbm == dbm_LE_ZERO);
    } while(--nbLines);
    return base_SUBSET;

    /* check dbm < mingraph
     */        
    do
    {
        nbCols = dim;
        do {
            dbm++;
            if (*dbm < mingraph_raw16to32(*saved))
            {
                return base_DIFFERENT;
            }
        TrySuperSet16:
            saved++;
            ;
        } while(--nbCols);
        dbm++; /* diagonal */
        assert(*dbm == dbm_LE_ZERO);
    } while (--nbLines);
    return base_SUPERSET;

#endif /* NLONGER_CODE */
}


/* Relation between a DBM and a stored DBM in format
 * minimal graph with a bit matrix telling which constraints
 * are saved and the constraints on 32 bits.
 * @param dbm: DBM to test
 * @param dim: dimension
 * @param mingraph: stored DBM format
 * @param unused: relation is always approximate
 * @return approximate relation of dbm (<,==,!=) mingraph
 */
static
relation_t mingraph_relationWithMinBitMatrix32(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *needsExact, BOOL *isExact)
{
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const raw_t *constraints = mingraph_getCodedData(mingraph);
    uint32_t *bitMatrix = (uint32_t*) &constraints[nbConstraints];
    BOOL hasStrictLess = FALSE;
    const raw_t *dbmBase;
    
    assert(dim == mingraph_readDimFromPtr(mingraph));
    assert(dbm && dim > 2);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim))
           == nbConstraints);
    
    for(dbmBase = dbm;; dbmBase += 32, dbm = dbmBase)
    {
        uint32_t b;
        for(b = *bitMatrix++; b != 0; ++dbm,b >>= 1)
        {
            for(; (b & 1) == 0; ++dbm, b >>= 1);
            if (*dbm > *constraints)
            {
                *isExact = hasStrictLess;
                return base_DIFFERENT;
            }
            hasStrictLess |= *dbm < *constraints;
            if (!--nbConstraints)
            {
                *isExact = hasStrictLess;
                return base_SUBSET;
            }
            constraints++;
        }
    }
}


/* Relation between a DBM and a stored DBM in format
 * minimal graph with a bit matrix telling which constraints
 * are saved and the constraints on 16 bits.
 * @param dbm: DBM to test
 * @param dim: dimension
 * @param mingraph: stored DBM format
 * @param unused: relation is always approximate
 * @return approximate relation of dbm (<,==,!=) mingraph
 */
static
relation_t mingraph_relationWithMinBitMatrix16(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *needsExact, BOOL *isExact)
{
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    const int16_t *constraints =
        (int16_t*) mingraph_getCodedData(mingraph);
    const uint32_t *bitMatrix =
        mingraph_jumpConstInt16(constraints, nbConstraints);
    BOOL hasStrictLess = FALSE;
    const raw_t *dbmBase;

    assert(dim == mingraph_readDimFromPtr(mingraph));
    assert(dbm && dim > 2);
    assert(base_countBitsN(bitMatrix, bits2intsize(dim*dim))
           == nbConstraints);
    
    /* similar to save */
    for(dbmBase = dbm;; dbmBase += 32, dbm = dbmBase)
    {
        uint32_t b;
        for(b = *bitMatrix++; b != 0; ++dbm, b >>= 1)
        {
            raw_t c;
            for(; (b & 1) == 0; ++dbm, b >>= 1);
            c = mingraph_finite16to32(*constraints);
            if (*dbm > c)
            {
                *isExact = hasStrictLess;
                return base_DIFFERENT;
            }
            hasStrictLess |= *dbm < c;
            if (!--nbConstraints)
            {
                *isExact = hasStrictLess;
                return base_SUBSET;
            }
            constraints++;
        }
    }
}


/* Relation between a DBM and a stored DBM in format
 * minimal graph with a couples (i,j) telling which constraints
 * are saved and the constraints on 32 bits.
 * @param dbm: DBM to test
 * @param dim: dimension
 * @param mingraph: stored DBM format
 * @param unused: the relation is always approximate
 * @return approximate relation of dbm (<,==,!=) mingraph
 */
static
relation_t mingraph_relationWithMinCouplesij32(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *needsExact, BOOL *isExact)
{
    uint32_t info = mingraph_getInfo(mingraph);
    size_t nbConstraints = mingraph_getNbConstraints(mingraph);
    
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
        BOOL hasStrictLess = FALSE;
        
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
            
            /* test constraint
             */
            if (dbm[i*dim+j] > *constraints)
            {
                *isExact = hasStrictLess;
                return base_DIFFERENT;
            }
            hasStrictLess |= dbm[i*dim+j] < *constraints;
            if (!--nbConstraints)
            {
                *isExact = hasStrictLess;
                return base_SUBSET;
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
        /* everything is included in the trivial
         * zone as defined by dbm_init(..)
         */
        return base_SUBSET; /* base_EQUAL */
    }
}


/* Relation between a DBM and a stored DBM in format
 * minimal graph with a couples (i,j) telling which constraints
 * are saved and the constraints on 16 bits.
 * @param dbm: DBM to test
 * @param dim: dimension
 * @param mingraph: stored DBM format
 * @param unused: unused, here to get the same signature as
 * other functions.
 * @return approximate relation of dbm (<=,!=) mingraph
 */
static
relation_t mingraph_relationWithMinCouplesij16(const raw_t *dbm, cindex_t dim,
                                               const int32_t *mingraph,
                                               raw_t *needsExact, BOOL *isExact)
{
    uint32_t info = mingraph_getInfo(mingraph);

    /* reminder: size of indices of couples i,j = 4, 8, or 16 bits
     * the stored code is 0, 1, or 2, hence size = 2**(code + 2)
     * or 1 << (code + 2)
     */
    uint32_t bitSize = 1 << (mingraph_typeOfIJ(info) + 2);
    uint32_t bitMask = (1 << bitSize) - 1; /* standard */
    
    size_t nbConstraints     = mingraph_getNbConstraints(mingraph);
    const int16_t *constraints = (int16_t*) mingraph_getCodedData(mingraph);
    const uint32_t *couplesij =
        mingraph_jumpConstInt16(constraints, nbConstraints);
    
    uint32_t consumed = 0; /* count consumed bits */
    uint32_t val_ij = *couplesij;
    BOOL hasStrictLess = FALSE;
    
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
        
        /* test constraint
         */
        raw_t c = mingraph_finite16to32(*constraints);
        if (dbm[i*dim+j] > c)
        {
            *isExact = hasStrictLess;
            return base_DIFFERENT;
        }
        hasStrictLess |= dbm[i*dim+j] < c;
        if (!--nbConstraints)
        {
            *isExact = hasStrictLess;
            return base_SUBSET;
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


/** Fatal error: should not be called
 */
static
relation_t mingraph_relationError(const raw_t *dbm, cindex_t dim,
                                  const int32_t *mingraph,
                                  raw_t *unpackBuffer, BOOL *isExact)
{
    fprintf(stderr,
            RED(BOLD) PACKAGE_STRING
            " fatal error: invalid encoded DBM to compute relation with"
            NORMAL "\n");
    exit(2);

    return base_DIFFERENT; /* unreachable: compiler happy */
}
