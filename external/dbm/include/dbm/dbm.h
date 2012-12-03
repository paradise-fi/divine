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
 * Filename: dbm.h (dbm)
 * C header.
 * 
 * Basic DBM operations.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2000, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * Not reviewed.
 * $Id: dbm.h,v 1.46 2005/09/29 16:10:43 adavid Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_DBM_DBM_H
#define INCLUDE_DBM_DBM_H

#include "base/intutils.h"
#include "hash/compute.h"
#include "base/relation.h"
#include "dbm/constraints.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @mainpage
 *
 * A dbm is defined as a squared matrix of raw_t. raw_t
 * is the encoded clock constraint @see constraints.h.
 *
 * IMPORTANT NOTE: in the system, you will typically have clocks
 * x1,x2...,xn. The dbm will have x0 as the reference clock, hence
 * dimension = n + 1 => ASSUME dimension > 0
 *
 * Format of a dbm of dimension dim:
 * dbm[i,j] = dbm[i*dim+j]
 * where dbm[i,j] = constraint xi-xj < or <= c_ij
 * The constraint encoding is described in constraints.h
 *
 * The API does not support indirection table for clocks.
 * Dynamic mappings must be resolved before calling these
 * functions. Be careful when dealing with operation that
 * involve arrays of constraints (e.g. kExtrapolate).
 * As a common assumption for all operations on DBM:
 * dim > 0, which means at least the reference clock is
 * in the DBM.
 * DBM non empty means the represented zone is non empty,
 * which is, for a closed dbm the diagonal elements are =0.
 *
 * Common pre-condition: DBMs are always of dimension >= 1.
 */


/** Initialize a DBM with:
 * - <= 0 on the diagonal and the 1st row
 * - <= infinity elsewhere
 * @param dbm: DBM to initialize.
 * @param dim: dimension.
 * @return initialized DBM.
 * @post DBM is closed.
 */
void dbm_init(raw_t *dbm, cindex_t dim);


/** Set the DBM so that it contains only 0.
 * @param dbm: DBM to set to 0
 * @param dim: dimension
 * @return zeroed DBM
 * @post DBM is closed
 */
static inline
void dbm_zero(raw_t *dbm, cindex_t dim)
{
    base_fill(dbm, dim*dim, dbm_LE_ZERO);
}


/** Equality test with trivial dbm as obtained from
 * dbm_init(dbm, dim).
 * @param dbm: DBM to test
 * @param dim: dimension.
 */
BOOL dbm_isEqualToInit(const raw_t *dbm, cindex_t dim);


/** Test if a DBM contains the zero point of not.
 * @param dbm: DBM to test.
 * @param dim: dimension.
 * @return TRUE if the DBM contains the origin 0, FALSE otherwise.
 */
BOOL dbm_hasZero(const raw_t *dbm, cindex_t dim);


/** Equality test with dbm as obtained from dbm_zero(dbm, dim, tauClock)
 * @param dbm: DBM to test
 * @param tauClock: clock that should not be set to 0.
 * @param dim: dimension
 * @return zeroed DBM
 * @post DBM is closed
 * @pre
 * - tauClock > 0 (not reference clock)
 */
static inline
BOOL dbm_isEqualToZero(const raw_t *dbm, cindex_t dim)
{
    return (BOOL)(base_diff(dbm, dim*dim, dbm_LE_ZERO) == 0);
}


/** Convex hull union between 2 DBMs.
 * @param dbm1,dbm2: DBMs.
 * @param dim: dimension
 * @pre
 * - both DBMs have the same dimension
 * - both DBMs are closed and non empty
 * @return dbm1 = dbm1 U dbm2
 * @post dbm1 is closed.
 */
void dbm_convexUnion(raw_t *dbm1, const raw_t *dbm2, cindex_t dim);


/** Intersection of 2 DBMs.
 * @param dbm1,dbm2: DBMs.
 * @param dim: dimension.
 * @pre
 * - both DBMs have the same dimension
 * - both DBMs are closed and non empty
 * @return dbm1 = dbm1 intersected with dbm2 and
 * TRUE if dbm1 is non empty.
 * @post dbm1 is closed and non empty OR dbm1 is empty.
 */
