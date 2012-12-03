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

// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2005, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: priced.h,v 1.15 2005/05/31 20:54:59 behrmann Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DBM_PRICED_H
#define INCLUDE_DBM_PRICED_H

#include <cstdio>
#include <utility>
#include <iostream>
#include "base/inttypes.h"
#include "dbm/dbm.h"
#include "dbm/mingraph.h"

/**
 * @file 
 *
 * Functions for handling priced DBMs.
 *
 * A priced DBM is a data structure representing a zone with an affine
 * hyperplane over the zone assigning a cost to each point in the
 * zone.
 *
 * A few facts about priced DBMs:
 *
 * - Priced DBMs have a positive dimension. 
 *
 * - Priced DBMs are stored continously in memory. 
 *
 * - The clock with index zero is the reference clock. 
 *
 * - The affine hyperplane is given by the cost in the offset point
 *   and the coefficients of the hyperplane. 
 *
 * - A closed DBM is never empty.
 *
 * Allocation can be handled by the library by using \c
 * pdbm_allocate() and \c pdbm_deallocate(). This can optionally be
 * combined with reference counting using \c pdbm_incRef() and \c
 * pdbm_decRef(), in which case \c pdbm_deallocate() is called
 * automatically as soon as the reference count reaches zero.
 * Reference counting is used to implement copy on write
 * semantics. Thus whenever you modify a priced DBM with a reference
 * count larger than 1, then that DBM is automatically copied. For
 * this reason, all functions modifying priced DBMs take a reference
 * parameter to the priced DBM to modify.
 *
 * A NULL pointer is equivalent to an empty DBM with a reference count
 * of 1. Functions that can cause the priced DBM to become empty can
 * choose to deallocate the DBM rather than to copy it. All functions
 * that do not require the input DBM to be closed will allocate a new
 * DBM when given a NULL pointer as input.
 *
 * Alternatively, memory can be allocated outside the library. In that
 * case \c pdbm_size() bytes need to be allocated and preinitialized
 * for the use by the library by calling \c pdbm_reserve(). Priced
 * DBMs allocated in this manner must not be reference counted.
 */

/**
 * Data type for priced dbm. 
 */
typedef struct PDBM_s *PDBM;

/** 
 * Computes the size in number of bytes of a priced dbm of the given
 * dimension. 
 *
 * @param  dim is a dimension.
 * @pre    dim > 0
 * @return Amount of memory in bytes.
 */
size_t pdbm_size(cindex_t dim);

/** 
 * Reserves a memory area for use as a priced DBM. The priced DBM will
 * be unitialised, hence further initialised with e.g. \c pdbm_init()
 * or \c pdbm_zero() is required.
 * 
 * @param dim is the dimension of the priced DBM to reserve.
 * @param p   is a memory area of size \c pdbm_size(dim).
 * 
 * @return An unitialised priced DBM of dimension \a dim.
 */
PDBM pdbm_reserve(cindex_t dim, void *p);

/**
 * Allocates a new priced DBM. The reference count is initialised to
 * 0. No other initialisation is performed.
 *
 * @param  dim is the dimension.
 * @return A newly allocated priced DBM of dimension \a dim.
 * @pre    dim is larger than 0.
 */
PDBM pdbm_allocate(cindex_t dim);

/**
 * Deallocates a priced DBM.
 *
 * @pre
 * - \a pdbm was allocated with \c pdbm_allocate().
 * - The reference count of \a pdbm is zero.
 */
void pdbm_deallocate(PDBM &pdbm);

/** 
 * Increases reference count on priced DBM.
 *
 * @param pdbm is a priced DBM.
 * @pre   
 * \a pdbm has been allocated with \c pdbm_allocate() and is not NULL.
 */
inline static void pdbm_incRef(PDBM pdbm)
{
    assert(pdbm);

    (*(int32_t*)pdbm)++;
}

/**
 * Decreases reference count on priced DBM. The DBM is deallocated
 * when the reference count reaches zero.
 *
 * @param pdbm is a priced DBM.
 * @pre   \a pdbm has been allocated with \c pdbm_allocate().
 */
