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
 * Filename : debug.c (dbm)
 *
 * Debugging code for DBMs. Mostly alternative implementation to
 * dbm.c.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: gen.c,v 1.4 2005/06/21 14:07:41 adavid Exp $
 *
 *********************************************************************/

#include <stdlib.h>
#include "dbm/gen.h"
#include "dbm/print.h"
#include "dbm/dbm.h"
#include "base/bitstring.h"
#include "base/doubles.h"
#include "debug/macros.h"

/* For easy reading */
#define DBM(I,J) dbm[(I)*dim+(J)]
#define SRC(I,J) src[(I)*dim+(J)]
#define DST(I,J) dst[(I)*dim+(J)]
#define ARG(I,J) arg[(I)*dim+(J)]

/* For debugging */

#define ASSERT_DIAG_OK(DBM,DIM) ASSERT(dbm_isDiagonalOK(DBM,DIM), dbm_print(stderr, DBM, DIM))
#define ASSERT_NOT_EMPTY(DBM, DIM) ASSERT(dim == 0 || !dbm_isEmpty(DBM, DIM), dbm_print(stderr, DBM, DIM))


/* Algorithm:
 * - set diagonal to <= 0
 * - generate dbm[i,0]
 * - generate 1st try for dbm[i,j]
 * - disturb the bounds
 */
BOOL dbm_generate(raw_t *dbm, cindex_t dim, raw_t range)
{
    cindex_t i,j;
    size_t nbThin = 0;
    size_t nbTighten = 0;
    size_t threshold = dim*(dim-1) >> 1;

    /* trivial */

    if (dim < 1)
    {
        return FALSE;
    }
    else if (dim == 1)
    {
        *dbm = dbm_LE_ZERO;
        return FALSE;
    }
    assert(dbm);

    if (range < 20) range = 20;

    /* upper and lower bounds + diagonal */

    dbm_init(dbm, dim);
    for(i = 1; i < dim; ++i)
    {
        raw_t middle = RAND()%(range >> 1);
        DBM(0,i) = 1 - RAND()%(middle+1);
        DBM(i,0) = 1 + middle + RAND()%(range >> 1);
    }

    dbm_close(dbm, dim);
    assert(!dbm_isEmpty(dbm, dim));

    /* tighten diagonals a bit */

    for(i = 1; i < dim; ++i)
    {
        for(j = 0; j < i; ++j)
        {
            /* +1: to avoid division by 0
             * -1: to avoid to tighten too much (fix +1)
             */
            int32_t rangeij = DBM(i,j)+DBM(j,i);
            int32_t maxTighten = nbTighten > threshold ?
                5*(rangeij-1)/12 : 7*(rangeij-1)/12;
            int32_t dij = RAND()%(maxTighten+1);
            int32_t dji = RAND()%(maxTighten+1);

            switch(rand()%4)
            {
            case 0: /* don't touch diagonals */
                break;

            case 1: /* tighten dbm[i,j] */
                if (!dij) break;
                DBM(i,j) -= dij;
                dbm_closeij(dbm, dim, i, j);
                assert(!dbm_isEmpty(dbm, dim));
                nbTighten++;
                break;

            case 2: /* tighten dbm[j,i] */
                if (!dji) break;
                DBM(j,i) -= dji;
                dbm_closeij(dbm, dim, j, i);
                assert(!dbm_isEmpty(dbm, dim));
                nbTighten++;
                break;

            case 3: /* tighten dbm[i,j] and dbm[j,i] */
                if (!dij || !dji) break;
                if (rangeij-dij-dji < 2)
                {
                    /* almost empty */
                    if (nbThin < 2) /* not too many times */
                    {
                        DBM(i,j) = DBM(i,j)-dij;
                        DBM(j,i) = 2-DBM(i,j);
                        nbThin++;
                    }
                    else
                    {
                        if (rand() & 1)
                        {
                            DBM(i,j) -= dij;
                            dbm_closeij(dbm, dim, i, j);
                        }
                        else
                        {
                            DBM(j,i) -= dji;
                            dbm_closeij(dbm, dim, j, i);
                        }
                        nbTighten++;
                        break;
                    }
                }
                else
                {
                    DBM(i,j) -= dij;
                    DBM(j,i) -= dji;
                    if (i == 0 && DBM(i,j) > dbm_LE_ZERO) DBM(i,j) = dbm_LE_ZERO;
                    if (j == 0 && DBM(j,i) > dbm_LE_ZERO) DBM(j,i) = dbm_LE_ZERO;
                }
                nbTighten += 2;
                dbm_close1(dbm, dim, i);
                dbm_close1(dbm, dim, j);
                assert(!dbm_isEmpty(dbm, dim));
                break;
            }
        }
        /* don't constrain too much */
        if (nbTighten >= 2*threshold && (rand() & 1)) break;
    }

    if (rand()%30 == 0) /* sometimes */
    {
        /* desactivate a number of clocks */

        cindex_t n = rand()%(dim-1);
        DODEBUG(BOOL check = TRUE);
        assert(dim > 1);
        for (i = 0 ; i < n; ++i)
        {
            /* choose one clock, not the reference clock */

            cindex_t k = rand()%(dim-1)+1;
            DBM(k,0) = dbm_LS_INFINITY;
            DBM(0,k) = dbm_LE_ZERO;
            for (j = 1 ; j < dim ; ++j)
            {
                if (j != k)
                {
                    DBM(j,k) = dbm_LS_INFINITY; /* col to inf */
                    DBM(k,j) = dbm_LS_INFINITY; /* row to inf */
                }
            }
        }
        if (n) DODEBUG(check =) dbm_close(dbm, dim);
        assert(check);
    }
    else if (rand()%30 == 0) dbm_up(dbm, dim);

    return (BOOL) !dbm_isEqualToInit(dbm, dim);
}