BOOL dbm_intersection(raw_t *dbm1, const raw_t *dbm2, cindex_t dim);


/** Relaxed intersection of 2 DBMs.
 * @param dbm1,dbm2: DBMs.
 * @param dim: dimension.
 * @pre
 * - both DBMs have the same dimension
 * - both DBMs are closed and non empty
 * - dim > 0
 * @return dst = relaxed(dbm1) intersected with relaxed(dbm2) and
 * TRUE if dst is non empty.
 * @post dst is closed and non empty OR dst is empty.
 */
BOOL dbm_relaxedIntersection(raw_t *dst, const raw_t *dbm1, const raw_t *dbm2, cindex_t dim);


/** Test if 2 DBMs have an intersection.
 * @param dbm1,dbm2: DBMs to test.
 * @param dim: dimension of both DBMs.
 * @return FALSE if dbm1 intersection dbm2 is empty
 * and TRUE if it *may* be non empty (not guaranteed).
 */
BOOL dbm_haveIntersection(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim);


/** Constrain a DBM with a constraint.
 * USAGE:
 * -# dbm must be closed.
 * -# reset touched: base_resetBits(touched, size)
 * -# apply constraints: dbm_constrain(...)
 * -# if not empty dbm_close(dbm, dim, touched)
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param i,j,constraint: the clock constraint xi-xj <= constraint
 * (or < constraint) according to the clock constraint encoding.
 * @param touched: bit table to keep track of modified clocks.
 * @return: FALSE if the DBM is empty, TRUE otherwise, the constrained
 * DBM, and which clocks were modified (touched).
 * It is not guaranteed that the DBM is non empty, but
 * if FALSE is returned then the DBM is guaranteed to be empty.
 * @pre
 * - touched is at least a uint32_t[bits2intsize(dim)]
 * - constraint is finite
 * - dim > 1 induced by i < dim & j < dim & i != j
 * - i < dim, j < dim, i != j
 * @post THE RESULTING DBM MAY NOT BE CLOSED, calls to isEmpty
 * can return erroneous results.
 */
BOOL dbm_constrain(raw_t *dbm, cindex_t dim,
                   cindex_t i, cindex_t j, raw_t constraint,
                   uint32_t *touched);


/** Constrain a DBM with several constraints.
 * USAGE:
 * @param dbm: DBM
 * @param dim: dimension.
 * @param constraints, n: the n constraints to use.
 * @pre
 * - DBM closed and non empty.
 * - valid constraint: not of the form xi-xi <= something
 * - dim > 1 induced by i < dim & j < dim & i != j
 * - constraints[*].{i,j} < dim
 * @return TRUE if the DBM is non empty, the constrained
 * DBM
 * @post the resulting DBM is closed if it is non empty.
 */
BOOL dbm_constrainN(raw_t *dbm, cindex_t dim,
                    const constraint_t *constraints, size_t n);


/** Constrain a DBM with several constraints using a table for
 * index translation (translates absolute clock indices to 
 * local clock indices for this DBM).
 * USAGE:
 * @param dbm: DBM
 * @param dim: dimension.
 * @param indexTable: table for index translation
 * @param constraints, n: the n constraints to use.
 * @pre
 * - DBM closed and non empty.
 * - valid constraint: not of the form xi-xi <= something
 * - dim > 1 induced by i < dim & j < dim & i != j
 * - constraints[*].{i,j} < dim
 * @return TRUE if the DBM is non empty, the constrained
 * DBM
 * @post the resulting DBM is closed if it is non empty.
 */
BOOL dbm_constrainIndexedN(raw_t *dbm, cindex_t dim, const cindex_t *indexTable,
                           const constraint_t *constraints, size_t n);


/** Constrain a DBM with one constraint.
 * If you have several constraints, it may be better to
 * use the previous functions.
 * @param dbm: DBM, ASSUME: closed.
 * @param dim: dimension.
 * @param i,j,constraint: constraint for xi-xj to use
 * @pre
 * - DBM is closed and non empty
 * - dim > 1 induced by i < dim & j < dim & i != j
 * - as a consequence: i>=0 & j>=0 & i!=j => (i or j) > 0
 *   and dim > (i or j) > 0 => dim > 1
 * - i < dim, j < dim, i != j
 * @return TRUE if the DBM is non empty and the constrained
 * DBM.
 * @post the resulting DBM is closed if it is non empty.
 */