inline static void pdbm_decRef(PDBM pdbm)
{
    assert(pdbm == NULL || *(int32_t*)pdbm);

    /* The following relies on the reference counter being the first
     * field of the structure.
     */
    if (pdbm && !--*(int32_t*)pdbm)
    {
        pdbm_deallocate(pdbm);
    }
}

/** 
 * Copy a priced DBM. 
 *
 * Priced DBMs normally use a copy-on-write scheme, thus is no need to
 * call this function. The only use of it (besides internally in the
 * library as part of the copy-on-write implementation) is for priced
 * DBMs, for which you choose not to use reference counting. Hence, a
 * call to this function is only permissible if the reference count of
 * \a dst is zero. If \a dst is NULL, a new DBM is allocate with \c
 * pdbm_allocate().
 * 
 * @param  dst The destination.
 * @param  src The source.
 * @param  dim The dimension of \a dst and \a src.
 * @pre    The reference count of \a dst is zero or dst is NULL.
 * @post   The reference count of the return value is zero.
 * @return The destination.
 */
PDBM pdbm_copy(PDBM dst, const PDBM src, cindex_t dim);

/** 
 * Initialises a priced DBM to the DBM containing all valuations. The
 * cost of all valuations will be zero. If \a pdbm is NULL, a new DBM
 * is allocated with \c pdbm_allocate().
 *
 * @param pdbm is the priced DBM to initialize.
 * @param dim  is the dimension of \a pdbm.
 */
void pdbm_init(PDBM &pdbm, cindex_t dim);

/** 
 * Initialize a priced DBM to only contain the origin with a cost of
 * 0. If \a pdbm is NULL, a new DBM is allocated with \c
 * pdbm_allocate().
 *
 * @param pdbm is the priced DBM to initialise.
 * @param dim  is the dimension of \a pdbm.
 */
void pdbm_zero(PDBM &pdbm, cindex_t dim);


/** 
 * Constrain a priced DBM.
 *
 * After the call, the DBM will be constrained such that the
 * difference between clock \a i and clock \a j is smaller than \a
 * constraint.
 *
 * @see    dbm_constrain1
 * @param  pdbm       is a closed priced DBM of dimension \a dim.
 * @param  dim        is the dimension of \a pdbm.
 * @param  i          is a clock index.
 * @param  j          is a clock index. 
 * @param  constraint is a raw_t bound.
 * @pre    i != j
 * @post   The DBM is empty or closed.
 * @return True if and only if the result is not empty.
 */
bool pdbm_constrain1(PDBM &pdbm, cindex_t dim,
                     cindex_t i, cindex_t j, raw_t constraint);


/** 
 * Constrain a priced DBM with multiple constraints.
 *
 * @see    dbm_constraint1
 * @param  pdbm        is a closed priced DBM of dimension \a dim.
 * @param  dim         is the dimension of \a pdbm.
 * @param  constraints is an array of \a n constraints.
 * @param  n           is the length of \a constraints.
 * @pre    the constraints are valid 
 * @post   The DBM is empty or closed.
 * @return
 * - TRUE if the DBM is non empty +
 *   closed non empty DBM + updated consistent rates
 * - or FALSE if the DBM is empty +
 *   empty DBM + inconsistent rates
 */
bool pdbm_constrainN(PDBM &pdbm, cindex_t dim,
                     const constraint_t *constraints, size_t n);

/** 
 * Constrain a priced DBM to a facet (\a i, \a j).
 *
 * @param  pdbm        is a closed priced DBM of dimension \a dim.
 * @param  dim         is the dimension of \a pdbm.
 * @param  i           is a clock index
 * @param  j           is a clock index
 * @post   The DBM is empty or closed.
 * @return
 * TRUE if and only if the result is non empty.
 */
bool pdbm_constrainToFacet(PDBM &pdbm, cindex_t dim, cindex_t i, cindex_t j);


/** 
 * Relation between two priced DBMs.
 *
 * @see    relation_t
 * @param  pdbm1       is a closed priced DBMs of dimension \a dim.
 * @param  pdbm2       is a closed priced DBMs of dimension \a dim.
 * @param  dim         is the dimension of \a pdbm1 and \a pdbm2.
 * @return The relation between pdbm1 and pdbm2
 */