/* Algorithm:
 * - start from unconstrained
 * - constrain with constraints[n]
 * - for all unconstrained dbm[i,j] s.t. i!=j
 *   may constrain it at most by (dbm[i,j]+dbm[j,i])/2
 */
BOOL dbm_generateConstrained(raw_t *dbm, cindex_t dim, raw_t range,
                             const constraint_t *constraints, size_t n)
{
    if (dim < 1) return FALSE;

    dbm_init(dbm, dim);

    if (dim > 1)
    {
        uint32_t bitMatrix[bits2intsize(dim*dim)];
        size_t k;
        assert(dbm && dim && (n == 0 || constraints));
        
        /* should hold by pre-condition */
        if (!dbm_constrainN(dbm, dim, constraints, n)) return FALSE;
        
        /* mark known constraints */
        base_resetSmall(bitMatrix, bits2intsize(dim*dim));
        for(k = 0; k < n; ++k)
        {
            base_setOneBit(bitMatrix, constraints[k].i*dim+constraints[k].j);
        }
        
        dbm_generatePreConstrained(dbm, dim, range, bitMatrix);
    }

    return TRUE;
}


/** Random constrain (internal).
 * @param dbm,dim: DBM of dimension dim
 * @param i,j: dbm[i,j] to constrain
 * @param range: max range to tighten
 */
static
void dbm_randomConstrain(raw_t *dbm, cindex_t dim, cindex_t i, cindex_t j,
                         raw_t range)
{
    assert(range > 2);
    if (DBM(i,j) == dbm_LS_INFINITY)
    {
        DBM(i,j) = RAND()%(range-2) + 2;
        if (DBM(j,i) != dbm_LS_INFINITY)
        {
            DBM(i,j) -= DBM(j,i);
        }
    }
    else
    {
        DBM(i,j) -= RAND()%(range-2);
    }
    dbm_closeij(dbm, dim, i, j);
}


/* Go through dbm and for the unmarked constraints
 * that are also not on the diagonal, constrain dbm[i,j].
 */