BOOL dbm_constrain1(raw_t *dbm, cindex_t dim,
                    cindex_t i, cindex_t j, raw_t constraint);


/** Wrapper for constrain1.
 * @param dbm: DBM, assume closed
 * @param dim: dimension
 * @param c: the constraint to apply
 * @return TRUE if the DBM is non empty
 * @post the resulting DBM is closed if it is non empty
 */
static inline
BOOL dbm_constrainC(raw_t *dbm, cindex_t dim, constraint_t c)
{
    return dbm_constrain1(dbm, dim, c.i, c.j, c.value);
}


/** Delay operation.
 * Remove constraints of the form xi-x0 <= ci
 * and replace them by xi-x0 < infinity.
 * It is also called strongest post-condition.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre
 * - DBM closed and non empty
 * @post DBM is closed.
 */
void dbm_up(raw_t *dbm, cindex_t dim);


/** Delay operation with stopped clocks.
 * Apply delay except for a number of stopped clocks. The
 * constraints of the form xi-x0 <= ci are replaced by
 * xi-x0 <= infinity for the "running" clocks xi and the
 * constraints xj-xi <= cji are replaced by xj-xi <= infinity
 * for xj "running" and xi stopped (j > 0).
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre
 * - DBM closed and non empty
 * - stopped != NULL and stopped is a uint32_t[bits2intsize(dim)]
 * @post DBM is closed
 */
void dbm_up_stop(raw_t* dbm, cindex_t dim, const uint32_t* stopped);


/** Internal dbm_down, don't use directly */
void dbm_downFrom(raw_t *dbm, cindex_t dim, cindex_t j0);

/** Inverse delay operation.
 * Also called weakest pre-condition.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre
 * - DBM closed and non empty
 * @post DBM is closed.
 */
static inline
void dbm_down(raw_t *dbm, cindex_t dim)
{
    dbm_downFrom(dbm, dim, 1);
}


/** Inverse delay operation with stopped clocks.
 * This one is more tricky: 1) It lowers the lower
 * bounds of the clocks to 0 or the minimum it can
 * be (due to other constraints) *for the clocks
 * that are running*. 2) It recomputes the lower
 * diagonal constraints between running and stopped
 * clocks.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre
 * - DBM clocsed and non empty.
 * - stopped != NULL and stopped is a uint32_t[bits2intsize(dim)]
 * @post DBM is closed.
 */
void dbm_down_stop(raw_t *dbm, cindex_t dim, const uint32_t *stopped);


/** Former "reset" operation, properly called update.
 * Implement the operation x := v, where x is a clock and v
 * a positive integer.
 * @param dbm: DBM
 * @param dim: dimension.
 * @param index: clock index.
 * @param value: value to reset to (may be non null), must be >=0..
 * @pre
 * - DBM closed and non empty
 * - dim > 1 induced by index > 0 and index < dim
 * - index > 0: never reset reference clock, index < dim
 * - value is finite and not an encoded clock constraint
 * - value >= 0 (int used for type convenience and compatibility).
 * @post DBM is closed.
 */
void dbm_updateValue(raw_t *dbm, cindex_t dim,
                     cindex_t index, int32_t value);


/** Free operation. Remove all constraints (lower and upper
 * bounds) for a given clock, i.e., set them to infinity,
 * except for x0-xk <= 0.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param index: the clock to free.
 * @pre
 * - DBM is closed and non empty
 * - dim > 1 induced by index > 0 && index < dim
 * - index > 0, index < dim
 * @post DBM is closed.
 */
void dbm_freeClock(raw_t *dbm, cindex_t dim, cindex_t index);

              
/** Free all upper bounds for a given clock.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param index: the concerned clock.
 * @pre DBM closed and non empty and 0 < index < dim
 * @post DBM is closed and non empty.
 */
void dbm_freeUp(raw_t *dbm, cindex_t dim, cindex_t index);


