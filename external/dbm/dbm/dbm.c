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
 * Filename : dbm.c (dbm)
 *
 * DBM library: basic functions.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2000, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * Not reviewed.
 * $Id: dbm.c,v 1.58 2005/11/22 12:58:54 adavid Exp $
 *
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "base/intutils.h"
#include "base/bitstring.h"
#include "base/doubles.h"
#include "debug/macros.h"
#include "dbm/dbm.h"
#include "dbm/print.h"
#include "dbm.h"

/* More readable code with this */

#define DBM(I,J) dbm[(I)*dim+(J)]
#define SRC(I,J) src[(I)*dim+(J)]
#define DST(I,J) dst[(I)*dim+(J)]
#define DBM1(I,J) dbm1[(I)*dim+(J)]
#define DBM2(I,J) dbm2[(I)*dim+(J)]

/** For debugging */

#define ASSERT_DIAG_OK(DBM,DIM) ASSERT(dbm_isDiagonalOK(DBM,DIM), dbm_print(stderr, DBM, DIM))
#define ASSERT_NOT_EMPTY(DBM, DIM) ASSERT(!dbm_isEmpty(DBM, DIM), dbm_print(stderr, DBM, DIM))


#ifndef NCLOSELU

/* Specialized close for extrapolations:
 * skip looping with lower[k] == upper[k] == -infinity
 * (clock k is already tightened and these constraints
 * are already fixed)
 * + assertion that the DBM is not empty.
 * See dbm_close.
 */
void dbm_closeLU(raw_t *dbm, cindex_t dim, const int32_t *lower, const int32_t *upper)
{
    raw_t *dbm_kdim = dbm; /* &dbm[k*dim] */
    cindex_t k = 0;
    assert(dim && dbm && lower && upper);
    ASSERT_DIAG_OK(dbm, dim);

    do { /* loop on k */
        if (lower[k] != -dbm_INFINITY || upper[k] != -dbm_INFINITY)
        {
            raw_t *dbm_idim = dbm; /* &dbm[i*dim]  */
            cindex_t i = 0;
            assert(k < dim && dbm_kdim == &dbm[k*dim]);
            do { /* loop on i */
                if (i != k)
                {
                    raw_t dbm_ik = dbm_idim[k]; /* dbm[i,k] == dbm[i*dim+k] */
                    assert(i < dim && dbm_idim == &dbm[i*dim]);
                
                    if (dbm_ik != dbm_LS_INFINITY)
                    {
                        cindex_t j = 0;
                        do { /* loop on j */
                            /* could try if j != k but it isn't worth */
                            raw_t dbm_kj = dbm_kdim[j]; /* dbm[k,j] == dbm[k*dim+j] */
                            if (dbm_kj != dbm_LS_INFINITY)
                            {
                                raw_t dbm_ikkj = dbm_addFiniteFinite(dbm_ik, dbm_kj);
                                if (dbm_idim[j] > dbm_ikkj) /* dbm[i,j] > dbm[i,k]+dbm[k,j] */
                                {
                                    dbm_idim[j] = dbm_ikkj;
                                }
                            }
                        } while(++j < dim);
                    }
                    assert(dbm_idim[i] >= dbm_LE_ZERO);
                }
                assert(dbm_idim[i] == dbm_LE_ZERO);
                dbm_idim += dim;
            } while(++i < dim);
        }
        dbm_kdim += dim;
    } while(++k < dim);
    ASSERT_NOT_EMPTY(dbm, dim);
}

#endif

/* Algorithm:
 * Write
 * - <=0 in the 1st row
 * - <infinity in the matrix
 * - <=0 in the diagonal.
 */
void dbm_init(raw_t *dbm, cindex_t dim)
{
    cindex_t n;
    assert(dim && dbm);
    
    base_fill(dbm, dim, dbm_LE_ZERO);
    base_fill(dbm+dim, dim*(dim-1), dbm_LS_INFINITY);

    for(n = dim - 1; n != 0; --n)
    {
        dbm += dim + 1;
        *dbm = dbm_LE_ZERO;
    }
}

/* Test if this DBM contains 0: Since the DBM
 * is closed, testing the lower bounds x0-xi is
 * is enough to see if 0 is there or not.
 */