void dbm_generatePreConstrained(raw_t *dbm, cindex_t dim, raw_t range,
                                const uint32_t *bits)
{
    if (dim > 1)
    {
        cindex_t i, j;
        uint32_t b=0, mask, x;
        
        /* constrain/generate missing constraints */
        mask = 0;
        x = 2;
        for(i = 0; i < dim; ++i)
        {
            for(j = 0; j < dim; ++j)
            {
                if (!mask)
                {
                    mask = 1;
                    b = *bits++;
                }
                /* if not constrained and want to constrain */
                if (!(b & mask) && i != j && rand()%(x >> 1) == 0)
                {
                    raw_t rangeij;
                    rangeij = dbm_addRawRaw(dbm[i*dim+j], dbm[j*dim+i]);
                    if (rangeij > 2) /* minimum */
                    {
                        if (rangeij > range) rangeij = range;
                        dbm_randomConstrain(dbm, dim, i, j, rangeij);
                        x++; /* this will limit the number of tightenings */
                    }
                }
                mask <<= 1;
            }
        }
    }
}


/* Cases to cover:
 * - no intersection: one of dim*(dim-1)
 * - intersec partial: cut in 2, cut semi, in corner, remove part (up,down,left,right)
 * - included
 */
void dbm_generateArgDBM(raw_t *arg, const raw_t *dbm, cindex_t dim)
{
    uint32_t bitMatrix[bits2intsize(dim*dim)];
    cindex_t i, j;
    size_t max, n=1;
    uint32_t x = 0;
    raw_t maxRange = dbm_getMaxRange(dbm, dim);
    assert(dbm && dim && arg);

    if (dim <= 1) /* case with only ref clock */
    {
        if (dim == 1) *arg = dbm_LE_ZERO;
        return;
    }

    if (maxRange < 10) maxRange = 10;

    /* swith on 8 to manage probability */
    switch(rand()%8)
    {
    case 0: /* no intersection at all */
    {
        constraint_t cij;

        /* pick up one constraint to negate, not on the diagonal */
        do {
            i = rand()%dim;
            j = rand()%dim;
        } while(i == j);

        cij.i = i;
        cij.j = j;
        cij.value = DBM(j,i) == dbm_LS_INFINITY ?
            RAND()%maxRange + 1 : /* can't get no intersection */
            dbm_negRaw(DBM(j,i));

        if (!dbm_generateConstrained(arg, dim, maxRange, &cij, 1))
        {
            /* It is possible to get it empty:
             * case i = 1, j = 0, negate constraint(0,1)== (<=0)
             */
            dbm_generate(arg, dim, maxRange);
        }
        return;
    }

    case 1: /* completely included */
        dbm_copy(arg, dbm, dim);
        x = 4;
    case 2: /* semi included/includes */
        if (!x)
        {
            dbm_generateSuperset(arg, dbm, dim);
            x = 4;
        }

        for(i = 0; i < dim; ++i)
        {
            for(j = 0; j < dim; ++j)
            {
                if (i != j && rand()%(x >> 2) == 0)
                {
                    raw_t rangeij = dbm_addRawRaw(ARG(i,j), ARG(j,i));
                    if (rangeij > 2) /* minimum */
                    {
                        if (rangeij > maxRange) rangeij = maxRange;
                        dbm_randomConstrain(arg, dim, i, j, rangeij);
                        x++; /* try to limit the number of tightenings */
                        /* we have to closeij everytime, otherwise rangeij
                         * becomes wrong */
                    }
                }
            }
        }
        return;

    case 3: /* try with 1 constraint */
        n = 1;
        break;
    case 4: /* try with 2 constraints */
        n = 2;
        break;
    case 5: /* ... */
        n = 3;
        break;
    case 6:
        n = dim*dim; /* as the choice of (i,j) is random, this is ok */
        break;
    case 7:
        n = rand()%(dim*(dim-1))+1;
        break;
    }

    base_resetSmall(bitMatrix, bits2intsize(dim*dim));
    dbm_init(arg, dim);
    max = dim*dim;
    maxRange = dbm_getMaxRange(dbm, dim);
    if (maxRange < 10) maxRange = 10;
    
    while(n) /* tighten */
    {
        /* choose i,j to keep */
        do {
            i = rand()%dim;
            j = rand()%dim;
        } while(i == j);
        base_setOneBit(bitMatrix, i*dim+j);

        if (DBM(i,j) < ARG(i,j)) /* if tighten */
        {
            raw_t rangeij = dbm_addRawRaw(DBM(i,j), ARG(j,i));
            if (rangeij > 2) /* can tighten */
            {
                ARG(i,j) = DBM(i,j);
                if (rangeij > maxRange) rangeij = maxRange;
                dbm_randomConstrain(arg, dim, i, j, rangeij);
            }
        }
        n--;
    }

    dbm_generatePreConstrained(arg, dim, maxRange, bitMatrix);
}