/** Free all upper bounds for all clocks.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre DBM closed and non empty.
 * @post DBM closed and non empty.
 */
void dbm_freeAllUp(raw_t* dbm, cindex_t dim);


/** @return true if dbm_freeAllUp(dbm,dim) has
 * no effect on dbm.
 * @param dbm,dim: DBM of dimension dim to test.
 */
BOOL dbm_isFreedAllUp(const raw_t *dbm, cindex_t dim);


/** @return 0 if dbm_freeAllDown(dbm,dim) has
 * no effect on dbm, or (j << 16)|i otherwise
 * where i and j are the indices from where dbm
 * differs from its expected result.
 * @param dbm,dim: DBM of dimension dim to test.
 */
uint32_t dbm_testFreeAllDown(const raw_t *dbm, cindex_t dim);


/** Free all lower bounds for a given clock.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param index: index of the clock to "down-free"
 * @pre
 * - dim > 1 induced by index > 0 && index < dim
 * - index > 0, index < dim
 * - DBM closed and non empty
 * @post DBM is closed and non empty.
 */
void dbm_freeDown(raw_t *dbm, cindex_t dim, cindex_t index);


/** Free all lower bounds for all clocks.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre DBM closed and non empty.
 * @post DBM closed and non empty.
 */
void dbm_freeAllDown(raw_t *dbm, cindex_t dim);


/** Clock copy operation = update clock:
 * xi := xj, where xi and xj are clocks.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param i,j: indices of the clocks.
 * @pre
 * - DBM closed and non empty.
 * - dim > 1 induced by i > 0 & i < dim
 * - i > 0, j > 0, i < dim, j < dim
 * @post DBM is closed.
 */
void dbm_updateClock(raw_t *dbm, cindex_t dim,
                     cindex_t i, cindex_t j);


/** Increment operation: xi := xi + value, where xi is a clock.
 * WARNING: if offset is negative it may be incorrect to use this.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param i: index of the clock.
 * @param value: the increment.
 * @pre
 * - DBM closed and non empty.
 * - dim > 1 induced by i > 0 && i < dim
 * - i > 0, i < dim
 * - if value < 0, then |value| <= min bound of clock i
 * @post DBM is closed.
 */
void dbm_updateIncrement(raw_t *dbm, cindex_t dim,
                         cindex_t i, int32_t value);


/** More general update operation: xi := xj + value,
 * where xi and yi are clocks.
 * WARNING: if offset is negative it may be incorrect to use this.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param i,j: indices of the clocks.
 * @param value: the increment.
 * @pre
 * - DBM is closed and non empty.
 * - dim > 1 induced by i > 0 && i < dim
 * - i > 0, j > 0, j < dim, i < dim
 * - if value < 0 then |value| <= min bound of clock j
 * @post DBM is closed.
 */
void dbm_update(raw_t *dbm, cindex_t dim,
                cindex_t i, cindex_t j, int32_t value);


/** @return constraint clock to == value, and return
 * if the result is non empty.
 * @param dbm,dim: DBM of dimension dim
 * @param clock: clock index for the reset
 * @param value: reset value to apply
 * @pre clock != 0 (not ref clock) && clock < dim
 */
BOOL dbm_constrainClock(raw_t *dbm, cindex_t dim, cindex_t clock, int32_t value);


/** Satisfy operation.
 * Check if a DBM satisfies a constraint. The DBM is not modified.
 * WARNING: using this for conjunction of constraints is incorrect
 * because the DBM is not modified.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param i,j: indices of clocks for the clock constraint.
 * @param constraint: the encoded constraint.
 * @pre
 * - DBM is closed and non empty.
 * - dim > 0
 * - i != j (don't touch the diagonal)
 * - i < dim, j < dim
 * @return TRUE if the DBM satisfies the constraint.
 */
static inline
BOOL dbm_satisfies(const raw_t *dbm, cindex_t dim,
                   cindex_t i, cindex_t j, raw_t constraint)
{
    assert(dbm && dim && i < dim && j < dim && i != j && dim > 0);
    return (BOOL)
        !(dbm[i*dim+j] > constraint &&             /* tightening ? */
          dbm_negRaw(constraint) >= dbm[j*dim+i]); /* too tight ? */
}