relation_t pdbm_relation(const PDBM pdbm1, const PDBM pdbm2, cindex_t dim);

/** 
 * Relation between 2 priced dbms where one is in compressed. Notice
 * that in constract to dbm_relationWithMinDBM, \a buffer may not be
 * NULL.
 *
 * @param  pdbm   is a closed priced DBM of dimension \a dim.
 * @param  dim    is the dimension of \a pdbm.
 * @param  minDBM is the compressed priced dbm.
 * @param  buffer is a buffer of size dim*dim.
 * @return The relation between pdbm1 and pdbm2
 * @see    dbm_relationWithMinDBM
 * @see    relation_t
 */
relation_t pdbm_relationWithMinDBM(
    const PDBM pdbm, cindex_t dim, const mingraph_t minDBM, raw_t *buffer);

/** 
 * Computes the infimum cost of the priced DBM.
 *
 * @param  pdbm is a closed priced DBM of dimension \a dim
 * @param  dim  is the dimension of \a pdbm.
 * @return The infimum cost of \a pdbm.
 */
int32_t pdbm_getInfimum(const PDBM pdbm, cindex_t dim);

/** 
 * Generates a valuation which has the infimum cost of the priced DBM.
 *
 * There is no guarantee that the valuation will be contained in the
 * priced DBM, but if it is not it is arbitrarily close to a valuation
 * that is contained in the priced DBM.
 *
 * A \a false entry in \a free indicates that the value of this clock
 * has already been set in \a valuation and that this clock must not
 * be modified. The function will only modify free clocks. 
 *
 * @param  pdbm      is a closed priced DBM of dimension \a dim
 * @param  dim       is the dimension of \a pdbm.
 * @param  valuation is an array of at least \a dim elements to which the
 *                   the valuation will be written.
 * @param  free      is an array of at least \a dim elements. 
 *
 * @return The cost of \a valuation.
 *
 * @throw out_of_range if no valuation with the given constraints can
 * be found.
 */
int32_t pdbm_getInfimumValuation(
    const PDBM pdbm, cindex_t dim, int32_t *valuation, const bool *free);

/** 
 * Check if a priced DBM satisfies a given constraint.
 *
 * @param pdbm       is a closed priced DBM of dimension \a dim.
 * @param dim        is the dimension of \a pdbm.
 * @param i,j        are theindices of clocks for the clock constraint.
 * @param constraint is the raw_t bound.
 * @return TRUE if the DBM satisfies the constraint.
 */
bool pdbm_satisfies(const PDBM pdbm, cindex_t dim,
                    cindex_t i, cindex_t j, raw_t constraint);

/** 
 * Returns true if the priced DBM is empty.
 *
 * @param pdbm is a closed or empty priced DBM of dimension \a dim.
 * @param dim  is the dimension of \a pdbm.
 * @return TRUE if and only if the priced dbm is empty.
 */
bool pdbm_isEmpty(const PDBM pdbm, cindex_t dim);

/** 
 * Check if at least one point can delay infinitely.
 *
 * @param  pdbm is a closed priced DBM of dimension \a dim.
 * @param  dim  is the dimension of \a pdbm.
 * @return TRUE if unbounded, FALSE otherwise.
 */
bool pdbm_isUnbounded(const PDBM pdbm, cindex_t dim);

/** 
 * Compute a hash value for a priced DBM.
 *
 * ISSUE: canonical form needed.
 *
 * @param  pdbm is a closed priced DBM of dimension \a dim.
 * @param  dim  is the dimension of \a pdbm.
 * @param  seed is a seed for the hash function.
 * @return hash value.
 */
uint32_t pdbm_hash(const PDBM pdbm, cindex_t dim, uint32_t seed);

/** 
 * Test if a point is included in the priced DBM.  
 *
 * @param  pdbm is a closed priced DBM of dimension \a dim.
 * @param  dim  is the dimension of \a pdbm.
 * @param  pt   is a clock valuation.
 * @return TRUE if \a pt satisfies the constraints of dbm.
 */
bool pdbm_containsInt(const PDBM pdbm, cindex_t dim, const int32_t *pt);