/* Algorithm:
 * - copy src to dbm and increment the bounds.
 * - restore diagonal
 * - close
 */
void dbm_generateSuperset(raw_t *dst, const raw_t *src, cindex_t dim)
{
    size_t n;
    raw_t *dbm = dst;
    assert(dim == 0 || (src && dst));

    if (dim == 1)
    {
        *dst = dbm_LE_ZERO;
        return;
    }

    /* inc bounds of src -> dst */

    for(n = dim*dim; n != 0; ++src, ++dst, --n)
    {
        *dst =
            *src == dbm_LS_INFINITY ?
            dbm_LS_INFINITY :
            dbm_addRawRaw(*src, rand()%20 + 1);
    }

    /* restore diagonal */
    for(dst = dbm, n = dim; n != 0; dst += dim+1, --n)
    {
        *dst = dbm_LE_ZERO;
    }

    /* check 1st row */
    for(dst = dbm, n = dim; n != 0; ++dst, --n)
    {
        if (*dst > dbm_LE_ZERO) *dst = dbm_LE_ZERO;
    }

    dbm_close(dbm, dim);

    ASSERT_NOT_EMPTY(dbm, dim);
}


/* Algorithm:
 * - copy
 * - try to tighten
 */
BOOL dbm_generateSubset(raw_t *dst, const raw_t *src, cindex_t dim)
{
    cindex_t i,j;
    BOOL subset = FALSE;

    if (dim <= 1)
    {
        if (dim == 1) *dst = dbm_LE_ZERO;
        return FALSE;
    }

    assert(dst && src);

    if (src != dst)
    {
        dbm_copy(dst, src, dim);
    }

    for (i = 0; i < dim; ++i)
    {
        for (j = 0; j < i; ++j)
        {
            if (DST(j,i) == dbm_LS_INFINITY)
            {
                if (DST(i,j) == dbm_LS_INFINITY)
                {
                    DST(i,j) = 1 + rand()%1000;
                    dbm_closeij(dst, dim, i, j);
                    subset = TRUE;
                }
                else if (i != 0 || DST(i,j) > dbm_LE_ZERO)
                {
                    DST(i,j) -= rand()%10 + 1;
                    if (i == 0 && DST(i,j) < dbm_LE_ZERO)
                    {
                        DST(i,j) = dbm_LE_ZERO;
                    }
                    dbm_closeij(dst, dim, i, j);
                    subset = TRUE;
                }
            }
            else
            {
                if (DST(i,j) == dbm_LS_INFINITY)
                {
                    DST(i,j) = 2+rand()%1000 - DST(j,i);
                    dbm_closeij(dst, dim, i, j);
                    subset = TRUE;
                }
                else
                {
                    raw_t rawRange = DST(i,j) + DST(j,i);
                    if (rawRange > 2)
                    {
                        DST(i,j) = DST(i,j) - 1 - rand()%(rawRange-2);
                        dbm_closeij(dst, dim, i, j);
                        subset = TRUE;
                    }
                }
            }
            if (subset && rand()%dim == 0) return subset;
        }
    }

    return subset;
}


/* Algorithm: get a real point,
 * discretize it and check if it is still
 * included.
 */