/** Check if a DBM is empty by looking
 * at its diagonal. There should be only == 0
 * constraints.
 * NOTE: one should never need to call this function
 * manually, unless for debugging.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre
 * - the close function was run before on the dbm
 * @return: TRUE if empty, FALSE otherwise.
 */
BOOL dbm_isEmpty(const raw_t *dbm, cindex_t dim);


/** Close operation. Complexity: cubic in dim.
 * Apply Floyd's shortest path algorithm.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @return TRUE if the DBM is non empty.
 * @post DBM is closed *if* non empty.
 * @pre if dim == 1 then *dbm==dbm_LE_ZERO: close has
 * not sense and will not work for dim == 1.
 */
BOOL dbm_close(raw_t *dbm, cindex_t dim);


/** Check that a DBM is closed. This test is as
 * expensive as dbm_close! It is there mainly for
 * testing/debugging purposes.
 * @param dbm: DBM to check.
 * @param dim: dimension.
 * @return TRUE if DBM is closed and non empty,
 * FALSE otherwise.
 */
BOOL dbm_isClosed(const raw_t *dbm, cindex_t dim);


/** Close operation. Complexity: custom*dim*dim,
 * where custom is the number of clocks to look at.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param touched: bit table that tells which clocks
 * to look at.
 * @pre
 * - touched is at least a uint32_t[bit2intsize(dim)]
 * - if there is no bit set (nothing to do) then
 *   the input DBM is non empty.
 * @return TRUE if the dbm is non empty.
 */
BOOL dbm_closex(raw_t *dbm, cindex_t dim, const uint32_t *touched);


/** Close operation for 1 clock. Complexity: dim*dim.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param k: the clock for which the closure applies.
 * @pre
 * - k < dim
 * @return TRUE if the DBM is non empty.
 */
BOOL dbm_close1(raw_t *dbm, cindex_t dim, cindex_t k);


/** Specialization of close valid if only one
 * constraint cij is tightened, ie, DBM is closed,
 * you tighten DBM[i,j] only, then this function
 * recomputes the closure more efficiently than
 * calling close1(i) and close1(j).
 * @param dbm: DBM
 * @param dim: dimension
 * @param i,j: the constraint that was tightened
 * @see Rokicki's thesis page 171.
 * @pre
 * - dim > 1 induced by i!=j & i < dim & j < dim
 * - i != j, i < dim, j < dim
 * - DBM non empty:
 *   DBM[i,j] + DBM[j,i] >= 0, ie, the tightening
 *   still results in a non empty DBM. If we have
 *   < 0 then this close should not be called
 *   at all because we know the DBM is empty.
 * @post DBM is not empty
 */
void dbm_closeij(raw_t *dbm, cindex_t dim, cindex_t i, cindex_t j);


/** Tighten with a constraint c and maintain the closed form.
 * @param dbm,dim DBM of dimension dim
 * @param i,j,c constraint c_ij to tighten
 * @pre c_ij < dbm[i,j] (it is a tightening) and -c_ij < dbm[j,i]
 * (it does not tighten too much, ie, to an empty DBM).
 * @post dbm is not empty.
 */
static inline
void dbm_tighten(raw_t* dbm, cindex_t dim, cindex_t i, cindex_t j, raw_t c)
{
    // it is a tightening and it does not tighten too much
    assert(dbm[i*dim+j] > c && dbm_negRaw(c) < dbm[j*dim+i]);
    dbm[i*dim+j] = c;
    dbm_closeij(dbm, dim, i, j);
    assert(!dbm_isEmpty(dbm, dim));
}


/** Check if a DBM is unbounded, i.e., if one point
 * can delay infinitely.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @return TRUE if unbounded, FALSE otherwise.
 * @pre
 * - dbm_isValid(dbm, dim)
 */
BOOL dbm_isUnbounded(const raw_t *dbm, cindex_t dim);


/** Relation between 2 dbms.
 * See relation_t.
 * @param dbm1,dbm2: DBMs to be tested.
 * @param dim: dimension of the DBMS.
 * @pre
 * - dbm1 and dbm2 have the same dimension
 * - dbm_isValid for both DBMs
 * @return: exact relation result, @see relation_t.
 */