/** 
 * Test if a point is included in the priced DBM when strictness of
 * constraints are ignored.
 *
 * @param  pdbm is a closed priced DBM of dimension \a dim.
 * @param  dim  is the dimension of \a pdbm.
 * @param  pt   is a clock valuation.
 * @return TRUE if \a pt satisfies the constraints of dbm 
 *              (ignoring strictness)
 */
bool pdbm_containsIntWeakly(const PDBM pdbm, cindex_t dim, const int32_t *pt);

/** 
 * Test if a point is included in the priced DBM.  
 *
 * @param  pdbm is a closed priced DBM of dimension \a dim.
 * @param  dim  is the dimension of \a pdbm.
 * @param  pt   is a clock valuation.
 * @return TRUE if \a pt satisfies the constraints of dbm.
 */
bool pdbm_containsDouble(const PDBM pdbm, cindex_t dim, const double *pt);

/**
 * Delay with the current delay rate.
 *
 * @param pdbm is a closed priced dbm of dimension \a dim.
 * @param dim is the dimension of \a pdbm.
 * @see   pdbm_delayRate()
 * @post  The priced DBM is closed.
 */
void pdbm_up(PDBM &pdbm, cindex_t dim);

/**
 * Delay with delay rate \a rate. There must be at least one clock
 * forming a zero cycle with the reference clock.
 *
 * @param pdbm  is a closed priced dbm of dimension \a dim.
 * @param dim   is the dimension of \a pdbm.
 * @param rate  is the cost of delaying.
 * @param zero  is the index of a clock forming a zero cycle with the 
 *              reference clock. 
 * @post  The priced DBM is closed.
 */
void pdbm_upZero(PDBM &pdbm, cindex_t dim, uint32_t rate, cindex_t zero);

/**
 * Updates \a clock to \a value. This is only legitimate if the
 * current rate of \a clock is zero.
 *
 * @param pdbm  is a closed priced dbm of dimension \a dim.
 * @param dim   is the dimension of \a pdbm.
 * @param clock is the index of the clock to reset.
 * @param value is the value to which to set \a clock.
 * @pre   pdbm_getRate(pdbm, dim, clock) == 0
 * @post  The priced DBM is closed.
 */
void pdbm_updateValue(PDBM &pdbm, cindex_t dim, cindex_t clock, uint32_t value);

/**
 * Updates \a clock to \a value. This is only legitimate if the clock
 * forms a zero cycle with another clock (possibly the reference
 * clock).
 *
 * @param pdbm  is a closed priced dbm of dimension \a dim.
 * @param dim   is the dimension of \a pdbm.
 * @param clock is the index of the clock to reset.
 * @param value is the value to which to set \a clock.
 * @param zero  is a clock (possibly zero) forming a zero cycle 
 *              with \a clock. 
 * @post  The priced DBM is closed.
 * @post  pdbm_getRate(pdbm, dim, clock) == 0
 */
void pdbm_updateValueZero(PDBM &pdbm, cindex_t dim, 
                          cindex_t clock, uint32_t value, cindex_t zero);

/**
 * Unfinished extrapolation function.
 *
 * @see dbm_extrapolateMaxBounds
 */
void pdbm_extrapolateMaxBounds(
    PDBM &pdbm, cindex_t dim, int32_t *max);

/** 
 * Unfinished extrapolation function.
 * 
 * @see dbm_diagonalExtrapolateMaxBounds
 */
void pdbm_diagonalExtrapolateMaxBounds(
    PDBM &pdbm, cindex_t dim, int32_t *max);

/** 
 * Extrapolate a priced zone. The extrapolation is based on a lower
 * and an upper bound for each clock. The output zone simulates the
 * input zone.
 *
 * The implementation is not finished.
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm.
 * @param lower is an array of lower bounds for each clock.
 * @param upper is an array of upper bounds for each clock.
 * @pre   lower[0] = upper[0] = 0 
 * @post  The priced DBM is closed.
 * @see dbm_diagonalExtrapolateLUBounds
 */
void pdbm_diagonalExtrapolateLUBounds(
    PDBM &pdbm, cindex_t dim, int32_t *lower, int32_t *upper);

