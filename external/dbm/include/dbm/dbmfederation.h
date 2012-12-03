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
 * Filename : dbmfederation.h (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: dbmfederation.h,v 1.28 2005/07/22 12:55:54 adavid Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_DBM_DBMFEDERATION_H
#define INCLUDE_DBM_DBMFEDERATION_H

#include <stdio.h>
#include <stdlib.h>
#include "dbm/dbm.h"
#include "debug/monitor.h"

#ifdef __cplusplus
#include <iostream>
extern "C" {
#endif

/*
 * THIS API IS DEPRECATED AND NOT SUPPORTED ANY LONGER.
 */

/** DBM Federation type: such federations
 * have one dimension and several DBMs of
 * the same dimension. This is the list
 * of DBMs for the federation.
 */
typedef struct dbmlist_s
{
    struct dbmlist_s *next; /**< next DBM in the federation */
    raw_t dbm[];            /**< DBM of size dim*dim        */
} DBMList_t;


/** DBM Federation.
 */
typedef struct
{
    DBMList_t *dbmList;
    cindex_t dim;
} DBMFederation_t;


/** DBM factory: wrapper around an allocator.
 * The allocated DBM is of dimension = max
 * possible dimension.
 */
typedef struct
{
    DBMList_t *freeDBM; /**< list of deallocated DBMs */
    size_t sizeOfAlloc; /**< sizeof(DBMList_t)+maxDim*maxDim*sizeof(raw_t)
                            in the preconditions, we refer to factory->dim for simplicity */
    cindex_t maxDim;    /**< maximal dimension handled by this factory */
} DBMAllocator_t;


#define SIZEOF_DBMLIST(DIM) (sizeof(DBMList_t) + (DIM)*(DIM)*sizeof(raw_t))

/** Assertion to check that requested DBMs
 * have their dim <= max dim of the factory.
 */
#define dbmf_CHECKDIM(FACTORY,DIM) \
assert((FACTORY) && (FACTORY)->sizeOfAlloc >= SIZEOF_DBMLIST(DIM) && \
       (DIM) <= (FACTORY)->maxDim && SIZEOF_DBMLIST((FACTORY)->maxDim) == (FACTORY)->sizeOfAlloc)


/****************************************************
 ** Allocation/deallocation of DBMs with a factory **
 ****************************************************/


/** Initialize a factory.
 * @param factory: the factory to initialize.
 * @param dim: maximal dimension of used DBMs.
 */
static inline
void dbmf_initFactory(DBMAllocator_t *factory, cindex_t dim)
{
    assert(factory);
    factory->freeDBM = NULL;
    factory->sizeOfAlloc = SIZEOF_DBMLIST(dim);
    factory->maxDim = dim;
}


/** Check a factory -- for debugging.
 * May detect memory corruption.
 * @param factory: allocator to check.
 */
#ifndef NDEBUG
#define dbmf_checkFactory(F) assert(dbmf_internCheckFactory(F))
BOOL dbmf_internCheckFactory(const DBMAllocator_t *factory);
#else
#define dbmf_checkFactory(F)
#endif


/** Allocate DBMs with the factory.
 * @param factory
 * @return allocated DBMList_t or NULL if out-of-memory
 * @pre factory != NULL
 */
DBMList_t *dbmf_internAllocate(DBMAllocator_t *factory);


/** Give back ONE DBM to its factory.
 * @param factory: factory (= owner)
 * @param dbm: DBM to deallocate
 * @pre dbm was obtained from dbmf_allocate(factory)
 */
static inline
void dbmf_internDeallocate(DBMAllocator_t *factory, DBMList_t *dbm)
{
    assert(dbm && factory);
    dbm->next = factory->freeDBM;
    factory->freeDBM = dbm;
}

/** Give back a LIST of DBMs to its factory.
 * @param factory: factory (= owner)
 * @param dbmList: DBM list to deallocate
 * @pre dbm was obtained from dbmf_allocate(factory)
 */
void dbmf_internDeallocateList(DBMAllocator_t *factory, DBMList_t *dbmList);


#ifndef ENABLE_MONITOR
#define dbmf_allocate(F)         dbmf_internAllocate(F)
#define dbmf_deallocate(F,D)     dbmf_internDeallocate(F,D)
#define dbmf_deallocateList(F,L) dbmf_internDeallocateList(F,L)
#else
#define dbmf_allocate(F)\
debug_remember(DBMList_t*, dbmf_internAllocate(F))
#define dbmf_deallocate(F,D)\
do { debug_forget(D); dbmf_internDeallocate(F,D); } while(0)
#define dbmf_deallocateList(F,L)\
do { debug_push(); dbmf_internDeallocateList(F,L); } while(0)
#endif

/** Same as deallocateList but for a federation.
 * @pre federation not empty.
 * @param fed: federation to deallocate.
 * @param factory: allocator.
 */
static inline
void dbmf_deallocateFederation(DBMAllocator_t *factory, DBMFederation_t *fed)
{
    assert(factory && fed && fed->dim <= factory->maxDim);
    if (fed->dbmList)
    {
        dbmf_deallocateList(factory, fed->dbmList);
        fed->dbmList = NULL;
    }
}


/** Clear a factory.
 * @param factory: factory to clear (deallocate its freeDBM)
 */
void dbmf_clearFactory(DBMAllocator_t *factory);


/*************************************
 ** Basic functions for federations **
 *************************************/


/** Initialize a federation.
 * @param fed: federation
 * @param dim: dimension of the DBMs
 * @pre
 * - fed != NULL
 * - dim > 0 (at least reference clock)
 * - fed is a new uninitialized federation
 *   (the list of DBMs is reset)
 */
static inline
void dbmf_initFederation(DBMFederation_t *fed, cindex_t dim)
{
    assert(dim && fed);
    fed->dim = dim;
    fed->dbmList = NULL;
}


/** Add a DBM in the federation (at the beginning of the list).
 * @param dbm: dbm to add
 * @param fed: federation
 * @pre dbm->next is NULL or invalid: if there is a linked list
 * then it is lost.
 */
static inline
void dbmf_addDBM(DBMList_t *dbm, DBMFederation_t *fed)
{
    assert(dbm && fed);
    dbm->next = fed->dbmList;
    fed->dbmList = dbm;
}


/** Clean up the federation from empty DBMs.
 * Operations on federations assume that the
 * DBMs in the federation are not empty, so
 * if for some strange reason you have empty
 * DBMs there then remove them.
 * @param fed: federation to clean up.
 * @param factory: allocator for deallocation.
 * @pre factory->dim >= fed->dim
 */
void dbmf_cleanUp(DBMFederation_t *fed, DBMAllocator_t *factory);


/** Append a list to another.
 * @param dbmList: list argument
 * @param endList: list to append at the tail of dbmList
 * @pre dbmList != NULL
 */
static inline
void dbmf_appendList(DBMList_t *dbmList, DBMList_t *endList)
{
    assert(dbmList);
    if (endList)
    {
        while(dbmList->next) dbmList = dbmList->next;
        dbmList->next = endList;
    }
}


/** Test if a federation has a particular DBM.
 * Useful for testing. This is not an inclusion check,
 * only a DBM match function.
 * @param fed: federation.
 * @param dbm: DBM to test.
 * @return TRUE if dbm is in fed, FALSE otherwise.
 */
BOOL dbmf_hasDBM(const DBMList_t *fed, const raw_t *dbm, cindex_t dim);


/** Wrapper for DBMFederation_t
 * @see dbmf_hasDBM
 */
static inline
BOOL dbmf_federationHasDBM(const DBMFederation_t fed, const raw_t *dbm)
{
    return dbmf_hasDBM(fed.dbmList, dbm, fed.dim);
}


/** Size of a federation.
 * @param dbmList: list of DBMs (maybe NULL).
 * @return size of the list.
 */
size_t dbmf_getSize(const DBMList_t *dbmList);


/** Copy a federation.
 * @param src: source
 * @param dst: destination
 * @param factory: compatible factory
 * @return 0 if success, -1 if out-of-memory
 * @pre
 * - factory != NULL && factory->dim >= src->dim && factory->dim >= dst->dim
 * - all DBMs were obtained from factory
 * - dst is initialized or contains DBMs
 * @post
 * - order of DBMs is preserved
 * - if out-of-memory then dst is emptied
 */
int dbmf_copy(DBMFederation_t *dst, const DBMFederation_t src,
              DBMAllocator_t *factory);


/** Copy a DBM to a federation.
 * @param dbm: DBM to copy
 * @param dim: dimension of the DBM
 * @param factory: compatible factory
 * @pre factory != NULL && factory->dim >= dim && dim && dim > 0
 * @return federation with a copy of dbm
 * @post if out-of-memory then the federation is empty.
 */
DBMFederation_t dbmf_copyDBM(const raw_t *dbm, cindex_t dim, DBMAllocator_t *factory);


/** Copy a DBM to a DBMList_t
 * @param dbm: DBM to copy
 * @param dim: dimension of the DBM
 * @param factory: compatible factory
 * @pre factory != NULL && factory->dim >= dim && dim && dim > 0
 * @return a copy of dbm as a DBMList_t or NULL if out-of-memory
 */
DBMList_t* dbmf_copyDBM2List(const raw_t *dbm, cindex_t dim, DBMAllocator_t *factory);


/** Copy a list of DBMs.
 * @param dbmList: list to copy
 * @param dim: dimension of the DBMs
 * @param factory: compatible factory
 * @param valid: pointer for validation (memory allocation)
 * @return copy of dbmList.
 * @pre factory->maxDim >= dim, valid != NULL
 * @post order is respected, *valid = FALSE if out-of-memory
 * (and nothing is copied), TRUE if allocation successful.
 */
DBMList_t* dbmf_copyDBMList(const DBMList_t *dbmList, cindex_t dim,
                            BOOL *valid, DBMAllocator_t *factory);


/** Similar to dbmf_copyDBMList: wrapper for federations.
 * @param fed: original federation to copy.
 * @param factory: allocator.
 * @param valid: pointer for validation (memory allocation).
 * @pre factory->maxDim >= fed.dim, valid!=NULL.
 */
static inline
DBMFederation_t dbmf_copyFederation(const DBMFederation_t fed, BOOL *valid,
                                    DBMAllocator_t *factory)
{
    DBMFederation_t result;
    result.dim = fed.dim;
    result.dbmList = dbmf_copyDBMList(fed.dbmList, fed.dim, valid, factory);
    return result;
}


/** Copy a list of DBMs and append the result
 * to a federation.
 * @param fed: federation that collects the copy.
 * @param dbmList: the list of DBMs to copy.
 * @param factory: DBM allocator.
 * @pre factory->dim >= fed->dim, valid!=NULL, all DBMS
 * of dbmList are of dimension fed->dim.
 * @return -1 if out-of-memory, 0 otherwise.
 */
static inline
int dbmf_addCopy(DBMFederation_t *fed, const DBMList_t *dbmList,
                 DBMAllocator_t *factory)
{
    BOOL valid;
    DBMList_t *copyList;
    assert(fed && factory);
    copyList = dbmf_copyDBMList(dbmList, fed->dim, &valid, factory);
    if (!valid) return -1;
    if (!fed->dbmList)
    {
        fed->dbmList = copyList; /* initialize list */
    }
    else /* append list */
    {
        dbmf_appendList(fed->dbmList, copyList);
    }
    return 0;
}


/** Add to the federation 1 DBM that
 * contains only 0. @see dbm_zero.
 * @param fed: federation
 * @param factory: compatible factory
 * @return 0 if success, -1 if out-of-memory
 * @pre
 * - factory != NULL & factory->dim >= fed->dim
 * - fed is initialized or contains valid DBMs
 */
int dbmf_addZero(DBMFederation_t *fed, DBMAllocator_t *factory);


/** Add to the federation 1 DBM that
 * is not constrained (all clocks >= 0, that's all).
 * @param fed: federation
 * @param factory: compatible factory
 * @return 0 if success, -1 if out-of-memory
 * @pre factory != NULL && factory->dim >= fed->dim
 */
int dbmf_addInit(DBMFederation_t *fed, DBMAllocator_t *factory);


/** Apply the same operation to all DBMs of a federation.
 */
#define APPLY_TO_FEDERATION(FED, OPERATION) \
do {                                        \
    DBMList_t *_first = (FED).dbmList;      \
    while(_first)                           \
    {                                       \
        OPERATION;                          \
        _first = _first->next;              \
    }                                       \
} while(0)


/*******************************************
 ** DBM operations applied to federations **
 *******************************************/


/** Union of a list of DBMs and a federation: add DBMs to
 * the federation. fed += DBMs. Note: union means that
 * inclusion check is done.
 * @param fed: federation
 * @param dbmList: list of DBMs to add
 * @param factory: compatible factory
 * @return TRUE if one dbm was accepted, FALSE otherwise
 * @pre
 * - all DBMs were allocated by factory
 * - DBMs are of dimension fed->dim
 * - factory->dim >= fed->dim
 * - dbmList is a list, ie, dbmList->next is initialized.
 * @post
 * - DBMs in dbmList are deallocated or belong to fed.
 */
BOOL dbmf_union(DBMFederation_t *fed, DBMList_t *dbmList,
                DBMAllocator_t *factory);


/** Union of a list of DBMs and a federation: add DBMs to
 * the federation. fed += DBMs. The argument is copied this
 * time. Note: union means that inclusion check is done.
 * It is possible to encode this function with other
 * functions but useless intermediate copies may be produced.
 * @param fed: federation
 * @param dbmList: list of DBMs to add
 * @param factory: compatible factory
 * @param valid: return flag for the allocation.
 * @return TRUE if one dbm was accepted, FALSE otherwise
 * @pre
 * - all DBMs were allocated by factory
 * - DBMs are of dimension fed->dim
 * - factory->dim >= fed->dim
 * - dbmList is a list, ie, dbmList->next is initialized.
 * @post
 * - *valid = TRUE if the allocation succeeded, FALSE
 *   otherwise. If the allocation failed then fed has
 *   an incomplete result.
 */
BOOL dbmf_unionWithCopy(DBMFederation_t *fed, const DBMList_t *dbmList,
                        DBMAllocator_t *factory, BOOL *valid);


/** Computes the convex hull union between one DBM
 * and a list of DBMs: dbm = dbm + list of dbm
 * @param dbm: DBM
 * @param dim: dimension of all the DBMs
 * @param dbmList: list of DBMs
 * @pre
 * - all the DBMs are of dimension dim
 * - dbmList != NULL
 * - dbm != NULL
 */
void dbmf_convexUnion(raw_t *dbm, cindex_t dim, const DBMList_t *dbmList);


/** Computes the convex hull union between one DBM
 * and a federation: dbm = dbm + federation
 * @param dbm: DBM
 * @param fed: federation
 * @pre
 * - dbm is of dimension fed->dim
 */
static inline
void dbmf_convexUnionWithFederation(raw_t *dbm, const DBMFederation_t fed)
{
    assert(dbm);
    if (fed.dbmList)
    {
        dbmf_convexUnion(dbm, fed.dim, fed.dbmList);
    }
}


/** Computes the convex hull union from all the
 * DBMs of a federation.
 * @param fed: federation
 * @param factory: compatible factory
 * @pre factory != NULL & factory->dim >= fed->dim
 * @post the federation has only one DBM = union
 * of all previous DBMs.
 */
void dbmf_convexUnionWithSelf(DBMFederation_t *fed, DBMAllocator_t *factory);


/** Computes the intersection between one DBM
 * and a list of DBMs = dbm intersec list of dbm.
 * result = dbm intersected with dbmList
 * @param dbm: DBM
 * @param dim: dimension of all the DBMs
 * @param dbmList: list of DBMs
 * @param factory: compatible factory
 * @param valid: where to validate the result
 * @pre
 * - all the DBMs are of dimension dim
 * - dbmList != NULL
 * - dbm != NULL
 * - valid != NULL
 * @return the result of dbm intersec dbmList (may be NULL) and
 * set valid to FALSE if out-of-memory (and return NULL), TRUE otherwise
 */
DBMList_t* dbmf_intersection(const raw_t *dbm, cindex_t dim, const DBMList_t *dbmList,
                             BOOL* valid, DBMAllocator_t *factory);



/** Computes the intersection between 2 federations.
 * The arguments are preserved, the result is a newly allocated federation
 * (list of DBMs). Arguments are DBMList_t* for more flexibility.
 * @param fed1, fed2: federations (lists of DBMs) to compute intersection of
 * (may be both NULL).
 * @param dim: dimension of all the DBMs
 * @param valid: where to validate the result
 * @param factory: compatible factory
 * @pre
 * - all the DBMs are of dimension dim
 * - valid != NULL
 * @return the result as a newly allocated DBM list (maybe NULL if intersection is empty).
 */
DBMList_t* dbmf_intersectionWithFederation(const DBMList_t *fed1, const DBMList_t *fed2,
                                           cindex_t dim, BOOL *valid, DBMAllocator_t *factory);


/** Computes the intersection between one DBM
 * and a federation. result = updated fed = fed intersected with dbm.
 * @param fed: federation
 * @param dbm: DBM
 * @param factory: factory for deallocation of DBMs
 * @return fed->dbmList != NULL
 */
BOOL dbmf_intersectsDBM(DBMFederation_t *fed, const raw_t *dbm, DBMAllocator_t *factory);


/** Computes the intersection between one federation
 * and a federation. result = updated fed1 = fed1 intersected with fed2.
 * @param fed1,fed2: federations
 * @param factory: factory for deallocation of DBMs
 * @pre
 * - DBMs in fed2 have the same dimension as those in fed1.
 * - fed1 != NULL, fed2 may be NULL
 * - factory is a compatible factory
 * @return -1 if out-of-memory, 0 otherwise.
 */
int dbmf_intersectsFederation(DBMFederation_t *fed1, const DBMList_t *fed2, DBMAllocator_t *factory);


/** Test if a list of DBMs and a DBM have an intersection.
 * @param dbmList: list of DBMs.
 * @param dbm: DBM to test.
 * @param dim: dimension of all the DBMs.
 * @return TRUE if there is an intersection, FALSE otherwise.
 */
BOOL dbmf_DBMsHaveIntersection(const DBMList_t *dbmList, const raw_t *dbm, cindex_t dim);


/** Test if a federation and a DBM have an intersection.
 * @param fed: federation.
 * @param dbm: DBM to test.
 * @pre dbm is of dimension fed->dim.
 * @return TRUE if there is an intersection, FALSE otherwise.
 */
static inline
BOOL dbmf_federationHasIntersection(const DBMFederation_t fed, const raw_t *dbm)
{
    return dbmf_DBMsHaveIntersection(fed.dbmList, dbm, fed.dim);
}


/** Test if 2 federations have an intersection.
 * @param fed1,fed2: federations to test.
 * @pre the federations have the same dimension.
 */
static inline
BOOL dbmf_federationsHaveIntersection(const DBMFederation_t fed1, const DBMFederation_t fed2)
{
    assert(fed1.dim == fed2.dim);
    APPLY_TO_FEDERATION(fed1,
                        if (dbmf_federationHasIntersection(fed2, _first->dbm))
                        return TRUE);
    return FALSE;
}


/** Constrain a federation with several constraints.
 * @param fed: federation
 * @param constraints,n: the n constraints to use.
 * @param factory: compatible factory
 * @pre
 * - valid constraints: not of the form xi-xi <= something
 * - constraints[*].{i,j} < fed->dim
 * - factory != NULL & factory->dim >= fed->dim
 */
void dbmf_constrainN(DBMFederation_t *fed, const constraint_t *constraints, size_t n,
                     DBMAllocator_t *factory);


/** Constrain a federation with several constraints using an index
 * table to translate constraint indices to DBM indices.
 * @param fed: federation
 * @param indexTable: table for index translation
 * @param constraints,n: the n constraints to use.
 * @param factory: compatible factory
 * @pre
 * - valid constraints: not of the form xi-xi <= something
 * - indexTable[constraints[*].{i,j}] < fed->dim and no out-of-bound for indexTable
 * - factory != NULL & factory->dim >= fed->dim
 */
void dbmf_constrainIndexedN(DBMFederation_t *fed, const cindex_t *indexTable,
                            const constraint_t *constraints, size_t n,
                            DBMAllocator_t *factory);


/** Constrain a federation with one constraint
 * constraint1 once is cheaper than constrainN with 1
 * constraintN with n > 1 is cheaper than constrain1 n times
 * @param fed: federation
 * @param i,j,constraint: constraint for xi-xj to use
 * @param factory: compatible factory
 * @pre
 * - valid constraint: i != j & i < fed->dim & j < fed->dim
 * - factory != NULL & factory->dim >= fed->dim
 */
void dbmf_constrain1(DBMFederation_t *fed, cindex_t i, cindex_t j, raw_t constraint,
                     DBMAllocator_t *factory);


/** Up/delay operation (strongest post-condition).
 * @param fed: federation.
 */
void dbmf_up(DBMFederation_t fed);


/** Down operation (weakest pre-condition).
 * @param fed: federation.
 */
void dbmf_down(DBMFederation_t fed);


/** Reduce a federation: remove DBMs that are included
 * in other DBMs of the same federation.
 * Cost: 1/2 * dim^2 * (nb DBMs)^2
 * This performs simple inclusion checks between the DBMs.
 * @param fed: federation to reduce
 * @param factory: compatible factory
 * @pre factory != NULL & fed != NULL & factory->dim >= fed->dim
 */
void dbmf_reduce(DBMFederation_t *fed, DBMAllocator_t *factory);


/** Reduce a federation: remove DBMs that are redundant.
 * This computes substractions to determine if a DBM
 * should be kept or not.
 * This function is here only for experimental purposes and should
 * not be used in practice because of its horrible complexity:
 * O(dim^(4*size)). Memory consumption may explode just for the needs
 * of one "reduce" computation (and thus it is not a reduce anymore).
 * @param fed: federation to reduce
 * @param factory: compatible factory
 * @pre factory != NULL & fed != NULL & factory->dim >= fed->dim
 * @return 0 if internal allocations were successful, -1 otherwise.
 * If -1 is returned then there is no guarantee that fed is reduced.
 * The factory is reset if -1 is returned to try to free memory.
 */
int dbmf_expensiveReduce(DBMFederation_t *fed, DBMAllocator_t *factory);


/** Free operation: remove all constraints for a given clock.
 * @param fed: federation
 * @param clock: clock to free
 * @pre clock > 0 & clock < fed->dim
 */
void dbmf_freeClock(DBMFederation_t fed, cindex_t clock);


/** Update a clock value: x := value
 * @param fed: federation
 * @param clock: clock to update
 * @param value: new value
 * @pre clock > 0 & clock < fed->dim
 */
void dbmf_updateValue(DBMFederation_t fed, cindex_t clock, int32_t value);


/** Copy a clock: x := y
 * @param fed: federation
 * @param x: clock to copy to
 * @param y: clock to copy from
 * @pre x > 0, y > 0, x < fed->dim, y < fed->dim
 */
void dbmf_updateClock(DBMFederation_t fed, cindex_t x, cindex_t y);


/** Increment a clock: x += value.
 * @param fed: federation
 * @param clock: clock to increment
 * @param value: value to increment
 * @pre
 * - clock > 0 && clock < fed->dim
 * - if value < 0 then |value| <= min bound of clock
 */
void dbmf_updateIncrement(DBMFederation_t fed, cindex_t clock, int32_t value);


/** General update: x := x + value
 * @param fed: federation
 * @param x: clock to copy to
 * @param y: clock to copy from
 * @param value: value to increment
 * @pre
 * - x > 0, y > 0, x < fed->dim, y < fed->dim
 * - if value < 0 then |value| <= min bound of y
 */
void dbmf_update(DBMFederation_t fed, cindex_t x, cindex_t y, int32_t value);


/** Check if a federation satisfies a constraint.
 * @param fed: federation
 * @param i,j,constraint: constraint xi-xj to check
 * @return TRUE if there is one DBM that satisfies the constraint,
 * FALSE otherwise
 * @pre i != j, i < fed->dim, j < fed->dim
 */
BOOL dbmf_satisfies(const DBMFederation_t fed, cindex_t i, cindex_t j, raw_t constraint);


/** Test for emptiness.
 * @param fed: federation
 * @return TRUE if federation is empty, FALSE otherwise
 */
static inline
BOOL dbmf_isEmpty(const DBMFederation_t fed)
{
    return (BOOL)(fed.dbmList == NULL);
}


/** Test if a federation is unbounded, ie, if
 * one of its DBM is unbounded.
 * @param fed: federation to test
 * @return TRUE if one of its DBM is unbounded,
 * FALSE otherwise
 */
BOOL dbmf_isUnbounded(const DBMFederation_t fed);


/** Partial relation between one DBM and a federation:
 * apply the relation to all the DBMs of a federation.
 * This relation computation is partial in the sense that
 * it will miss inclusions that result from the (non-convex) union
 * of several DBMs, ie, the true semantics of a federation.
 * @param dbm: DBM to test
 * @param fed: federation
 * @pre dbm is of dimension fed->dim, dbm closed non empty
 * @return safe approximate relation:
 * dbm_SUBSET if dbm included in one DBM of fed
 * dbm_SUPERSET if dbm includes all DBMs of fed
 * dbm_EQUAL if subset & superset
 * dbm_DIFFERENT otherwise
 */
relation_t dbmf_partialRelation(const raw_t *dbm, const DBMFederation_t fed);


/** Relation between one DBM and a federation.
 * This computation is more expensive but it is correct
 * with respect to the semantics of a federation.
 * @param dbm: DBM to test
 * @param fed: federation
 * @param factory: compatible factory
 * @param valid: pointer to validate internal memory allocation.
 * @pre dbm is of dimension fed->dim, dbm closed non empty,
 * factory->dim >= fed.dim, valid != NULL
 * @return the relation between dbm and fed. The result is validated
 * by the flag *valid.
 * @post if *valid = FALSE then the relation cannot be computed,
 * dbm_DIFFERENT is returned, and the factory is reset.
 */
relation_t dbmf_relation(const raw_t *dbm, const DBMFederation_t fed,
                         BOOL *valid, DBMAllocator_t *factory);


/** Remove DBMs of fed that are (partially) included
 * in arg.
 * @return TRUE if fed is not empty afterwards, FALSE otherwise.
 * @param fed: federation to shrink
 * @param arg: list of DBM to test for inclusion
 * @param factory: the DBM allocator
 * @pre
 * - DBMs of arg are of dimension fed->dim
 * - fed!=NULL
 * - factory->maxDim>=fed->dim
 */
BOOL dbmf_removePartialIncluded(DBMFederation_t *fed, const DBMList_t *arg,
                                DBMAllocator_t *factory);


/** Test if a DBM is included in a list of DBMs.
 * @param dbm: dbm to test.
 * @param dbmList: list argument.
 * @param dim: dimension of all the DBMs.
 * @return TRUE if dbm is included in a DBM of dbmList, FALSE otherwise.
 */
BOOL dbmf_isIncludedIn(const raw_t *dbm, const DBMList_t *dbmList, cindex_t dim);


/** Test if a DBM is really included in a federation semantically.
 * This is MORE EXPENSIVE than the dbmf_isIncludedIn function.
 * @param dbm: dbm to test.
 * @param fed: federation to test.
 * @param valid: to tell if the result is valid (invalid = out of memory occured)
 * @param factory: the DBM allocator
 * @pre
 * - all DBMs of fed are of dimension fed.dim
 * - factory->maxDim >= fed.dim
 * @return TRUE if dbm is included in a DBM of dbmList, FALSE otherwise.
 */
static inline
BOOL dbmf_isReallyIncludedIn(const raw_t *dbm, const DBMFederation_t fed,
                             BOOL *valid, DBMAllocator_t *factory)
{
    return (BOOL)((dbmf_relation(dbm, fed, valid, factory) & base_SUBSET) != 0);
}


/** Test if all the DBMs of l1 are included in some DBMs of l2.
 * @param l1, l2: lists of DBMs to test.
 * @param dim: dimension of all the DBMs.
 * @return TRUE if forall DBMs d1 of l1, there exists d2 in l2 s.t. d1 included in d2.
 */
BOOL dbmf_areDBMsIncludedIn(const DBMList_t *l1, const DBMList_t *l2, cindex_t dim);


/** Test if all the DBMs of l1 are included semantically in l2.
 * @param l1, l2: lists of DBMs to test.
 * @param valid: to tell if the result is valid (invalid = out of memory occured)
 * @param factory: the DBM allocator
 * @pre
 * - all DBMs of fed are of dimension fed.dim
 * - factory->maxDim >= fed.dim
 * @return TRUE if forall DBMs d of l1, d included in federation l2.
 */
BOOL dbmf_areDBMsReallyIncludedIn(const DBMList_t *l1, const DBMFederation_t l2,
                                  BOOL *valid, DBMAllocator_t *factory);


/** Stretch-up operation for approximation.
 * Remove all upper bounds for all clocks.
 * @param fed: federation
 */
void dbmf_stretchUp(DBMFederation_t fed);


/** Stretch-down operation for approximation.
 * Remove all lower bounds for a given clock.
 * @param fed: federation
 * @param clock: clock to "down-free"
 * @pre clock > 0, clock < fed->dim
 */
void dbmf_stretchDown(DBMFederation_t fed, cindex_t clock);


/** Smallest possible delay. Render upper bounds xi-x0 <= ci0
 * non strict.
 * @param fed: federation
 */
void dbmf_microDelay(DBMFederation_t fed);


/** Test semantic equality. Expensive test, computes fed1-fed2
 * and fed2-fed1.
 * This is *very* expensive.
 * @param fed1, fed2: federations to test.
 * @param factory: compatible factory
 * @param valid: pointer to validate internal memory allocation.
 * @return TRUE if fed1 == fed2, FALSE otherwise.
 * @pre factory->dim >= fed1.dim && factory->dim >= fed2.dim, valid != NULL
 * @post if *valid == FALSE then the result cannot be trusted and the factory
 * is reset.
 */
BOOL dbmf_areEqual(const DBMFederation_t fed1, const DBMFederation_t fed2,
                   BOOL *valid, DBMAllocator_t *factory);


/** Check if a discrete point is included in a federation.
 * @param fed: federation
 * @param pt: point
 * @pre pt is a int32_t[fed->dim]
 * @return TRUE if pt satisfies the constraints of one
 * DBM in fed, FALSE otherwise
 */
BOOL dbmf_isPointIncluded(const int32_t *pt, const DBMFederation_t fed);


/** Check if a real point is included in a federation.
 * @param fed: federation
 * @param pt: point
 * @pre pt is a int32_t[fed->dim]
 * @return TRUE if pt satisfies the constraints of one
 * DBM in fed, FALSE otherwise
 */
BOOL dbmf_isRealPointIncluded(const double *pt, const DBMFederation_t fed);


/** Substraction operation: computes C = A - B
 * @param dbm: input DBM to substract from (A)
 * @param fed: federation to substract (B)
 * @param factory: compatible factory
 * @param valid: pointer to validate memory allocation
 * @return C as a list of DBMs (may be NULL if empty)
 * @pre
 * - dbm is of dimension fed.dim
 * - factory->dim >= fed.dim
 * - valid != NULL
 * @post
 * - the result (if != NULL) is allocated by the factory
 * - if *valid == FALSE then memory went out and the result =NULL
 */
DBMList_t* dbmf_substractFederationFromDBM(const raw_t *dbm, const DBMFederation_t fed,
                                           BOOL *valid, DBMAllocator_t *factory);


/** Substraction operation: computes C = A - B
 * @param dbm1: DBM to substract from (A)
 * @param dbm2: DBM to substract (B)
 * @param dim: dimension of the DBMs
 * @param factory: compatible factory
 * @param valid: pointer to validate memory allocation
 * @return C as a list of DBMs (may be NULL if empty)
 * @pre
 * - both DBMs have same dimension
 * - factory->dim >= dim
 * - valid != NULL
 * @post
 * - the result (if != NULL) is allocated by the factory
 * - the resulting federation may be non disjoint
 * - if *valid == FALSE then memory went out and the result =NULL
 */
DBMList_t* dbmf_substractDBMFromDBM(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim,
                                    BOOL *valid, DBMAllocator_t *factory);


/** Substraction operation: computes C = A - B
 * @param fed: input federation to substract from (A)
 * @param dbm: DBM to substract (B)
 * @param factory: compatible factory
 * @param valid: pointer to validate memory allocation
 * @return C
 * @pre
 * - dbm is of dimension fed.dim
 * - factory->dim >= fed.dim
 * - valid != NULL
 * @post
 * - the DBMList of the result (if != NULL) is allocated by the factory
 * - if *valid == FALSE then memory went out and result =NULL
 */
DBMFederation_t dbmf_substractDBMFromFederation(const DBMFederation_t fed, const raw_t *dbm,
                                                BOOL *valid, DBMAllocator_t *factory);


/** Substraction operation: computes C = A - B
 * @param fed1: input federation to substract from (A)
 * @param fed2: federation to substract (B)
 * @param factory: compatible factory
 * @param valid: pointer to validate memory allocation
 * @return C
 * @pre
 * - fed1.dim == fed2.dim
 * - factory->dim >= fed1.dim
 * - valid != NULL
 * @post
 * - the DBMList of the result (if != NULL) is allocated by the factory
 * - if *valid == FALSE then memory went out and the result =NULL
 */
DBMFederation_t dbmf_substractFederationFromFederation(const DBMFederation_t fed1,
                                                       const DBMFederation_t fed2,
                                                       BOOL *valid, DBMAllocator_t *factory);


/** Substraction operation: computes A = A - B
 * @param fed: input federation to substract from (A)
 * @param dbm: DBM to substract (B)
 * @param factory: compatible factory
 * @return A = A - B, 0 if allocation successful, -1 if
 * out-of-memory and the federation is empty.
 * @pre
 * - dbm is of dimension fed.dim
 * - factory->dim >= fed.dim
 */
int dbmf_substractDBM(DBMFederation_t *fed, const raw_t *dbm,
                      DBMAllocator_t *factory);


/** Substraction operation: computes A = A - B
 * @param fed1: input federation to substract from (A)
 * @param fed2: federation to substract (B)
 * @param factory: compatible factory
 * @return A = A - B (maybe NULL), 0 if allocation successful,
 * -1 if out-of-memory and the federation is empty.
 * @pre
 * - fed1->dim == dim of fed2 (if fed2 != NULL)
 * - factory->dim >= fed1->dim
 * @post fed1 is deallocated if out-of-memory
 */
int dbmf_substractFederation(DBMFederation_t *fed1, const DBMList_t *fed2,
                             DBMAllocator_t *factory);


/** Operation similar to down but constrained:
 * predecessors of fed avoiding the states that
 * could reach bad.
 * @param fed: federation to compute the predecessors
 * from.
 * @param bad: list of DBMs to avoid
 * @param factory: DBM allocator
 * @pre fed->dim <= factory->dim, all DBMs of bad are
 * of dimension fed->dim, fed!=NULL, factory!=NULL.
 * @post
 * - bad is deallocated
 * - fed contains all the (symbolic) states from which
 * you can reach the original fed while avoiding bad.
 * @return -1 if out-of-memory and fed is empty, 0 otherwise.
 * THIS IMPLEMENTATION IS KNOWN TO BE WRONG.
 * Please use the API from DBM v2.x. fed_t::predt is correct.
 */
int dbmf_predt(DBMFederation_t *fed, DBMFederation_t *bad, DBMAllocator_t *factory);



/** Similarly, on federations.
 * @param fed: federation
 * @param bitSrc: source bit array
 * @param bitDst: destination bit array
 * @param bitSize: size in int of the bit arrays
 * @param table: indirection table to write
 * @param factory: compatible factory
 * @pre
 * - factory->dim >= fed->dim & factory->dim >= resulting dim
 *   factory->dim == max possible dim
 * - @see dbm_shrinkExpand
 * @post
 * - fed->dim updated
 * - table written as by dbm_shrinkExpand
 */
void dbmf_shrinkExpand(DBMFederation_t *fed,
                       const uint32_t *bitSrc,
                       const uint32_t *bitDst,
                       size_t bitSize,
                       cindex_t *table,
                       DBMAllocator_t *factory);

/** Classical extrapolation based on maximal bounds,
 * formerly called k-normalization.
 * Note: you may want to call dbmf_reduce afterwards.
 * @see dbm_classicExtrapolateMaxBounds
 * @param fed: federation
 * @param max: table of maximal constants to use for the clocks.
 * - max is a int32_t[fed->dim]
 * - max[0] = 0 (reference clock)
 */
void dbmf_extrapolateMaxBounds(DBMFederation_t fed, const int32_t *max);


/** Diagonal extrapolation based on maximal bounds.
 * Note: you may want to call dbmf_reduce afterwards.
 * @see dbm_diagonalExtrapolateMaxBounds
 * @param fed: federation
 * @param max: table of maximal constants.
 * @pre
 * - fed->dim > 0
 * - max is a int32_t[fed->dim]
 * - max[0] = 0 (reference clock)
 */
void dbmf_diagonalExtrapolateMaxBounds(DBMFederation_t fed, const int32_t *max);


/** Extrapolation based on lower-upper bounds.
 * Note: you may want to call dbmf_reduce afterwards.
 * @see dbm_extrapolateLUBounds
 * @param fed: federation
 * @param lower: lower bounds.
 * @param upper: upper bounds.
 * @pre
 * - fed->dim > 0
 * - lower and upper are int32_t[fed->dim]
 * - lower[0] = upper[0] = 0 (reference clock)
 */
void dbmf_extrapolateLUBounds(DBMFederation_t fed, const int32_t *lower, const int32_t *upper);


/** Diagonal extrapolation based on lower-upper bounds.
 * Most general approximation.
 * Note: you may want to call dbmf_reduce afterwards.
 * @see dbm_diagonalExtrapolateLUBounds
 * @param fed: federation
 * @param lower: lower bounds.
 * @param upper: upper bounds.
 * @pre
 * - fed->dim > 0
 * - lower and upper are a int32_t[fed->dim]
 * - lower[0] = upper[0] = 0
 */
void dbmf_diagonalExtrapolateLUBounds(DBMFederation_t fed, const int32_t *lower, const int32_t *upper);



/******** moved here temporary print functions related to federations *************/


/** Pretty print of a federation.
 * @param dbmList: list of DBMs.
 * @param dim: dimension of all the DBMs.
 * @param out: output stream.
 */
void dbm_printFederation(FILE* out, const DBMList_t *dbmList, cindex_t dim);

#ifdef __cplusplus

/** Pretty print of a DBM federation.
 * @param dbmList: list of DBMs to print.
 * @param dim: dimension of all the DBMs.
 * @param out: output stream.
 */
std::ostream& dbm_cppPrintFederation(std::ostream& out, const DBMList_t *dbmList, cindex_t dim);

}
#endif

#endif /* INCLUDE_DBM_DBMFEDERATION_H */