relation_t dbm_relation(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim);


/** Test only if dbm1 <= dbm2.
 * @param dbm1,dbm2: DBMs to be tested.
 * @param dim: dimension of the DBMs.
 * @pre
 * - dbm1 and dbm2 have the same dimension
 * - dbm_isValid for both DBMs
 * @return TRUE if dbm1 <= dbm2, FALSE otherwise.
 */
BOOL dbm_isSubsetEq(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim);

/** Symmetric relation, just for completeness.
 * @return TRUE if dbm1 >= dbm2, FALSE otherwise.
 */
static inline
BOOL dbm_isSupersetEq(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim)
{
    return dbm_isSubsetEq(dbm2, dbm1, dim);
}


/** Relax upper bounds of a given clocks, ie, make them weak.
 * @param dbm, dim: DBM of dimension dim
 * @param clock: clock to relax.
 * @pre (dim > 0 induced by) clock >= 0 && clock < dim
 * && dbm_isValid(dbm,dim)
 * @post DBM is closed and non empty
 */
void dbm_relaxUpClock(raw_t *dbm, cindex_t dim, cindex_t clock);


/** Relax lower bounds of a given clocks, ie, make them weak.
 * @param dbm, dim: DBM of dimension dim
 * @param clock: clock to relax.
 * @pre (dim > 0 induced by) clock >= 0 && clock < dim
 * && dbm_isValid(dbm,dim)
 * @post DBM is closed and non empty
 */
void dbm_relaxDownClock(raw_t* dbm, cindex_t dim, cindex_t clock);


/** Relax all bounds so that they are non strict (except
 * for infinity).
 * @param dbm,dim: DBM of dimension dim.
 * @pre dbm_isValid(dbm, dim)
 * @post dbm_isValid(dbm, dim)
 */
void dbm_relaxAll(raw_t* dbm, cindex_t dim);


/** Smallest possible delay. Render upper bounds xi-x0 <= ci0
 * non strict if possible.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre
 * - dbm_isValid(dbm, dim)
 * @post DBM is closed and non empty (if dim > 0)
 */
static inline
void dbm_relaxUp(raw_t *dbm, cindex_t dim)
{
    // down of the ref clock == up of all other clocks
    dbm_relaxDownClock(dbm, dim, 0);
}


/** Smallest possible inverse delay. Render lower bounds x0-xi <= c0i
 * non strict if possible.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre
 * - dbm_isValid(dbm, dim)
 * @post DBM is closed and non empty (if dim > 0)
 */
static inline
void dbm_relaxDown(raw_t *dbm, cindex_t dim)
{
    // up of the ref clock == down of all other clocks
    dbm_relaxUpClock(dbm, dim, 0);
}


/** Constrain all lower bounds to be strict.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre dbm_isValid(dbm,dim)
 * @return TRUE if the DBM is non empty and closed, 0 otherwise.
 */
BOOL dbm_tightenDown(raw_t* dbm, cindex_t dim);

/** Constrain all upper bounds to be strict.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @pre dbm_isValid(dbm,dim)
 * @return TRUE if the DBM is non empty and closed, 0 otherwise.
 */
BOOL dbm_tightenUp(raw_t* dbm, cindex_t dim);

/** Copy DBMs.
 * @param src: source.
 * @param dst: destination.
 * @param dim: dimension.
 * @pre src and dst are raw_t[dim*dim]
 */
static inline
void dbm_copy(raw_t *dst, const raw_t *src, cindex_t dim)
{
    base_copyBest(dst, src, dim*dim);
}


/** Test equality.
 * @param dbm1,dbm2: DBMs to compare.
 * @param dim: dimension.
 * @pre dbm1 and dbm2 are raw_t[dim*dim]
 * @return TRUE if dbm1 == dbm2
 */
static inline
BOOL dbm_areEqual(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim)
{
    return base_areEqual(dbm1, dbm2, dim*dim);
}


/** Compute a hash value for a DBM.
 * @param dbm: input DBM.
 * @param dim: dimension.
 * @pre dbm is a raw_t[dim*dim]
 * @return hash value.
 */