/**
 * Increments the cost of each point in a priced DBM by \a value.
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm.
 * @param value is the amount with which to increase the cost.
 * @post  The priced DBM is closed.
 * @pre   value >= 0
 */
void pdbm_incrementCost(PDBM &pdbm, cindex_t dim, int32_t value);

/**
 * Compute the closure of a priced DBM. This function is only relevant
 * if the DBM has been modified with \c pdbm_setBound().
 *
 * @param pdbm  is a priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm.
 * @post  The priced DBM is closed or empty.
 * @see   pdbm_setBound
 */
void pdbm_close(PDBM &pdbm, cindex_t dim);

/** 
 * Analyze a priced DBM for its minimal graph representation. Computes
 * the smallest number of constraints needed to represent the same
 * zone. 
 *
 * The result is returned as a bit matrix, where each bit indicates
 * whether that entry of the DBM is relevant. I.e. if the bit \f$ i
 * \cdot dim + j\f$ is set, then the constraint \f$(i,j)\f$ of \a dbm
 * is needed.
 *
 * @param  pdbm       is a closed priced DBM of dimension \a dim.
 * @param  dim        is the dimension of \a pdbm.
 * @param  bitMatrix  is bit matrix of size dim*dim
 * @return The number of bits marked one in \a bitMatrix.
 */
uint32_t pdbm_analyzeForMinDBM(
    const PDBM pdbm, cindex_t dim, uint32_t *bitMatrix);


/** 
 * Convert the DBM to a more compact representation.
 *
 * The API supports allocation of larger data structures than needed
 * for the actual zone representation. When the \a offset argument is
 * bigger than zero, \a offset extra integers are allocated and the
 * zone is written with the given offset. Thus when \c
 * int32_t[data_size] is needed to represent the reduced zone, an \c
 * int32_t array of size \c offset+data_size is allocated. The first
 * \a offset elements can be used by the caller. It is important to
 * notice that the other functions typically expect a pointer to the
 * actual zone data and not to the beginning of the allocated
 * block. Thus in the following piece of code, most functions expect
 * \c mg and not \c memory:
 *
 * \code 
 * int32_t *memory = dbm_writeToMinDBMWithOffset(...);
 * mingraph_t mg = &memory[offset]; 
 * \endcode 
 * 
 * @param  pdbm             is a closed priced DBM of dimension \a dim.
 * @param  dim              is the dimension of \a pdbm.
 * @param  minimizeGraph    when true, try to use minimal constraint form.
 * @param  tryConstraints16 when true, try to save constraints on 16 bits.
 * @param  c_alloc          is a C allocator wrapper.
 * @param  offset           is the offset for allocation.
 * @return The converted priced DBM. The first \a offset integers are unused.
 * @pre    allocFunction allocates memory in integer units
 */
int32_t *pdbm_writeToMinDBMWithOffset(
    const PDBM pdbm, cindex_t dim, bool minimizeGraph, bool tryConstraints16,
    allocator_t c_alloc, uint32_t offset);


/**
 * Uncompresses a compressed priced DBM. The compressed priced DBM \a
 * src is written to \a dst. The destination does not need to
 * initialised beforehand. The dimension \a dim must match the
 * dimension of the compressed priced DBM.
 *
 * @param dst  is a memory area of size pdbm_size(dim).
 * @param dim  is the dimension of the priced DBM.
 * @param src  is the compressed priced DBM.
 * @post  dst is a closed priced DBM.
 */
void pdbm_readFromMinDBM(PDBM &dst, cindex_t dim, mingraph_t src);

/**
 * Finds a clock that is on a zero cycle with \a clock. Returns TRUE
 * if and only if such a clock is found. If multiple clocks are on a
 * zero cycle with \a clock, then the clock with the smallest index is
 * returned.
 *
 * This function is equivalent to calling \c pdbm_findNextZeroCyle
 * with \a output initialised to zero.
 *
 * @param  pdbm  is a closed priced DBM of dimension \a dim.
 * @param  dim   is the dimension of \a pdbm.
 * @param  clock is the index of a clock.
 * @param  out   is where the clock found is written.
 * @return TRUE if and only if a clock is found.
 */