BOOL dbm_generatePoint(int32_t *pt, const raw_t *dbm, cindex_t dim)
{
    double dpt[dim];
    cindex_t i;
    if (dim < 1)
    {
        return FALSE;
    }
    else if (dim == 1)
    {
        pt[0] = 0;
        return TRUE;
    }
    if (dbm_generateRealPoint(dpt, dbm, dim))
    {
        for (i = 0; i < dim; ++i) pt[i] = (int32_t) dpt[i];
        return dbm_isPointIncluded(pt, dbm, dim);
    }
    return FALSE;

    /*
    uint32_t i;
    assert(pt && dbm && dim);
    pt[0] = 0;
    for (i = 1; i < dim; ++i)
    {
        int32_t min = -dbm_raw2bound(DBM(0,i));
        int32_t max = dbm_raw2bound(DBM(i,0));
        if (max == dbm_INFINITY) max = dbm_INFINITY >> 8;
        pt[i] = (min + max) >> 1;
    }
    return dbm_isPointIncluded(pt, dbm, dim);
    */
}


/* Algorithm:
 * - compute upper and lower bounded such that
 *   pt[i]-pt[j] and pt[j]-pt[i] satisfy the
 *   constraints dbm[i,j] and dbm[j,i] for j < i
 * - choose a random pt[i] between upper and lower bounds
 */
BOOL dbm_generateRealPoint(double *pt, const raw_t *dbm, cindex_t dim)
{
    cindex_t i,j;
    uint32_t retries = 0;

    /* A DBM of dim 0 is semantically empty
     * and does not contain any point.
     */
    if (dim < 1)
    {
        return FALSE;
    }

    assert(pt && dbm);
    pt[0] = 0.0;

    if (dim == 1)
    {
        return TRUE;
    }

    for (i = 1; i < dim;)
    {
        double lower = -1e99;
        double upper = 1e99;
        BOOL ok;
        
        /* tighten lower and upper */
        
        for (j = 0; j < i; ++j)
        {
            double loweri, upperi;
            double dbmji = DBM(j,i) < dbm_LS_INFINITY ?
                dbm_raw2bound(DBM(j,i)) :
                (dbm_LS_INFINITY >> 8);
            double dbmij = DBM(i,j) < dbm_LS_INFINITY ?
                dbm_raw2bound(DBM(i,j)) :
                (dbm_LS_INFINITY >> 8);
            
            if (dbm_rawIsStrict(DBM(j,i))) dbmji -= 0.01;
            if (dbm_rawIsStrict(DBM(i,j))) dbmij -= 0.01;
            
            loweri = pt[j]-dbmji;
            upperi = dbmij+pt[j];
            
            if (lower < loweri) lower = loweri;
            if (upper > upperi) upper = upperi;
        }

        /* if retrying twice then try the middle */
#ifdef __MINGW32__
        pt[i] = lower + (retries > 1 ? 0.5 : (rand()%10000)/10000.0)*(upper-lower);
#else
        pt[i] = lower + (retries > 1 ? 0.5 : drand48())*(upper-lower);
#endif
        
        /* if too much close to a corner then
         * we may have pt outside dbm */
        
        ok = TRUE;
        for (j = 0; j < i; ++j) if (i != j)
        {
            double diff = pt[i] - pt[j] - (double) dbm_raw2bound(DBM(i,j));
            if (dbm_rawIsStrict(DBM(i,j)))
            {
                if (IS_GE(diff, 0.0)) { ok = FALSE; break; }
            }
            else
            {
                if (IS_GT(diff, 0.0)) { ok = FALSE; break; }
            }
            diff = pt[j] - pt[i] - (double) dbm_raw2bound(DBM(j,i));
            if (dbm_rawIsStrict(DBM(j,i)))
            {
                if (IS_GE(diff, 0.0)) { ok = FALSE; break; }
            }
            else
            {
                if (IS_GT(diff, 0.0)) { ok = FALSE; break; }
            }
        }
        
        if (ok)
        {
            i++;
        }
        else /* retry */
        {
            i = 1;
            if (++retries > 2) return FALSE;
        }
    }

    /* temporary fix */
    return dbm_isRealPointIncluded(pt, dbm, dim);

    /*
    assert(dbm_isRealPointIncluded(pt, dbm, dim));
    return TRUE;
    */
}