static inline
uint32_t dbm_hash(const raw_t *dbm, cindex_t dim)
{
    return hash_computeI32(dbm, dim*dim, dim);
}


/** Test if a (discrete) point is included in the
 * zone represented by the DBM.
 * @param pt: the point
 * @param dbm: DBM
 * @param dim: dimension
 * @pre
 * - pt is a int32_t[dim]
 * - dbm is a raw_t[dim*dim]
 * - dbm is closed
 * @return TRUE if the pt satisfies the constraints of dbm
 */
BOOL dbm_isPointIncluded(const int32_t *pt, const raw_t *dbm, cindex_t dim);


/** Test if a (real) point is included in the
 * zone represented by the DBM.
 * @param pt: the point
 * @param dbm: DBM
 * @param dim: dimension
 * @pre
 * - pt is a int32_t[dim]
 * - dbm is a raw_t[dim*dim]
 * - dbm is closed
 * @return TRUE if the pt satisfies the constraints of dbm
 */
BOOL dbm_isRealPointIncluded(const double *pt, const raw_t *dbm, cindex_t dim);


/** Classical extrapolation based on maximal bounds,
 * formerly called k-normalization.
 *
 * Extrapolate the zone using maximal constants:
 * - if dbm[i,j] >  max_xi then set it to infinity
 * - if dbm[i,j] < -max_xj then set it to < -max_xj
 * - otherwise don't touch dbm[i,j]
 *
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param max: table of maximal constants to use for the clocks.
 * @pre
 * - DBM is closed and non empty
 * - max is a int32_t[dim]
 * - max[0] = 0 (reference clock)
 * @post DBM is closed
 */
void dbm_extrapolateMaxBounds(raw_t *dbm, cindex_t dim, const int32_t *max);


/** Diagonal extrapolation based on maximal bounds.
 *
 * Update dbm[i,j] with
 * - infinity if dbm[i,j] > max_xi
 * - infinity if dbm[0,i] < -max_xi
 * - infinity if dbm[0,j] < -max_xj, i != 0
 * - <-max_xj if dbm[i,j] < -max_xj, i == 0
 * - dbm[i,j] otherwise
 *
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param max: table of maximal constants.
 * @pre
 * - DBM is closed and non empty
 * - max is a int32_t[dim]
 * - max[0] = 0 (reference clock)
 * @post DBM is closed.
 */
void dbm_diagonalExtrapolateMaxBounds(raw_t *dbm, cindex_t dim, const int32_t *max);


/** Extrapolation based on lower-upper bounds.
 *
 * - if dbm[i,j] > lower_xi then dbm[i,j] = infinity
 * - if dbm[i,j] < -upper_xj then dbm[i,j] = < -upper_xj
 *
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param lower: lower bounds.
 * @param upper: upper bounds.
 * @pre
 * - DBM is closed
 * - lower and upper are int32_t[dim]
 * - lower[0] = upper[0] = 0 (reference clock)
 * @post DBM is closed.
 */
void dbm_extrapolateLUBounds(raw_t *dbm, cindex_t dim,
                             const int32_t *lower, const int32_t *upper);


/** Diagonal extrapolation based on lower-upper bounds.
 * Most general approximation.
 *
 * Update dbm[i,j] with
 * - infinity if dbm[i,j] > lower_xi
 * - infinity if dbm[0,i] < -lower_xi
 * - infinity if dbm[0,j] < -upper_xj for i != 0
 * - <-upper_xj if dbm[0,j] < -upper_xj for i = 0
 * - dbm[i,j] otherwise
 *
 * @param dbm: DBM.
 * @param dim: dimension.
 * @param lower: lower bounds.
 * @param upper: upper bounds.
 * @pre
 * - DBM is closed and non empty
 * - lower and upper are int32_t[dim]
 * - lower[0] = upper[0] = 0 (reference clock)
 * @post DBM is closed.
 */
void dbm_diagonalExtrapolateLUBounds(raw_t *dbm, cindex_t dim,
                                     const int32_t *lower, const int32_t *upper);