bool pdbm_findZeroCycle(const PDBM pdbm, cindex_t dim, cindex_t clock, cindex_t *out);

/**
 * Finds a clock that is on a zero cycle with \a clock. Returns TRUE
 * if and only if such a clock is found. Only clocks with a value
 * equal to or greater than the existing value of \a output are
 * considered. If multiple clocks are on a zero cycle with \a clock,
 * then the clock with the smallest index is returned.
 *
 * @param  pdbm  is a closed priced DBM of dimension \a dim.
 * @param  dim   is the dimension of \a pdbm.
 * @param  clock is the index of a clock.
 * @param  out   is where the clock found is written.
 * @return TRUE if and only if a clock is found.
 */
bool pdbm_findNextZeroCycle(const PDBM pdbm, cindex_t dim, cindex_t x, cindex_t *out);

/**
 * Returns the slope of the cost plane along the delay trajectory.
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 */
int32_t pdbm_getSlopeOfDelayTrajectory(const PDBM pdbm, cindex_t dim);

/**
 * Returns the rate (coefficient of the hyperplane) of \a clock.
 *
 * @param  pdbm  is a closed priced DBM of dimension \a dim.
 * @param  dim   is the dimension of \a pdbm. 
 * @param  clock is the clock for which to return the coefficient.
 * @return the rate of \a clock.
 */
int32_t pdbm_getRate(const PDBM pdbm, cindex_t dim, cindex_t clock);

const int32_t *pdbm_getRates(const PDBM pdbm, cindex_t dim);

/**
 * Returns the cost of the offset point.
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 */
uint32_t pdbm_getCostAtOffset(const PDBM pdbm, cindex_t dim);

/**
 * Sets the cost at the offset point. 
 *
 * Notice that the value must be large enough such that the infimum
 * cost of the priced DBM is not negative. Temporary inconsistencies
 * are allowed as long as no operations are performed on the priced
 * DBM.
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 * @param value is the new cost of the offset point.
 */
void pdbm_setCostAtOffset(PDBM &pdbm, cindex_t dim, uint32_t value);

/**
 * Returns true if the DBM is valid. Useful for debugging.
 *
 * @param pdbm  is a priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 */
bool pdbm_isValid(const PDBM pdbm, cindex_t dim);

/** 
 * Computes the lower facets of a priced DBM relative to \a clock.  As
 * a side-effect, all lower facets relative to \a clock are converted
 * to weak facets.
 *
 * @param  pdbm   is a closed priced DBM of dimension \a dim.
 * @param  dim    is the dimension of \a pdbm. 
 * @param  clock  is the clock for which to return the lower facets
 * @param  facets is an array of at least \a dim elements to which
 *                the lower facets will be written.
 * @return The number of facets written to \a facets.
 */
uint32_t pdbm_getLowerRelativeFacets(
    PDBM &pdbm, cindex_t dim, cindex_t clock, cindex_t *facets);

/** 
 * Computes the upper facets of a priced DBM relative to \a clock.  As
 * a side-effect, all upper facets relative to \a clock are converted
 * to weak facets.
 *
 * @param  pdbm   is a closed priced DBM of dimension \a dim.
 * @param  dim    is the dimension of \a pdbm. 
 * @param  clock  is the clock for which to return the upper facets
 * @param  facets is an array of at least \a dim elements to which
 *                the upper facets will be written.
 * @return The number of facets written to \a facets.
 */
uint32_t pdbm_getUpperRelativeFacets(
    PDBM &pdbm, cindex_t dim, cindex_t clock, cindex_t *facets);

/**
 * Computes the lower facets of a priced DBM.
 *
 * @param  pdbm   is a closed priced DBM of dimension \a dim.
 * @param  dim    is the dimension of \a pdbm. 
 * @param  facets is an array of at least \a dim elements to which the
 *                lower facets will be written.
 * @return The number of facets written to \a facets.
 */ 
uint32_t pdbm_getLowerFacets(PDBM &pdbm, cindex_t dim, cindex_t *facets);

/**
 * Computes the upper facets of a priced DBM.
 *
 * @param  pdbm   is a closed priced DBM of dimension \a dim.
 * @param  dim    is the dimension of \a pdbm. 
 * @param  facets is an array of at least \a dim elements to which the
 *                upper facets will be written.
 * @return The number of facets written to \a facets.
 */ 