BOOL dbm_hasZero(const raw_t *dbm, cindex_t dim)
{
    cindex_t i;
    for(i = 1; i < dim; ++i)
    {
        if (dbm[i] < dbm_LE_ZERO)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* Equality test with trivial dbm as obtained from
 * dbm_init(dbm, dim):
 * - test 1st row
 * - test 1st element of 2nd row
 * - test element of diagonal followed by dim*infinity
 * - test last element of diagonal
 * Semi-optimistic algorithm.
 */
BOOL dbm_isEqualToInit(const raw_t *dbm, cindex_t dim)
{
    assert(dbm && dim);
    
    /* 1st row */

    if (base_diff(dbm, dim, dbm_LE_ZERO))
    {
        return FALSE;
    }
    if (dim == 1) return TRUE;

    /* 1st element 2nd row */

    dbm += dim;
    if (*dbm != dbm_LS_INFINITY) return FALSE;

    /* remaining */

    if (dim > 2)
    {
        /* dim = # of rows
         * dim-1 = # of rows starting from 
         * diagonal, ending to next diagonal element
         * dim-2 because we've read the 1st row
         */
        size_t nbLines = dim - 2;
        do {
            if ((dbm[1] ^ dbm_LE_ZERO) |
                base_diff(dbm+2, dim, dbm_LS_INFINITY))
            {
                return FALSE;
            }
            dbm += dim + 1;
        } while(--nbLines);
    }
    
    /* last element of diagonal */

    return (BOOL)(*++dbm == dbm_LE_ZERO);
}


/* Algorithm:
 * Go though all constraints and keep the max of dst and src.
 */
void dbm_convexUnion(raw_t *dst, const raw_t *src, cindex_t dim)
{
    assert(dst && src && dim);
    ASSERT_DIAG_OK(dst, dim);
    ASSERT_DIAG_OK(src, dim);

    if (dim > 1) /* if more than reference clock */
    {   
        size_t n = dim*dim - 2; /* -2: start + end of diagonal */
        DODEBUGX(raw_t *dbmPtr = dst);

        do { if (*++dst < *++src) *dst = *src; } while(--n);

        assertx(dbm_isValid(dbmPtr, dim));
    }
}


/* Algorithm:
 * for 0 <= i < dim, 0 <= j < dim:
 *   if dst[i,j] > src[i,j] then
 *     dst[i,j] = src[i,j]
 *     if dst[i,j] + dst[j,i] < 0 then
 *       dst[i,i] = dst[j,j] = dst[i,j] + dst[j,i]
 *       return FALSE
 *     endif
 *   endif
 * return close
 */
BOOL dbm_intersection(raw_t *dst, const raw_t *src, cindex_t dim)
{
    assert(dim && dst && src);
    ASSERT_DIAG_OK(dst, dim);
    ASSERT_DIAG_OK(src, dim);
    assertx(dbm_isValid(dst, dim));
    assertx(dbm_isValid(src, dim));

    if (dim > 1)
    {
        cindex_t i,j, ci = 0, cj = 0, count = 0;
        uint32_t touched[bits2intsize(dim)]; /* to reduce final close */
        DODEBUG(BOOL haveInter = dbm_haveIntersection(dst, src, dim));
        base_resetBits(touched, bits2intsize(dim));

        i = 0;
        do {
            j = 0;
            do {
                /* See constrain */
                if (DST(i,j) > SRC(i,j))
                {
                    DST(i,j) = SRC(i,j);
                    if (dbm_negRaw(SRC(i,j)) >= DST(j,i))
                    {
                        assert(!haveInter);
                        DST(0,0) = -1; /* consistent with isEmpty */
                        return FALSE;
                    }
                    count++;
                    ci = i;
                    cj = j;
                    base_setOneBit(touched, i);
                    base_setOneBit(touched, j);
                }
            } while(++j < dim);
        } while(++i < dim);
        assert(haveInter);

        /* choose best close */
        if (count > 1)
        {
            return dbm_closex(dst, dim, touched);
        }
        else if (count == 1)
        {
            dbm_closeij(dst, dim, ci, cj);
        }
        ASSERT_NOT_EMPTY(dst, dim);
    }

    return TRUE;
}


/* Similar to intersection but with weak constraints of dbm1 and dbm2
 */
BOOL dbm_relaxedIntersection(raw_t *dst, const raw_t *dbm1, const raw_t *dbm2, cindex_t dim)
{
    cindex_t i, j, ci = 0, cj = 0, count = 0;
    uint32_t touched[bits2intsize(dim)]; /* to reduce final close */
    base_resetBits(touched, bits2intsize(dim));
    
    assert(dim && dst && dbm1 && dbm2);
    ASSERT_DIAG_OK(dbm1, dim);
    ASSERT_DIAG_OK(dbm2, dim);
    assertx(dbm_isValid(dbm1, dim));
    assertx(dbm_isValid(dbm2, dim));

    for(i = 0; i < dim; ++i)
    {
        for(j = 0; j < dim; ++j)
        {
            DST(i,j) = DBM1(i,j) == dbm_LS_INFINITY
                ? dbm_LS_INFINITY
                : (DBM1(i,j)|dbm_WEAK);

            /* The test works for infinity too */
            raw_t dij = DBM2(i,j) | dbm_WEAK;
            if (DST(i,j) > dij)
            {
                assert(dij != dbm_LS_INFINITY);
                DST(i,j) = dij;
                /* Again ok for the test */
                if (dbm_negRaw(dij) >= (DBM1(j,i)|dbm_WEAK))
                {
                    DST(0,0) = -1; /* consistent with isEmpty */
                    return FALSE;
                }
                count++;
                ci = i;
                cj = j;
                base_setOneBit(touched, i);
                base_setOneBit(touched, j);
            }
        }
    }
    
    /* choose best close */
    if (count > 1)
    {
        return dbm_closex(dst, dim, touched);
    }
    else if (count == 1)
    {
        dbm_closeij(dst, dim, ci, cj);
    }
    ASSERT_NOT_EMPTY(dst, dim);
    
    return TRUE;
}


/* Go through the DBMs and compare lower bounds against
 * upper bounds. As the zones described by the DBMs are
 * convex, we can deduce if there is an intersection
 * without computing it.
 * The loop iterates on half the matrix. Interpretation
 * of low and high may be reversed: we iterate on the
 * lower left part of the matrix for convenience but
 * we could it on the upper right part.
 */
BOOL dbm_haveIntersection(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim)
{
    cindex_t i,j;

    assert(dbm1 && dbm2 && dim);
    assertx(dbm_isValid(dbm1, dim));
    assertx(dbm_isValid(dbm2, dim));

    for(i = 1; i < dim; ++i)
    {
        j = 0;
        do {
            if (DBM1(i,j) != dbm_LS_INFINITY &&
                dbm_negRaw(DBM1(i,j)) >= DBM2(j,i))
            {
                return FALSE;
            }
            if (DBM2(i,j) != dbm_LS_INFINITY &&
                dbm_negRaw(DBM2(i,j)) >= DBM1(j,i))
            {
                return FALSE;
            }
        } while(++j < i);
    }

    return TRUE;
}


/* if cij > constraint then
 *  tighten cij
 *  check cij + cji >= 0
 *  touch clocks i and j
 * endif
 */
BOOL dbm_constrain(raw_t *dbm, cindex_t dim,
                   cindex_t i, cindex_t j, raw_t constraint,
                   uint32_t *touched)
{
    assert(i < dim && j < dim && i != j);
    assert(dbm && touched);

    /* tighten the constraint
     */
    if (DBM(i,j) > constraint)
    {
        /* alter the dbm for later checks to work
         */
        DBM(i,j) = constraint;

        /* if dbm[i,j] + dbm[j,i] < 0 then stop
         * don't set bits: no need to close, DBM is empty
         * same as neg(constraint) >= dbm[j,i] (raw)
         *
         * if (dbm_addRawFinite(DBM(j,i), constraint) < dbm_LE_ZERO)
         */
        if (dbm_negRaw(constraint) >= DBM(j,i))
        {
            /* DBM is not closed, no need to mark it non-empty */
            return FALSE;
        }

        /* touch clocks for later closure
         */
        base_setOneBit(touched, i);
        base_setOneBit(touched, j);
    }

    return TRUE;
}

/* if dbm[i,j] > constraint then
 *  tighten dbm[i,j]
 *  check dbm[i,j] + dbm[j,i] >= 0
 *  close for clocks i and j
 * endif
 */
BOOL dbm_constrain1(raw_t *dbm, cindex_t dim,
                    cindex_t i, cindex_t j, raw_t constraint)
{
    assert(i < dim && j < dim && i != j);
    assert(dbm);

     /* tighten the constraint
      */
    if (DBM(i,j) > constraint)
    {
        /* alter the dbm for later checks to work
         */
        DBM(i,j) = constraint;

        /* if dbm[i,j] + dbm[j,i] < 0 then stop
         * if (dbm_addRawFinite(DBM(j,i), constraint) < dbm_LE_ZERO)
         */
        if (dbm_negRaw(constraint) >= DBM(j,i))
        {
            DBM(0,0) = -1; /* consistent with isEmpty */
            return FALSE;
        }

        /* close for i and j */

        dbm_closeij(dbm, dim, i, j);
        
        ASSERT_NOT_EMPTY(dbm, dim);
    }

    return TRUE;
}


/* Inlined version of constrain with detection of change */
#define DBM_CONSTRAIN(I,J,V)           \
cindex_t i = I;                         \
cindex_t j = J;                         \
raw_t   v = V;                         \
assert(i < dim && j < dim && i != j);  \
if (DBM(i,j) > v)                      \
{                                      \
    DBM(i,j) = v;                      \
    if (dbm_negRaw(v) >= DBM(j,i))     \
    {                                  \
        DBM(0,0) = -1; /* mark empty */\
        return FALSE;                  \
    }                                  \
    ++changed;                         \
    base_setOneBit(touched, ci = i);   \
    base_setOneBit(touched, cj = j);   \
}


/* for all constraints do
 *  constrain constraint_i
 * done
 * if still non empty
 *  close dbm
 * endif
 * NOTE: optimistic code.
 */
BOOL dbm_constrainN(raw_t *dbm, cindex_t dim,
                    const constraint_t *constraints, size_t n)
{
    assert(dbm);
    /* n!=0 implies (constraints OK and can't constraint ref clock) */
    assert(!n || (constraints && dim > 1));
    assert(!dbm_isEmpty(dbm, dim));

    if (n != 0) /* normally it is the case */
    {
        uint32_t changed = 0, ci = 0, cj = 0;
        uint32_t touched[bits2intsize(dim)];
        base_resetBits(touched, bits2intsize(dim));

        do {
            DBM_CONSTRAIN(constraints->i, constraints->j, constraints->value);
            ++constraints;
        } while(--n);
        
        if (changed == 1)
        {
            dbm_closeij(dbm, dim, ci, cj);
        }
        else if (changed > 1)
        {
            return dbm_closex(dbm, dim, touched);
        }
    }

    return TRUE;
}


/* Same as dbm_constrainN but with a table for index translation.
 */
BOOL dbm_constrainIndexedN(raw_t *dbm, cindex_t dim, const cindex_t *indexTable,
                           const constraint_t *constraints, size_t n)
{
    assert(dbm && indexTable);
    /* n!=0 implies (constraints OK and can't constraint ref clock) */
    assert(!n || (constraints && dim > 1));
    assert(!dbm_isEmpty(dbm, dim));

    if (n) /* normally it is the case */
    {
        uint32_t changed = 0, ci = 0, cj = 0;
        uint32_t touched[bits2intsize(dim)];
        base_resetBits(touched, bits2intsize(dim));

        do {
            DBM_CONSTRAIN(indexTable[constraints->i],
                          indexTable[constraints->j],
                          constraints->value);
            ++constraints;
        } while(--n);

        if (changed == 1)
        {
            dbm_closeij(dbm, dim, ci, cj);
        }
        else if (changed > 1)
        {
            return dbm_closex(dbm, dim, touched);
        }
    }

    return TRUE;
}


/* Reset 1st column of DBM to infinity, which is
 * xi-x0 <= infinity: no upper bound.
 * The DBM stays closed.
 */
void dbm_up(raw_t *dbm, cindex_t dim)
{
    cindex_t i;
    assert(dbm && dim);

    for (i = 1; i < dim; ++i) DBM(i,0) = dbm_LS_INFINITY;

    assertx(dbm_isValid(dbm, dim));
}


/* Apply up except for stopped clocks and free
 * upper-bound diagonals of the running clocks
 * w.r.t the stopped clocks.
 */
void dbm_up_stop(raw_t *dbm, cindex_t dim, const uint32_t *stopped)
{
    cindex_t i,j;
    assert(dbm && dim && stopped);

    for(i = 1; i < dim; ++i)
    {
        if (base_readOneBit(stopped, i))
        {
            // Clock is stopped.
            for(j = 1; j < dim; ++j)
            {
                if (i != j && !base_readOneBit(stopped,j))
                {
                    DBM(j,i) = dbm_LS_INFINITY;
                }
            }
        }
        else
        {
            // Clock is running.
            DBM(i,0) = dbm_LS_INFINITY;
        }
    }

    assertx(dbm_isValid(dbm, dim));
}

/* For all clock constraints x0-xj: compute the new value.
 * Ideally we would like to set it to zero, i.e. removing the
 * lower bound on j. However, relations to other constraints may
 * force a non-zero lower bound.
 * Algorithm implemented:
 *
 * for (j = 1; j < dim; ++j) {
 *   if dbm[0,j] < LE_ZERO)  {
 *     dbm[0,j] = LE_ZERO;
 *     for (i = 1; i < dim; ++i) {
 *       if dbm[0,j] > dbm[i,j]
 *         dbm[0,j] = dbm[i,j];
 *     }
 * }
 * Notation: dbm_ij == dbm[i,j]
 */
void dbm_downFrom(raw_t *dbm, cindex_t dim, cindex_t j)
{
    cindex_t i;
    assert(dim && dbm);

    for (; j < dim; ++j)
    {
        if (DBM(0,j) < dbm_LE_ZERO)
        {
            DBM(0,j) = dbm_LE_ZERO;

            for (i = 1; i < dim; ++i)
            {
                if (DBM(0,j) > DBM(i,j))
                {
                    DBM(0,j) = DBM(i,j);
                }
            } 
        }
    }

    assertx(dbm_isValid(dbm, dim));
}


void dbm_down_stop(raw_t *dbm, cindex_t dim, const uint32_t *stopped)
{
    cindex_t i,j;

#if 1
    /* Basic version: Test the bits when needed. */
    assert(dbm && dim);

    for(j = 1; j < dim; ++j)
    {
        /* Loosen DBM[0,j] to 0 if j is running. */
        if (DBM(0,j) < dbm_LE_ZERO &&
            !base_readOneBit(stopped, j))
        {
            DBM(0,j) = dbm_LE_ZERO;
            
            /* Tighten DBM[0,j] with running clocks. */
            for(i = 1; i < dim; ++i)
            {
                if (!base_readOneBit(stopped, i)
                    && DBM(0,j) > DBM(i,j))
                {
                    DBM(0,j) = DBM(i,j);
                }
            }
            /* Recompute diagonals with stopped clocks. */
            for(i = 1; i < dim; ++i)
            {
                if (base_readOneBit(stopped, i))
                {
                    DBM(i,j) = dbm_addRawFinite(DBM(i,0), DBM(0,j));
                }
            }
        }
    }
#else
    /* Attempt to optimize: Filter 1st and then run the algorithm.
     * Unfortunately not branch-prediction friendly.
     */

    /* First filter clocks. */
    cindex_t run[dim-1], stop[dim-1], loose[dim-1];
    size_t nrun = 0, nstop = 0, nloose = 0;
    assert(dbm && dim);

    for(i = 1; i < dim; ++i)
    {
        if (base_readOneBit(stopped, i))
        {
            stop[nstop++] = i;
        }
        else
        {
            run[nrun++] = i;

            if (DBM(0,i) < dbm_LE_ZERO)
            {
                loose[nloose++] = i;
            }
        }
    }

    /* Run same algorithm as before for running clocks
     * and recompute diagonals with stopped clocks.
     */
    for(j = 0; j < nloose; ++j)
    {
        cindex_t rj = loose[j];
        DBM(0,rj) = dbm_LE_ZERO;

        for(i = 0; i < nrun; ++i)
        {
            cindex_t ri = run[i];
            
            if (DBM(0,rj) > DBM(ri,rj))
            {
                DBM(0,rj) = DBM(ri,rj);
            }
        }
        for(i = 0; i < nstop; ++i)
        {
            cindex_t ri = stop[i];
            DBM(ri,rj) = dbm_addRawFinite(DBM(ri,0), DBM(0,rj));
        }
    }
#endif

    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * dbm[k,0] = <= value
 * dbm[0,k] = <= -value
 * for 1 <= i < dim:
 *   if i != k then
 *     dbm[k,i] = dbm[k,0] + dbm[0,i]
 *     dbm[i,k] = dbm[i,0] + dbm[0,k]
 *   endif
 * We can skip the test i != k since for
 * i == k, we end up writing <=0 twice.
 */
void dbm_updateValue(raw_t *dbm, cindex_t dim, cindex_t k, int32_t value)
{
    cindex_t i;
    assert(dbm && k > 0 && k < dim);
    assert(value >= 0 && value < dbm_INFINITY);

    DBM(k,0) = dbm_bound2raw(value, dbm_WEAK);
    DBM(0,k) = dbm_bound2raw(-value, dbm_WEAK);
    
    for (i = 1; i < dim; ++i)
    {
        DBM(k,i) = dbm_addFiniteRaw(DBM(k,0), DBM(0,i));
        DBM(i,k) = dbm_addRawFinite(DBM(i,0), DBM(0,k));
    }

    assert(DBM(k,k) == dbm_LE_ZERO);
    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * for all i != k:
 *   dbm[k,i] = infinity
 *   dbm[i,k] = bound_i0
 *
 * xk-xi <= inf -> no upper bound
 * xi-xk <= xi-x0 <=> xk >= x0 -> no lower bound
 * We recompute the closure on-the-fly.
 */
void dbm_freeClock(raw_t *dbm, cindex_t dim, cindex_t k)
{
    cindex_t i;
    assert(dbm && k > 0 && k < dim);
    
    for (i = 0; i < dim; ++i)
    {
        if (i != k)
        {
            DBM(k,i) = dbm_LS_INFINITY;
            DBM(i,k) = DBM(i,0);
        }
    }

    assertx(dbm_isValid(dbm, dim));
}



/* Algorithm: remove upper bounds for k =
 * setting all dbm[k,j] to infinity except
 * the diagonal.
 */
void dbm_freeUp(raw_t* dbm, cindex_t dim, cindex_t k)
{
    cindex_t j;
    assert(dbm && k > 0 && k < dim);

    for (j = 0; j < dim; ++j)
    {
        if (j != k) DBM(k,j) = dbm_LS_INFINITY;
    }

    assertx(dbm_isValid(dbm, dim));
    
}


/* Algorithm:
 * Removing all upper bounds =
 * setting all bounds dbm[i,j] to infinity
 * except for the 1st row and the diagonal.
 * See dbm_init.
 */
void dbm_freeAllUp(raw_t *dbm, cindex_t dim)
{
    cindex_t n;
    assert(dim && dbm);

    /* like dbm_init but keep the 1st row */

    base_fill(dbm+dim, dim*(dim-1), dbm_LS_INFINITY);
    for(n = dim - 1; n != 0; --n)
    {
        dbm += dim + 1;
        *dbm = dbm_LE_ZERO;
    }
}

/* Partial test from dbm_isEqualToInit.
 */
BOOL dbm_isFreedAllUp(const raw_t *dbm, cindex_t dim)
{
    assert(dim && dbm);

    /* 1st element of diagonal + 1st element of 2nd row */

    if (*dbm != dbm_LE_ZERO) return FALSE;
    if (dim < 2) return TRUE;
    dbm += dim;
    if (*dbm != dbm_LS_INFINITY) return FALSE;

    /* remaining */

    if (dim > 2)
    {
        /* dim = # of rows
         * dim-1 = # of rows starting from 
         * diagonal, ending to next diagonal element
         * dim-2 because we've read the 1st row
         */
        size_t nbLines = dim - 2;
        do {
            if ((dbm[1] ^ dbm_LE_ZERO) |
                base_diff(dbm+2, dim, dbm_LS_INFINITY))
            {
                return FALSE;
            }
            dbm += dim + 1;
        } while(--nbLines);
    }
    
    /* last element of diagonal */

    return (BOOL)(*++dbm == dbm_LE_ZERO);
}

/* Algorithm:
 * Removing all lower bounds for a clock k =
 * for all i < dim: dbm[i,k] = dbm[i,0]
 * since dbm[0,k] (lower bound for xk) is set to 0,
 * the shortest path becomes dbm[i,k] = dbm[i,0] + dbm[0,k].
 */
void dbm_freeDown(raw_t *dbm, cindex_t dim, cindex_t k)
{
    cindex_t i;
    assert(dbm && k > 0 && k < dim);

    for (i = 0; i < dim; ++i)
    {
        if (i != k) DBM(i,k) = DBM(i,0);
    }

    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * run dbm_freeDown for all k, though modify
 * the DBM row by row and not column by column.
 */
void dbm_freeAllDown(raw_t* dbm, cindex_t dim)
{
    cindex_t i,j;
    assert(dbm && dim);
    for(i = 0; i < dim; ++i)
    {
        for(j = 1; j < dim; ++j)
        {
            if (i != j) DBM(i,j) = DBM(i,0);
        }
    }

    assertx(dbm_isValid(dbm, dim));
}


uint32_t dbm_testFreeAllDown(const raw_t *dbm, cindex_t dim)
{
    cindex_t i,j;
    assert(dbm && dim);
    for(i = 0; i < dim; ++i)
    {
        for(j = 1; j < dim; ++j)
        {
            if (i != j && DBM(i,j) != DBM(i,0))
            {
                return (j << 16) | i;
            }
        }
    }
    return 0;
}


/* Algorithm:
 * for all k != i:
 *   dbm[i,k] = dbm[j,k]
 *   dbm[k,i] = dbm[k,j]
 */
void dbm_updateClock(raw_t *dbm, cindex_t dim,
                     cindex_t i, cindex_t j)
{
    cindex_t k;
    assert(dbm && i > 0 && j > 0 && i < dim && j < dim);

    if (i == j) return;

    for (k = 0; k < dim; ++k)
    {
        if (i != k)
        {
            DBM(i,k) = DBM(j,k);
            DBM(k,i) = DBM(k,j);
        }
    }

    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * for all i != k:
 *   dbm[k,i] += uBound (<= value)
 *   dbm[i,k] += lBound (<= -value)
 * case i==k does not matter in practice
 */
void dbm_updateIncrement(raw_t *dbm, cindex_t dim,
                         cindex_t k, int32_t value)
{
    cindex_t i;
    assert(dbm && k > 0 && k < dim);

    if (value == 0) return;

    for (value <<= 1, i = 0; i < dim; ++i)
    {
        if (DBM(k,i) < dbm_LS_INFINITY) DBM(k,i) += value;
        if (DBM(i,k) < dbm_LS_INFINITY) DBM(i,k) -= value;
    }

    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * for all k != i:
 *   dbm[i,k] = dbm[j,k] + upper bound
 *   dbm[k,i] = dbm[k,j] + lower bound
 */
void dbm_update(raw_t *dbm, cindex_t dim,
                cindex_t i, cindex_t j, int32_t value)
{
    cindex_t k;
    assert(dbm && i > 0 && j > 0 && i < dim && j < dim);

    if (i == j)
    {
        dbm_updateIncrement(dbm, dim, i, value);
        return;
    }
    if (value == 0)
    {
        dbm_updateClock(dbm, dim, i, j);
        return;
    }

    for (value <<= 1, k = 0; k < dim; ++k)
    {
        DBM(i,k) = dbm_rawInc(DBM(j,k), value);
        DBM(k,i) = dbm_rawDec(DBM(k,j), value);
    }
    DBM(i,i) = dbm_LE_ZERO; /* restore diagonal */

    assertx(dbm_isValid(dbm, dim));
}


/* symmetric contrain clock,0 and 0,clock */
BOOL dbm_constrainClock(raw_t *dbm, cindex_t dim, cindex_t clock, int32_t value)
{
    raw_t c;
    raw_t *dbm_k0 = &DBM(clock,0);
    raw_t *dbm_0k = &DBM(0,clock);
    BOOL changed = FALSE;

    assert(dbm != NULL && dim > 0 &&
           clock < dim && clock > 0 &&
           value != dbm_INFINITY);
    
    /* constrain clock,0 */
    c = dbm_bound2raw(value, dbm_WEAK);
    if (*dbm_k0 > c)
    {
        *dbm_k0 = c;
        if (dbm_negRaw(c) >= *dbm_0k)
        {
            dbm_k0[clock] = DBM(0,0) = -1;
            return FALSE;
        }
        changed = TRUE;
    }

    /* constrain 0,clock */
    c = dbm_bound2raw(-value, dbm_WEAK);
    if (*dbm_0k > c)
    {
        *dbm_0k = c;
        if (dbm_negRaw(c) >= *dbm_k0)
        {
            dbm_k0[clock] = DBM(0,0) = -1;
            return FALSE;
        }
        changed = TRUE;
    }
    
    /* close and return !empty */
    return (BOOL)
        !changed ||
        /* not closeij, we need to be symmetric */
        (dbm_close1(dbm, dim, 0) &&
         dbm_close1(dbm, dim, clock));
}


/** Floyd's shortest path algorithm for the
 * closure. Complexity cubic in dim.
 * Algorithm:
 * for all k < dim do
 *   for all i < dim do
 *     for all j < dim do
 *       if dbm[i,j] > dbm[i,k]+dbm[k,j]
 *         dbm[i,j] = dbm[i,k]+dbm[k,j]
 */
BOOL dbm_close(raw_t *dbm, cindex_t dim)
{
    assert(dim && dbm);

    raw_t *dbm_kdim = dbm; /* &dbm[k*dim] */
    cindex_t k = 0;
    
    ASSERT_DIAG_OK(dbm, dim);
    
    do { /* loop on k */
        raw_t *dbm_idim = dbm; /* &dbm[i*dim]  */
        cindex_t i = 0;
        
        assert(k < dim && dbm_kdim == &dbm[k*dim]);
        
        do { /* loop on i */
            
            /* optimization: if i == k
             * the loop will tighten dbm[i,j] with dbm[i,i] + dbm[i,j]
             * which will change nothing, even for i == j (if < 0 then
             * more < 0, but still empty).
             */
            if (i != k)
            {
                raw_t dbm_ik = dbm_idim[k]; /* dbm[i,k] == dbm[i*dim+k] */
                
                assert(i < dim && dbm_idim == &dbm[i*dim]);
                
                if (dbm_ik != dbm_LS_INFINITY)
                {
                    cindex_t j = 0;
                    do { /* loop on j */
                        /* could try if j != k but it isn't worth */
                        
                        raw_t dbm_kj = dbm_kdim[j]; /* dbm[k,j] == dbm[k*dim+j] */
                        
                        if (dbm_kj != dbm_LS_INFINITY)
                        {
                            raw_t dbm_ikkj = dbm_addFiniteFinite(dbm_ik, dbm_kj);
                            if (dbm_idim[j] > dbm_ikkj) /* dbm[i,j] > dbm[i,k]+dbm[k,j] */
                            {
                                dbm_idim[j] = dbm_ikkj;
                            }
                        }
                    } while(++j < dim);
                }
                /* *MUST* be there (ideally before the loop on j)
                 * to avoid numerical problems: computation may
                 * diverge to -infinity and go beyond reasonable
                 * bounds and produce numerical errors in case of
                 * empty DBMs. This was exhibited by extensive testing.
                 * The old implementation is therefor numerically wrong.
                 * Having the test here also allows for testing non
                 * emptiness of a DBM.
                 * It is still possible to have numerical errors but
                 * it is much more difficult now.
                 */
                if (dbm_idim[i] < dbm_LE_ZERO)
                {
                    *dbm = -1; /* mark at beginning */
                    return FALSE;
                }
            }
            assert(dbm_idim[i] == dbm_LE_ZERO);
            
            dbm_idim += dim;
        } while(++i < dim);
        
        dbm_kdim += dim;
    } while(++k < dim);


    ASSERT_NOT_EMPTY(dbm, dim);
    return TRUE;
}


/** Floyd's shortest path algorithm for the
 * closure. Complexity cubic in dim.
 * Algorithm:
 * for all k < dim do
 *   for all i < dim do
 *     for all j < dim do
 *       if dbm[i,j] > dbm[i,k]+dbm[k,j]
 *         needs update -> return false
 */
BOOL dbm_isClosed(const raw_t *dbm, cindex_t dim)
{
    cindex_t k,i,j;
    assert(dim && dbm);

    for (k = 0; k < dim; ++k)
    {
        for (i = 0; i < dim; ++i)
        {
            if (DBM(i,k) < dbm_LS_INFINITY)
            {
                for (j = 0; j < dim; ++j)
                {
                    if (DBM(k,j) < dbm_LS_INFINITY &&
                        DBM(i,j) > dbm_addFiniteFinite(DBM(i,k), DBM(k,j)))
                    {
                        return FALSE;
                    }
                }
            }
        }
    }

    return TRUE;
}


/** Floyd's shortest path algorithm for the
 * closure, applied only to some clocks.
 * Complexity quadratic in dim * # of clocks
 * to iterate on.
 * Algorithm:
 * for all k < dim s.t. clock k is touched do
 *   for all i < dim do
 *     for all j < dim do
 *       if dbm[i,j] > dbm[i,k]+dbm[k,j]
 *         dbm[i,j] = dbm[i,k]+dbm[k,j]
 */
BOOL dbm_closex(raw_t *dbm, cindex_t dim, const uint32_t *touched)
{
    if (dim > 1) /* if other clocks than reference clock */
    {
        /* We read 32 bits at once, so if we skip remaining
         * bits, this will set the base to (previous +) 32.
         */
        cindex_t clockBase = 0;
        
        /* For debugging */
        DODEBUG(const uint32_t *debugTouched = touched);
        DODEBUG(size_t nb_k = base_countBitsN(touched, bits2intsize(dim)));
        assert(dbm && touched);
        assert(nb_k <= dim);
        assert(nb_k != 0 || !dbm_isEmpty(dbm, dim));
        ASSERT_DIAG_OK(dbm, dim);

        do {
            cindex_t k;
            uint32_t bits;
            
            /* loop on k; 32 at a time */
            for(bits = *touched++, k = clockBase; bits != 0; ++k, bits >>= 1)
            {
                raw_t *dbm_kdim, *dbm_idim;
                cindex_t i;

                assert(k < dim);
                assert(base_getOneBit(debugTouched, k) == (bits & 1));
                
                for(; (bits & 1) == 0; ++k, bits >>= 1); /* must find one */
                dbm_kdim = &dbm[k*dim];
                dbm_idim = dbm; /* &dbm[i*dim] */
                i = 0;          /* i = dim - ni */
                    
                do { /* loop on i */
                    /* optimization: see close */
                    if (i != k)
                    {
                        /* dbm[i,k] == dbm[i*dim+k] */
                        raw_t dbm_ik = dbm_idim[k];
                        
                        assert(i < dim && dbm_idim == &dbm[i*dim]);
                        
                        if (dbm_ik != dbm_LS_INFINITY)
                        {
                            cindex_t j = 0;
                            do { /* loop on j */
                                /* dbm[k,j] == dbm[k*dim+j] */
                                raw_t dbm_kj = dbm_kdim[j];
                                
                                if (dbm_kj != dbm_LS_INFINITY)
                                {
                                    raw_t dbm_ikkj = dbm_addFiniteFinite(dbm_ik, dbm_kj);
                                    if (dbm_idim[j] > dbm_ikkj)
                                    {
                                        dbm_idim[j] = dbm_ikkj;
                                    }
                                }
                            } while(++j < dim);
                        }
                        /* see close */
                        if (dbm_idim[i] < dbm_LE_ZERO) return FALSE;
                    }
                    assert(dbm_idim[i] == dbm_LE_ZERO);
                    
                    dbm_idim += dim;
                } while(++i < dim);
            }
            clockBase += 32;
        } while(clockBase < dim);
    }

    ASSERT_NOT_EMPTY(dbm, dim);
    return TRUE;
}


/** Floyd's shortest path algorithm for the
 * closure, applied only to some clocks.
 * Complexity quadratic in dim * # of clocks
 * to iterate on.
 * Algorithm:
 * for one k < dim do
 *   for all i < dim do
 *     for all j < dim do
 *       if dbm[i,j] > dbm[i,k]+dbm[k,j]
 *         dbm[i,j] = dbm[i,k]+dbm[k,j]
 */
BOOL dbm_close1(raw_t *dbm, cindex_t dim, cindex_t k)
{
    cindex_t i,j;
    assert(dim && dbm);
    ASSERT_DIAG_OK(dbm, dim);

    for (i = 0; i < dim; ++i) if (i != k)
    {
        if (DBM(i,k) < dbm_LS_INFINITY)
        {
            for (j = 0; j < dim; ++j)
            {
                if (DBM(k,j) < dbm_LS_INFINITY)
                {
                    raw_t dbm_ikkj = dbm_addFiniteFinite(DBM(i,k), DBM(k,j));
                    if (DBM(i,j) > dbm_ikkj) DBM(i,j) = dbm_ikkj;
                }
            }
        }
        if (DBM(i,i) < dbm_LE_ZERO)
        {
            DBM(0,0) = -1; /* mark empty at beginning */
            return FALSE;
        }
    }

    ASSERT_NOT_EMPTY(dbm, dim);
    return TRUE;
}


/* Specialization of Floyd's shortest path algorithm
 * when a constraint dbm[b,a] is tightened (easier
 * to have b,a: see Rokicki's thesis p 171).
 * Algorithm:
 *
 * for j in 0..dim-1 do
 *   if dbm[b,j] > dbm[b,a]+dbm[a,j] then
 *      dbm[b,j] = dbm[b,a]+dbm[a,j]
 *   endif
 * done
 *
 * for i in 0..dim-1 do
 *   if dbm[i,a] > dbm[i,b]+dbm[b,a] then
 *      dbm[i,a]=dbm[i,b]+dbm[b,a]
 *      for j in 0..dim-1 do
 *        if dbm[i,j] > dbm[i,a]+dbm[a,j] then
 *           dbm[i,j]=dbm[i,a]+dbm[a,j]
 *        endif
 *      done
 *   endif
 * done
 */
void dbm_closeij(raw_t *dbm, cindex_t dim, cindex_t b, cindex_t a)
{
    assert(dbm && a < dim && b < dim && a != b);
    assert(DBM(b,a) != dbm_LS_INFINITY && dbm_negRaw(DBM(b,a)) < DBM(a,b));

    if (dim > 2) /* if only one clock (+clock ref) then nothing to do */
    {
        raw_t *dbm_bj, *dbm_aj, *dbm_i, *end, dbm_ba;

        dbm_bj = &dbm[b*dim]; /* dbm[b,j], j = 0 */
        dbm_aj = &dbm[a*dim]; /* dbm[a,j], j = 0 */
        dbm_ba = dbm_bj[a];
        end = &dbm_bj[dim];
        do {
            if (*dbm_aj != dbm_LS_INFINITY)
            {
                raw_t bj = dbm_addFiniteFinite(dbm_ba, *dbm_aj);
                if (*dbm_bj > bj) *dbm_bj = bj;
            }
            dbm_aj++;
            dbm_bj++;
        } while (dbm_bj < end);

        dbm_aj -= dim;   /* dbm[a,j], j = 0 */
        dbm_i = dbm;     /* dbm[i,0], i = 0 */
        end = &dbm_i[dim*dim];
        do {
            if (dbm_i[b] != dbm_LS_INFINITY)
            {
                raw_t ia = dbm_addFiniteFinite(dbm_i[b], dbm_ba);
                if (dbm_i[a] > ia)
                {
                    cindex_t j = 0;
                    dbm_i[a] = ia;
                    do {
                        if (dbm_aj[j] != dbm_LS_INFINITY)
                        {
                            raw_t ij = dbm_addFiniteFinite(ia, dbm_aj[j]);
                            if (dbm_i[j] > ij) dbm_i[j] = ij;
                        }
                    } while (++j < dim);
                }
            }
            dbm_i += dim;
        } while (dbm_i < end);

        assertx(dbm_isValid(dbm, dim));
    }
}


/* Algorithm:
 * go through diagonal and accumulate the difference
 * value ^ (<=0). If one element has 1 bit that differs
 * from (<=0) then the result is !=0, which means the
 * DBM is empty.
 * NOTE: algorithm is optimistic and works best for
 * non empty DBMs.
 */
BOOL dbm_isEmpty(const raw_t *dbm, cindex_t dim)
{
    cindex_t i;
    assert(dim && dbm);
    assert(dbm_isDiagonalOK(dbm ,dim));

    i = 0;
    do { if (DBM(i,i) < dbm_LE_ZERO) return TRUE; } while(++i < dim);
    return FALSE;
}


/* Algorithm:
 * as the DBM is closed, we need to check for
 * upper bounds dbm[i,0] only.
 */
BOOL dbm_isUnbounded(const raw_t *dbm, cindex_t dim)
{
    cindex_t i;
    assert(dim && dbm);

    for (i = 1; i < dim; ++i)
    {
        if (DBM(i,0) < dbm_LS_INFINITY) return FALSE;
    }

    return TRUE;
}


/* Algorithm:
 * 1) look at all == elements: return == if no element left
 * 2)    look at all >= elements: return > if no < element
 * 3) or look at all <= elements: return < if no > element
 */
relation_t dbm_relation(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim)
{
    size_t n;

    assert(dbm1 && dbm2 && dim);
    assertx(dbm_isValid(dbm1, dim));
    assertx(dbm_isValid(dbm2, dim));

    if (dim <= 1 || dbm1 == dbm2)
    {
        return base_EQUAL;
    }

    /* n is the number of elements to compare. The first and the last
     * elements of the DBMs are on the diagonal, so there is no need
     * to compare them.
     */
    n = dim*dim - 2;

    /* We use a kind of partial evaluation of the comparison
     * algorithm. Basically the same code is repeated three times:
     *
     * 1. Assuming that the two DBMs are equal. If this turns out
     *    not to be the case, we jump to the same point in copy 2 
     *    or 3.
     * 2. Assuming that the first DBM is a subset of the second.
     * 3. Assuming that the first DBM is a superset of the second.
     */

    /* Test for == */
    do
    {
        if (*++dbm1 != *++dbm2) 
        {
            if (*dbm1 > *dbm2) 
            {
                goto TrySuperSet;
            }
            else
            {
                goto TrySubSet;
            }
        }
    } while (--n);
    return base_EQUAL;

    /* Test for > */
    do 
    {
        if (*++dbm1 < *++dbm2)
        {
            return base_DIFFERENT;
        }
    TrySuperSet:
        ;
    } while (--n);
    return base_SUPERSET;

    /* Test for < */
    do
    {
        if (*++dbm1 > *++dbm2)
        {
            return base_DIFFERENT;
        }
    TrySubSet:
        ;
    } while (--n);
    return base_SUBSET;
}

/* Specialized algorithm for <= only, we don't care about > or != */
BOOL dbm_isSubsetEq(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim)
{
    size_t n;

    assert(dbm1 && dbm2 && dim);
    assertx(dbm_isValid(dbm1, dim));
    assertx(dbm_isValid(dbm2, dim));

    if (dim <= 1 || dbm1 == dbm2)
    {
        return TRUE;
    }

    n = dim*dim - 2; /* dim >= 2, ok */

    do
    {
        if (*++dbm1 > *++dbm2) 
        {
            return FALSE;
        }
    } while (--n);

    return TRUE;
}

/* Relax all non infinite bounds */
void dbm_relaxAll(raw_t* dbm, cindex_t dim)
{
    const raw_t *end;
    assert(dbm && dim);
    
    /* end = after DBM and start with skipping the diagonal */
    for(end = dbm++ + dim*dim; dbm < end; ++dbm)
    {
        if (dbm_rawIsStrict(*dbm) && *dbm != dbm_LS_INFINITY)
        {
            *dbm = dbm_weakRaw(*dbm);
        }
    }
}

/* Algorithm:
 * for i < dim:
 *   weaken dbm[i,clock] and check that it is not
 *   tightened by dbm[i,j]+dbm[j,clock] for all j
 *   considering that all dbm[j,clock] are going to
 *   be tightened.
 * We do not change infinity, it has to stay strict.
 */
void dbm_relaxDownClock(raw_t *dbm, cindex_t dim, cindex_t clock)
{
    cindex_t i,j;
    assert(dbm && dim);
    assertx(dbm_isValid(dbm, dim));
    
    for(i = 0; i < dim; ++i)
    {
        if (DBM(i,clock) < dbm_LS_INFINITY && dbm_rawIsStrict(DBM(i,clock)))
        {
            DBM(i,clock) = dbm_weakRaw(DBM(i,clock));
            
            /* Input DBM is closed and we loosen an upper bound:
             * only way to break the closed form is if there exists
             * a strict bound dbm[i,j] that implies the strict bound
             * dbm[i,clock] since dbm[j,clock] will be weakened too.
             */
            for(j = 0; j < dim; ++j)
            {
                raw_t cik;
                if (DBM(i,j) < dbm_LS_INFINITY &&
                    DBM(j,clock) < dbm_LS_INFINITY &&
                    (cik = dbm_addFiniteWeak(DBM(i,j), dbm_weakRaw(DBM(j,clock)))) < DBM(i,clock))
                {
                    DBM(i,clock) = cik; // tighten back
                    break;
                }
            }
        }
    }
    
    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * symmetric to relaxDownClock:
 * for i < dim: weaken dbm[clock,i]...
 */
void dbm_relaxUpClock(raw_t *dbm, cindex_t dim, cindex_t clock)
{
    cindex_t i,j;
    assert(dbm && dim);
    assertx(dbm_isValid(dbm, dim));

    for(i = 0; i < dim; ++i)
    {
        if (DBM(clock,i) < dbm_LS_INFINITY && dbm_rawIsStrict(DBM(clock,i)))
        {
            DBM(clock,i) = dbm_weakRaw(DBM(clock,i));
            
            /* Input DBM is closed and we loosen an upper bound:
             * only way to break the closed form is if there exists
             * a strict bound dbm[i,j] that implies the strict bound
             * dbm[i,clock] since dbm[j,clock] will be weakened too.
             */
            for(j = 0; j < dim; ++j)
            {
                raw_t cki;
                if (DBM(j,i) < dbm_LS_INFINITY &&
                    DBM(clock,j) < dbm_LS_INFINITY &&
                    (cki = dbm_addFiniteWeak(DBM(j,i), dbm_weakRaw(DBM(clock,j)))) < DBM(clock,i))
                {
                    DBM(clock,i) = cki; // tighten back
                    break;
                }
            }
        }
    }
    
    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * for 0 <= i < dim, tighten with the corresponding strict constraint.
 * close.
 */
BOOL dbm_tightenDown(raw_t *dbm, cindex_t dim)
{
    assertx(dbm_isValid(dbm, dim));

    if (dim > 1)
    {
        cindex_t j, count = 0, cj = 0;
        uint32_t touched[bits2intsize(dim)];
        base_resetBits(touched, bits2intsize(dim));

        j = 1;
        do {
            if (dbm_rawIsWeak(DBM(0,j)))
            {
                DBM(0,j) = dbm_strictRaw(DBM(0,j));
                if (dbm_negRaw(DBM(0,j)) >= DBM(j,0))
                {
                    DBM(0,0) = -1; /* mark empty */
                    return FALSE;
                }
                count++;
                cj = j;
                base_setOneBit(touched, j);
            }
        } while(++j < dim);

        /* choose best close */
        if (count > 1)
        {
            base_setOneBit(touched, 0);
            return dbm_closex(dbm, dim, touched);
        }
        else if (count == 1)
        {
            dbm_closeij(dbm, dim, 0, cj);
        }
        ASSERT_NOT_EMPTY(dbm, dim);
    }

    return TRUE;
}

/* Algorithm:
 * for 0 <= i < dim, tighten with the corresponding strict constraint.
 * close.
 */
BOOL dbm_tightenUp(raw_t *dbm, cindex_t dim)
{
    assertx(dbm_isValid(dbm, dim));

    if (dim > 1)
    {
        cindex_t i, count = 0, ci = 0;
        uint32_t touched[bits2intsize(dim)];
        base_resetBits(touched, bits2intsize(dim));

        i = 1;
        do {
            if (dbm_rawIsWeak(DBM(i,0)))
            {
                DBM(i,0) = dbm_strictRaw(DBM(i,0));
                if (dbm_negRaw(DBM(i,0)) >= DBM(0,i))
                {
                    DBM(0,0) = -1; /* mark empty */
                    return FALSE;
                }
                count++;
                ci = i;
                base_setOneBit(touched, i);
            }
        } while(++i < dim);

        /* choose best close */
        if (count > 1)
        {
            base_setOneBit(touched, 0);
            return dbm_closex(dbm, dim, touched);
        }
        else if (count == 1)
        {
            dbm_closeij(dbm, dim, ci, 0);
        }
        ASSERT_NOT_EMPTY(dbm, dim);
    }

    return TRUE;
}

/* Algorithm:
 * for 0 <= i < dim, 0 <= j < dim
 *   check that xi-xj from the point <= dbm[i,j]
 */
BOOL dbm_isPointIncluded(const int32_t *pt, const raw_t *dbm, cindex_t dim)
{
    cindex_t i,j;
    assert(pt && dbm && dim);

    for (i = 0; i < dim; ++i)
    {
        for (j = 0; j < dim; ++j)
        {
            /* dbm_WEAK because we want the point included */
            if (dbm_bound2raw(pt[i] - pt[j], dbm_WEAK) > DBM(i,j))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

/* Check that all xi-xj satisfy the constraints.
 */
BOOL dbm_isRealPointIncluded(const double *pt, const raw_t *dbm, cindex_t dim)
{
    cindex_t i,j;
    assert(pt && dbm && dim);

    for(i = 0; i < dim; ++i)
    {
        for(j = 0; j < dim; ++j)
        {
            if (DBM(i,j) < dbm_LS_INFINITY)
            {
                double bound = dbm_raw2bound(DBM(i,j));
                /* if strict: !(pi-pj < bij) -> false
                 * if weak  : !(pi-pj <= bij) -> false
                 */
                if (dbm_rawIsStrict(DBM(i,j)))
                {
                    //if (pt[i] >= pt[j]+bound) return FALSE;
                    if (IS_GE(pt[i], pt[j]+bound)) return FALSE;
                }
                else
                {
                    //if (pt[i] > pt[j]+bound) return FALSE;
                    if (IS_GT(pt[i], pt[j]+bound)) return FALSE;
                }
            }
        }
    }
    return TRUE;
}



/** Internal function: compute redirection tables
 * table and cols (for update of DBM).
 * @see dbm_shrinkExpand for table
 * cols redirects indices from the source DBM to the
 * destination DBM for copy purposes:
 * copy row i to destination[i] from source[cols[i]]
 * @param bitSrc: source array bits
 * @param bitDst: destination array bits
 * @param bitSize: size of the arrays
 * @param table, cols: tables to compute
 * @return dimension of the new DBM
 */
cindex_t dbm_computeTables(const uint32_t *bitSrc,
                          const uint32_t *bitDst,
                          size_t bitSize,
                          cindex_t *table,
                          cindex_t *cols)
{
    cindex_t dimDst = 0, j = 0;

    /* debug */
    DODEBUG(size_t check = base_countBitsN(bitDst, bitSize));

    /* compute: table cols rows dimDst
     */
    do {
        uint32_t b1 = *bitSrc++;
        uint32_t b2 = *bitDst++;
        cindex_t *tab = table;
        table += 32; /* treat by bunch of 32 bits */

        while(b1 | b2)      /* index in src or dst */
        {
            /* if b1 & 1: index in src
             * if b2 & 1: index in dst
             *
             * if (b1 & 1) then j++ and
             *   if (b2 & 1) cols[dimDst] = j
             * else don't do j++ and
             *   if (b2 & 1) don't copy cols[dimDst]
             *
             * This is encoded as:
             * j += (b1 & 1) for the conditional +
             * j | ((b1 & 1) - 1) for the conditional set
             * that gives j | 0 if (b1 & 1)
             * and j | 0xffffffff if not (b1 & 1)
             * The value 0xffffffff has also the property
             * that (a == 0xffffffff) == (~a == 0)
             */

            if (b2 & 1)
            {
                *tab = dimDst;
                cols[dimDst] = j | ((b1 & 1) - 1);
                dimDst++;
            }
            j += (b1 & 1);

            b1 >>= 1;
            b2 >>= 1;
            tab++;
        }
    } while(--bitSize);

    assert(cols[0] == 0); /* always copy reference clock(0) */
    assert(dimDst > 0 && check == dimDst);

    return dimDst;
}

void dbm_updateDBM(raw_t *dbmDst, const raw_t *dbmSrc,
                   cindex_t dimDst, cindex_t dimSrc,
                   const cindex_t *cols)
{
    cindex_t i, j;
    DODEBUGX(raw_t *saveDBM = dbmDst);

    *dbmDst = dbm_LE_ZERO;
    if (dimDst <= 1) return;

    /* copy constraint[i] or reference(0) for new clock constraints
     */
    j = 1;
    do {
        assert(!~cols[j] || cols[j] < dimSrc);
        dbmDst[j] = (~cols[j]) ? dbmSrc[cols[j]] : dbm_LE_ZERO;
    } while(++j < dimDst);
        
    /* -------- the rest: infinity for new ------- */
    i = 1;
    do
    {
        /* write row i */
        dbmDst += dimDst;

        if (~cols[i]) /* if copy from source */
        {
            const raw_t *src = dbmSrc + dimSrc*cols[i];
            raw_t constraint0 = src[0];

            /* line xi-xj where xi was used previously
             * if xi-xj defined then copy
             * else xi-xj = xi-x0
             * because xk-xj undefined for all k and xi-x0
             * was the tightest bound in the previous DBM
             */
            dbmDst[0] = constraint0;
            
            assert(dimDst > 1);
            j = 1;
            do {
                dbmDst[j] = (~cols[j]) ? src[cols[j]] : constraint0;
            } while(++j < dimDst);
        }
        else /* insert new row */
        {
            /* line xi-xj where xi is new
             * xi-xk = inf except for xi-xi = 0
             */
            j = 0;
            do {
                dbmDst[j] = dbm_LS_INFINITY;
            } while(++j < dimDst);
        }
        dbmDst[i] = dbm_LE_ZERO; /* correct diagonal */

    } while(++i < dimDst);

    assertx(dbm_isValid(saveDBM, dimDst));
}


/* Algorithm:
 * - compute indirection table together with
 *   with rows and cols to copy
 * - partial copy of DBM and fill the blanks
 */
cindex_t dbm_shrinkExpand(const raw_t *dbmSrc,
                         raw_t *dbmDst,
                         cindex_t dimSrc,
                         const uint32_t *bitSrc,
                         const uint32_t *bitDst,
                         size_t bitSize,
                         cindex_t *table)
{
    cindex_t maxDim = bitSize << 5; /* *32 = max # of bits */
    cindex_t dimDst;
    cindex_t cols[maxDim]; /* index j to copy          */

    /* no null pointers */
    assert(dbmSrc && dbmDst);
    assert(bitSrc && bitDst && table);

    /* at least ref clock */
    assert(bitSize && dimSrc);
    assert(*bitSrc & *bitDst & 1);

    /* source != destination */
    assert(dbmSrc != dbmDst);

    /* correct dim */
    assert(base_countBitsN(bitSrc, bitSize) == dimSrc);
    assert(dimSrc <= maxDim);

    /* some job to do */
    assert(!base_areBitsEqual(bitSrc, bitDst, bitSize));

    /* compute indirection tables table & cols */
    dimDst = dbm_computeTables(bitSrc, bitDst, bitSize, table, cols);

    assert(dimDst && dimDst <= maxDim);

    /* shrink and expand DBM now:
     * equivalent to copy existing clock constraints and
     * apply free to the new ones.
     */
    dbm_updateDBM(dbmDst, dbmSrc, dimDst, dimSrc, cols);

    return dimDst;
}


/* Algorithm:
 * for 0 <= i < dim, 0 <= j < dim:
 *   if dbm[i,j] > max_xi then dbm[i,j] = infinity
 *   if dbm[i,j] < -max_xj then dbm[i,j] = <-max_xj
 *
 * NOTE: 1st row treated separately.
 */
void dbm_extrapolateMaxBounds(raw_t *dbm, cindex_t dim, const int32_t *max)
{
    cindex_t i,j;
    int changed = FALSE;
    assert(dbm && dim > 0 && max);

    /* 1st row */
    for (j = 1; j < dim; ++j)
    {
        if (dbm_raw2bound(DBM(0,j)) < -max[j])
        {
            DBM(0,j) = (max[j] >= 0 
                        ? dbm_bound2raw(-max[j], dbm_STRICT) 
                        : dbm_LE_ZERO);

            changed |= (max[j] > -dbm_INFINITY);
        }
    }

    /* other rows */
    for (i = 1; i < dim; ++i)
    {
        for (j = 0; j < dim; ++j) if (i != j)
        {
            if (max[j] == -dbm_INFINITY)
            {
                DBM(i,j) = DBM(i,0);
            }
            else
            {
                /* if dbm[i,j] > max_xi (upper bound)
                 *    dbm[i,j] = infinity
                 * else if dbm[i,j] < -max_xj (lower bound)
                 *    dbm[i,j] = lower bound
                 * fi
                 */
                int32_t bound = dbm_raw2bound(DBM(i,j));
                if (bound > max[i] && bound != dbm_INFINITY)
                {
                    DBM(i,j) = dbm_LS_INFINITY; /* raw */
                    changed |= (max[i] > -dbm_INFINITY); /* bound */
                }
                else if (bound < -max[j])
                {
                    DBM(i,j) = dbm_bound2raw(-max[j], dbm_STRICT);
                    changed = TRUE;
                }
            }
        }
    }
#ifndef NCLOSELU
    if (changed) dbm_closeLU(dbm, dim, max, max);
#else
    if (changed) dbm_close(dbm, dim);
#endif
    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * for 0 <= i < dim, 0 <= j < dim:
 * Update dbm[i,j] with
 * - infinity if dbm[i,j] > max_xi
 * - infinity if dbm[0,i] < -max_xi
 * - infinity if dbm[0,j] < -max_xj, i != 0
 * - <-max_xj if dbm[i,j] < -max_xj, i == 0
 *
 * In practice, we update the 1st row last because its
 * values are used as tests for the rest of the DBM. The
 * tests must be done on the original values and not the
 * updated ones.
 */
void dbm_diagonalExtrapolateMaxBounds(raw_t *dbm, cindex_t dim, const int32_t *max)
{
    cindex_t i,j;
    raw_t diff = 0;
    assert(dbm && dim > 0 && max);

    for (i = 1; i < dim; ++i)
    {
        if (dbm_raw2bound(DBM(0,i)) < -max[i])
        {
            /* case j == 0 */
            /* diff |= DBM(i,0) ^ dbm_LS_INFINITY; */
            DBM(i,0) = dbm_LS_INFINITY;
            raw_t newji = (max[i] >= 0
                           ? dbm_bound2raw(-max[i], dbm_STRICT)
                           : dbm_LE_ZERO);
            /* diff |= DBM(0,i) ^ newji; */
            DBM(0,i) = newji;
            /* cases j > 0 */
            for(j = 1; j < dim; ++j) if (i != j)
            {
                /* diff |= DBM(i,j) ^ dbm_LS_INFINITY; */
                DBM(i,j) = dbm_LS_INFINITY;
                diff |= DBM(j,i) ^ dbm_LS_INFINITY;
                DBM(j,i) = dbm_LS_INFINITY;
            }
        }
        else
        {
            for (j = 0; j < dim; ++j) if (i != j)
            {
                if (DBM(i,j) != dbm_LS_INFINITY && dbm_raw2bound(DBM(i,j)) >  max[i])
                {
                    DBM(i,j) = dbm_LS_INFINITY;
                    diff = 1;
                }
            }
        }
    }
#ifndef NCLOSELU
    if (diff) dbm_closeLU(dbm, dim, max, max);
#else
    if (diff) dbm_close(dbm, dim);
#endif
    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * for 0 <= i < dim, 0 <= j < dim:
 * - if dbm[i,j] > lower_xi then dbm[i,j] = infinity
 * - if dbm[i,j] < -upper_xj then dbm[i,j] = < -upper_xj
 *
 * NOTE: 1st row treated separately.
 */
void dbm_extrapolateLUBounds(raw_t *dbm, cindex_t dim, const int32_t *lower, const int32_t *upper)
{
    cindex_t i,j;
    int changed = FALSE;
    assert(dbm && dim > 0 && lower && upper);

    /* 1st row */
    for (j = 1; j < dim; ++j)
    {
        if (dbm_raw2bound(DBM(0,j)) < -upper[j])
        {
            DBM(0,j) = (upper[j] >= 0 
                        ? dbm_bound2raw(-upper[j], dbm_STRICT) 
                        : dbm_LE_ZERO);

            changed |= (upper[j] > -dbm_INFINITY);
        }
    }

    /* other rows */
    for (i = 1; i < dim; ++i)
    {
        for (j = 0; j < dim; ++j) if (i != j)
        {
            if (upper[j] == -dbm_INFINITY)
            {
                DBM(i,j) = DBM(i,0);
            }
            else
            {
                /* if dbm[i,j] > max_xi (upper bound)
                 *    dbm[i,j] = infinity
                 * else if dbm[i,j] < -max_xj (lower bound)
                 *    dbm[i,j] = lower bound
                 * fi
                 */
                int32_t bound = dbm_raw2bound(DBM(i,j));
                if (bound > lower[i] && bound != dbm_INFINITY)
                {
                    DBM(i,j) = dbm_LS_INFINITY; /* raw */
                    changed |= (lower[i] > -dbm_INFINITY); /* bound */
                }
                else if (bound < -upper[j])
                {
                    DBM(i,j) = dbm_bound2raw(-upper[j], dbm_STRICT);
                    changed = TRUE;
                }
            }
        }
    }
#ifndef NCLOSELU
    if (changed) dbm_closeLU(dbm, dim, lower, upper);
#else
    if (changed) dbm_close(dbm, dim);
#endif
    assertx(dbm_isValid(dbm, dim));
}


/* Algorithm:
 * for 0 <= i < dim, 0 <= j < dim:
 * Update dbm[i,j] with
 * - infinity if dbm[i,j] > lower_xi
 * - infinity if dbm[0,i] < -lower_xi
 * - infinity if dbm[0,j] < -upper_xj for i != 0
 * - <-upper_xj if dbm[0,j] < -upper_xj for i = 0
 *
 * In practice, we update the 1st row last because its
 * values are used as tests for the rest of the DBM. The
 * tests must be done on the original values and not the
 * updated ones.
 */
void dbm_diagonalExtrapolateLUBounds(raw_t *dbm, cindex_t dim,
                                     const int32_t *lower, const int32_t *upper)
{
    cindex_t i,j;
    raw_t diff = 0;
    assert(dbm && dim > 0 && lower && upper);

    /* other rows */
    for (i = 1; i < dim; ++i)
    {
        int infij = dbm_raw2bound(DBM(0,i)) < -lower[i];
        for (j = 0; j < dim; ++j) if (i != j)
        {
            raw_t dbmij = DBM(i,j);
            if (infij ||
                dbm_raw2bound(dbmij) >  lower[i] ||
                dbm_raw2bound(DBM(0,j)) < -upper[j])
            {
                if (!infij) diff |= dbmij ^ dbm_LS_INFINITY;
                DBM(i,j) = dbm_LS_INFINITY;
            }
        }
    }

    /* 1st row */
    for (j = 1; j < dim; ++j)
    {
        assert(dbm_raw2bound(DBM(0,j))<= lower[0]);
        
        if (dbm_raw2bound(DBM(0,j)) < -upper[j])
        {
            raw_t new0j = (upper[j] >= 0 
                           ? dbm_bound2raw(-upper[j], dbm_STRICT)
                           : dbm_LE_ZERO);
            /* diff |= DBM(0,j) ^ new0j; */
            DBM(0,j) = new0j;
        }
    }
#ifndef NCLOSELU
    if (diff) dbm_closeLU(dbm, dim, lower, upper);
#else
    if (diff) dbm_close(dbm, dim);
#endif
    assertx(dbm_isValid(dbm, dim));
}

void dbm_swapClocks(raw_t *dbm, cindex_t dim, cindex_t x, cindex_t y)
{
    raw_t *rx, *ry, *endx;
    assert(dbm && dim);
    
    /* Swap columns */
    for(rx = dbm + x, ry = dbm + y, endx = rx + dim*dim;
        rx < endx;
        rx += dim, ry += dim)
    {
        raw_t c = *rx;
        *rx = *ry;
        *ry = c;
    }

    /* Swap rows */
    for(rx = dbm + x*dim, ry = dbm + y*dim, endx = rx + dim;
        rx < endx;
        ++rx, ++ry)
    {
        raw_t c = *rx;
        *rx = *ry;
        *ry = c;
    }
}

/* Look for >0 on the diagonal.
 */
BOOL dbm_isDiagonalOK(const raw_t *dbm, cindex_t dim)
{
    cindex_t i;
    assert(dbm && dim);
    
    for (i = 0; i < dim; ++i)
    {
        if (DBM(i,i) > dbm_LE_ZERO) return FALSE;
    }

    return TRUE;
}


/* Apply the 3 tests and print debug info.
 */
BOOL dbm_isValid(const raw_t *dbm, cindex_t dim)
{
    cindex_t j;
    assert(dbm && dim);

    /* closed */
    if (!dbm_isClosed(dbm, dim))
    {
#ifndef NDEBUG
        fprintf(stderr, RED(BOLD)
                "DBM is NOT closed:" NORMAL "\n");
        dbm_printCloseDiff(stderr, dbm, dim);
#endif
        return FALSE;
    }

    /* not empty */
    if (dbm_isEmpty(dbm, dim))
    {
#ifndef NDEBUG
        fprintf(stderr, RED(BOLD)
                "DBM is empty:" NORMAL "\n");
        dbm_print(stderr, dbm, dim);
#endif
        return FALSE;
    }

    /* 1st row ok */
    for(j = 0; j < dim; ++j)
    {
        if (DBM(0,j) > dbm_LE_ZERO)
        {
#ifndef NDEBUG
            fprintf(stderr, RED(BOLD)
                    "Incorrect constraint on 1st row:" NORMAL "\n");
            dbm_print(stderr, dbm, dim);
#endif
            return FALSE;
        }
    }

    return TRUE;
}


const char* dbm_relation2string(relation_t rel)
{
    switch(rel)
    {
    case base_DIFFERENT: return "Different";
    case base_SUPERSET:  return "Superset";
    case base_SUBSET:    return "Subset";
    case base_EQUAL:     return "Equal";
    }
    return "Error";
}


/* Get max range of DBM:
 * go through all constraints and accumulate
 * the bits needed to represent the constraints.
 */
raw_t dbm_getMaxRange(const raw_t *dbm, cindex_t dim)
{
    raw_t max = 0;
    assert(dbm && dim > 0);
    
    /* Look at 2 elements per loop,
     * the last element of the diagonal
     * does not matter so it
     * is safe to just divide by 2.
     */
    for(dim = (dim*dim) >> 1; dim != 0; dbm += 2, --dim)
    {
        /* see base/utils.h */
        ADD_BITS(max, dbm[0]);
        ADD_BITS(max, dbm[1]);
    }

    return max;
}