/** Shrink and expand a DBM:
 * - takes 2 bit arrays: the source array marks which
 * clocks are used in the source DBM, and the
 * destination array marks the clocks in the destination
 * DBM
 * - removes clock constraints in source and not
 *   in destination
 * - adds clock constraints not in source and in
 *   destination
 * - leaves clock constraints that are in source
 *   and in destination
 *
 * @param dbmSrc: source DBM
 * @param dimSrc: dimension of dbmSrc
 * @param dbmDst: destination DBM
 * @param bitSrc: source bit array
 * @param bitDst: destination bit array
 * @param bitSize: size in int of the bit arrays \a bitSrc and \a bitDst
 * @param table: where to write the indirection table for the
 * destination DBM, \a dbmDst. If bit \a i is set in the bitDst, then
 * table[i] gives the index used in the \a dbmDst.
 *
 * @return dimDst = dimension of dbmDst.
 * @pre let maxDim = bitSize * 32:
 * - dbmDst is at least a raw_t[resultDim*resultDim] where
 *   resultDim = base_countBitsN(bitDst, bitSize)
 * - dimSrc = number of bits set in bitSrc
 * - bitSrc and bitDst are uint32_t[bitSize]
 * - table is at least a uint32_t[maxDim]
 * - bitSize <= bits2intsize(maxDim)
 * - dimSrc > 0
 * - first bit in bitSrc is set
 * - first bit in bitDst is set
 * - bitSrc and bitDst are different (contents):
 *   the function is supposed to be called only
 *   if there is work to do
 * - dbmSrc != dbmDst (pointers): we do not
 *   modify the source DBM
 * - bitSize > 0 because at least ref clock
 * @post
 * - dimDst (returned) == number of bits in bitDst
 * - for all bits set in bitDst at positions i,
 *   then table[i] is defined and gives the proper
 *   indirection ; for other bits, table[i] is
 *   untouched.
 */
cindex_t dbm_shrinkExpand(const raw_t *dbmSrc,
                         raw_t *dbmDst,
                         cindex_t dimSrc,
                         const uint32_t *bitSrc,
                         const uint32_t *bitDst,
                         size_t bitSize,
                         cindex_t *table);


/** Variant of dbm_shrinkExpand: Instead of giving
 * bit arrays, you provide one array of the clocks
 * you want in the destination.
 * @param dbmDst, dimDst: destination DBM of dimension dimDst
 * @param dbmSrc, dimSrc: source DBM of dimension dimSrc
 * @param cols: indirection table to copy the DBM, ie,
 * rows (and columns) i of the destination come from
 * cols[i] in the source if ~cols[i] (ie != ~0).
 * @pre cols is a cindex_t[dimDst], cols[0] is ignored
 * since the ref clock is always at 0, and
 * for all i < dimDst: cols[i] < dimSrc.
 */
void dbm_updateDBM(raw_t *dbmDst, const raw_t *dbmSrc,
                   cindex_t dimDst, cindex_t dimSrc,
                   const cindex_t *cols);


/** Swap clocks.
 * @param dbm,dim: DBM of dimension dim.
 * @param x,y: clocks to swap.
 */
void dbm_swapClocks(raw_t *dbm, cindex_t dim, cindex_t x, cindex_t y);


/** Test if the diagonal is correct.
 * Constraints on the diagonal should be <0 if the
 * DBM is empty or <=0 for non empty DBMs. Positive
 * constraints are allowed in principle but such DBMs
 * are not canonical.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @return TRUE if the diagonal <=0.
 */
BOOL dbm_isDiagonalOK(const raw_t *dbm, cindex_t dim);


/** Test if
 * - dbm is closed
 * - dbm is not empty
 * - constraints in the 1st row are at most <=0
 */
BOOL dbm_isValid(const raw_t *dbm, cindex_t dim);


/** Convert code to string.
 * @param rel: relation result to translate.
 * @return string to print.
 * DO NOT deallocate or touch the result string.
 */
const char* dbm_relation2string(relation_t rel);


/** Go through a DBM (dim*dim) and
 * compute the max range needed to store
 * the constraints, excluding infinity.
 * @param dbm: DBM.
 * @param dim: dimension.
 * @return max range, positive value.
 */
raw_t dbm_getMaxRange(const raw_t *dbm, cindex_t dim);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DBM_DBM_H */