uint32_t pdbm_getUpperFacets(PDBM &pdbm, cindex_t dim, cindex_t *facets);

/**
 * Computes the cost of a valuation in a priced DBM.
 *
 * @param  pdbm      is a closed priced DBM of dimension \a dim.
 * @param  dim       is the dimension of \a pdbm. 
 * @param  valuation is a valuation in \a pdbm.
 * @pre    pdbm_containsInt(pdbm, dim, valuation)
 * @return The cost of \a valuation in \a pdbm.
 */
int32_t pdbm_getCostOfValuation(const PDBM pdbm, cindex_t dim, const int32_t *valuation);

/**
 * Makes all strong constraints of a priced DBM weak. 
 *
 * @param pdbm      is a closed priced DBM of dimension \a dim.
 * @param dim       is the dimension of \a pdbm. 
 * @post  The priced DBM is closed.
 */
void pdbm_relax(PDBM &pdbm, cindex_t dim);

/**
 * Computes the offset point of a priced DBM.
 * 
 * @param pdbm      is a closed priced DBM of dimension \a dim.
 * @param dim       is the dimension of \a pdbm. 
 * @param valuation is an array of at least \a dim elements to which the
 *                  offset point is written.
 */
void pdbm_getOffset(const PDBM pdbm, cindex_t dim, int32_t *valuation);

/**
 * Sets a coefficient of the hyperplane of a priced DBM.
 * 
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 * @param clock is the index of a clock for which to set the coefficient.
 * @param rate  is the coefficient.
 */
void pdbm_setRate(PDBM &pdbm, cindex_t dim, cindex_t clock, int32_t rate);

/**
 * Returns the inner matrix of a priced DBM. The matrix can be
 * modified as long as \c pdbm_close() is called before any other
 * operations are performed on the priced DBM.
 * 
 * @param pdbm  is a priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 */
raw_t *pdbm_getMutableMatrix(PDBM &pdbm, cindex_t dim);

/**
 * Returns the inner matrix of a priced DBM. The matrix is read-only.
 * 
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 */
const raw_t *pdbm_getMatrix(const PDBM pdbm, cindex_t dim);

/**
 * Frees a clock of a priced DBM. 
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 * @param clock is the index of the clock to free.
 */
void pdbm_freeClock(PDBM &pdbm, cindex_t dim, cindex_t clock);


/**
 * Prints a priced DBM to a stream.
 *
 * @param f    is the stream to print to.
 * @param pdbm is a closed priced DBM of dimension \a dim.
 * @param dim  is the dimension of \a pdbm. 
 * @see dbm_print
 */
void pdbm_print(FILE *f, const PDBM pdbm, cindex_t dim);

/**
 * Prints a priced DBM to a stream.
 *
 * @param o    is the stream to print to.
 * @param pdbm is a closed priced DBM of dimension \a dim.
 * @param dim  is the dimension of \a pdbm. 
 * @see dbm_print
 */
void pdbm_print(std::ostream &o, const PDBM pdbm, cindex_t dim);

/**
 * Implementation of the free up operation for priced DBMs. 
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 * @param index is an index of a clock.
 * @see dbm_freeUp
 */
void pdbm_freeUp(PDBM &pdbm, cindex_t dim, cindex_t index);

/**
 * Implementation of the free down operation for priced DBMs. 
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 * @param index is an index of a clock. *
 * @see dbm_freeDown
 */
void pdbm_freeDown(PDBM &pdbm, cindex_t dim, cindex_t index);

/**
 * Checks whether a priced DBM is in normal form.
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 */
bool pdbm_hasNormalForm(PDBM pdbm, cindex_t dim);

/**
 * Brings a priced DBM into normal form.
 *
 * @param pdbm  is a closed priced DBM of dimension \a dim.
 * @param dim   is the dimension of \a pdbm. 
 */
void pdbm_normalise(PDBM pdbm, cindex_t dim);

///////////////////////////////////////////////////////////////////////////





#endif /* INCLUDE_DBM_PRICED_H */

