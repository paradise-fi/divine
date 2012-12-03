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
 * Filename : dbmfederation.c (dbm)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: dbmfederation.c,v 1.50 2005/07/22 12:55:54 adavid Exp $
 *
 *********************************************************************/

#include "base/intutils.h"
#include "base/bitstring.h"
#include "dbm/mingraph.h"
#include "dbm/dbmfederation.h"
#include "dbm/print.h"
#include "debug/macros.h"
#include "debug/malloc.h"
#include "dbm.h"

/* This implementation is known to have a bug
 * and it has been fixed in v2.0. This API is
 * not supported and this implementation will
 * not be fixed.
 */
#ifndef NMINIMAL_SUBSTRACT_SPLIT
#define NMINIMAL_SUBSTRACT_SPLIT
#endif

/* Configuration:
 * -DNTRY_SKIP_REDUCE
 * Disable test before substracting if the minimal
 * graph reduction can be skipped.
 *
 * -DNDISJOINT_DBM_SUBSTRACT
 * Disable the guarantee that the resulting DBMs from
 * DBM substractions are disjoint.
 *
 * -DNMINIMAL_SUBSTRACT_SPLIT
 * Disable the guarantee of minimal split.
 */


/* Notes on the test negRaw(x_ij) >= x_ji to determine
   if the zone is empty or not (i for x, j for y):

   Take
   1) c1 = <5, c2 = <-5, coded c1'=10, c2'=-10
   2) c1 = <5, c2 = <=-5, coded c1'=10, c2'=-9
   3) c1 = <=5, c2 = <-5, coded c1'=11, c2'=-10
   4) c1 = <=5, c2 = <=-5, coded c1'=11, c2'=-9

   Now we can interpret these differently as
   x-y ~ c1, y-x ~ c2 => -c1 ~ y-x ~ c2
   OR => -c2 ~ x-y ~ c1

   which gives:
   1) -5 < y-x < -5: empty
   2) -5 < y-x <= -5: empty
   3) -5 <= y-x < -5: empty
   4) -5 <= y-x <= -5: y-x==-5
   
   The test c1+c2 >= 0 we do to know if it is non empty is:
   c1'+c2' - (c1'|c2')&1 >= 1

   1) 10-10-0 == 0: empty
   2) 10-9-1 == 0: empty
   3) 11-10-1 == 0: empty
   4) 11-9-1 == 1: non empty

   Now if we come back to the 1st interpretation,
   we should have neg(c1 ~) >= c2 => empty
   or neg(c2 ~) >= c1 => empty
   where neg(c') = 1-c'

   1) -9 > -10, 11 > 10: empty
   2) -9 >= -9, 10 >= 10: empty
   3) -10 >= -10, 11 >= 11: empty
   4) -10 < -9, 10 < 11: non empty
   
   So we simplify the test to 1-c1' < c2' to check
   that the interval is non empty and avoid the addition.

   This fails if c1=-inf,c2=inf or c1=inf,c2=-inf.
   If c != +/- infinity, then test 1-c < whatever is OK.
   c == -infinity should never occur in legal DBMs.
*/
extern int allocatedhop;

/* Try to allocate from the free list first
 * and then with malloc.
 */
DBMList_t *dbmf_internAllocate(DBMAllocator_t *factory)
{
    assert(factory);

    if (factory->freeDBM)
    {
        DBMList_t *result = factory->freeDBM;
        factory->freeDBM = result->next;
        return result;
    }
    else
    {
        return (DBMList_t*) malloc(factory->sizeOfAlloc);
    }
}


/* - append the free list to the argument list
 * - update the free list
 */
void dbmf_internDeallocateList(DBMAllocator_t *factory, DBMList_t *dbmList)
{
#ifdef ENABLE_MONITOR
    DBMList_t *ptr;
    for(ptr = dbmList; ptr != NULL; ptr = ptr->next)
    {
        debug_pop();
        debug_forgetPtr(ptr);
    }
#endif

    assert(dbmList && factory);

    if (factory->freeDBM)
    {
        DBMList_t *last = dbmList;
        while(last->next) last = last->next;
        last->next = factory->freeDBM;
    }

    factory->freeDBM = dbmList;
}

#ifndef NDEBUG
BOOL dbmf_internCheckFactory(const DBMAllocator_t *factory)
{
    const DBMList_t *elem1, *elem2;
    uint32_t n, count;

    assert(factory);
    for(elem1 = factory->freeDBM, n = 0; elem1 != NULL; elem1 = elem1->next, ++n)
    {
        /* check for stupid pointers */
        assert((uintptr_t)elem1 > 0x00001000);
        /* check for loop */
        for(elem2 = factory->freeDBM, count = 0; elem2 != elem1; elem2 = elem2->next, ++count);
        if (count != n)
        {
            fprintf(stderr,
                    "Loop in DBMAllocator_t: same DBM (0x%d) at %d and %d!\n",
                    (uintptr_t)elem2, count, n);
            dbm_print(stderr, elem2->dbm, factory->maxDim);
            return FALSE;
        }
    }
    return TRUE;
}
#endif

/* free all DBMs in the free list, reset the free list
 */
void dbmf_clearFactory(DBMAllocator_t *factory)
{
    DBMList_t *dbml, *next;
    assert(factory);

    for(dbml = factory->freeDBM,
        factory->freeDBM = NULL; dbml != NULL; dbml = next)
    {
        next = dbml->next;
        free(dbml);
    }
}


/* try to find a match in the federation
 */
BOOL dbmf_hasDBM(const DBMList_t *fed, const raw_t *dbm, cindex_t dim)
{
    for( ; fed != NULL; fed = fed->next)
    {
        if (dbm_areEqual(fed->dbm, dbm, dim)) return TRUE;
    }
    return FALSE;
}


/* count number of DBMs
 */
size_t dbmf_getSize(const DBMList_t *dbmList)
{
    size_t size;
    
    for(size = 0 ; dbmList != NULL; ++size, dbmList = dbmList->next);

    return size;
}


/* go through the DBMs and remove empty DBMs
 */
void dbmf_cleanUp(DBMFederation_t *fed, DBMAllocator_t *factory)
{
    DBMList_t **ptr;
    assert(fed);
    dbmf_CHECKDIM(factory, fed->dim);

    ptr = &fed->dbmList;
    while(*ptr)
    {
        if (dbm_isEmpty((*ptr)->dbm, fed->dim))
        {
            DBMList_t *next = (*ptr)->next;
            dbmf_deallocate(factory, *ptr);
            *ptr = next;
        }
        else
        {
            ptr = &(*ptr)->next;
        }
    }
}


/* - if src is empty then deallocate dst if dst non empty
 * - if src non empty, copy all and deallocate remaining DBMs
 */
int dbmf_copy(DBMFederation_t *dst, const DBMFederation_t src,
              DBMAllocator_t *factory)
{
    DBMList_t *firstSrc, **atFirstDst;

    assert(dst);
    dbmf_CHECKDIM(factory, src.dim);
    dbmf_CHECKDIM(factory, dst->dim);

    dst->dim = src.dim;
    firstSrc = src.dbmList;
    atFirstDst = &dst->dbmList;

    if (firstSrc)
    {
        /* copy */
        do {
            if (!*atFirstDst)
            {
                *atFirstDst = dbmf_allocate(factory);
                if (!*atFirstDst)
                {
                    /* out-of-memory: clean-up and error */
                    if (dst->dbmList)
                    {
                        dbmf_deallocateList(factory, dst->dbmList);
                        dst->dbmList = NULL;
                    }
                    return -1;
                }
                (*atFirstDst)->next = NULL;
            }
            dbm_copy((*atFirstDst)->dbm, firstSrc->dbm, src.dim);
            firstSrc = firstSrc->next;
            atFirstDst = &(*atFirstDst)->next;
        } while(firstSrc);
    }

    /* deallocate remaining */
    if (*atFirstDst)
    {
        dbmf_deallocateList(factory, *atFirstDst);
        *atFirstDst = NULL;
    }

    return 0;
}


/* - set a new federation with a copy
 */
DBMFederation_t dbmf_copyDBM(const raw_t *dbm, cindex_t dim, DBMAllocator_t *factory)
{
    DBMFederation_t result;
    dbmf_CHECKDIM(factory, dim);

    result.dim = dim;
    result.dbmList = dbmf_copyDBM2List(dbm, dim, factory);

    return result;
}


/* - allocate new DBMList_t
 * - copy input
 */
DBMList_t* dbmf_copyDBM2List(const raw_t *dbm, cindex_t dim, DBMAllocator_t *factory)
{
    DBMList_t *result = dbmf_allocate(factory);
    dbmf_CHECKDIM(factory, dim);

    if (result)
    {
        result->next = NULL;
        dbm_copy(result->dbm, dbm, dim);
    }

    return result;
}


/** Internal function: another way to copy and to
 * use *valid instead of a null return code.
 * @param dbm,dim: DBM of dimension dim
 * @param valid: pointer to validate the memory allocation
 * @param factory: factory to allocate
 * @pre factory->dim >= dim, valid && *valid == TRUE
 */
static
DBMList_t* dbmf_validCopyDBM2List(const raw_t *dbm, cindex_t dim,
                                  BOOL *valid, DBMAllocator_t *factory)
{
    DBMList_t *result = dbmf_allocate(factory);

    assert(valid && *valid == TRUE);

    if (!result)
    {
        *valid = FALSE;
        return NULL;
    }

    result->next = NULL;
    dbm_copy(result->dbm, dbm, dim);

    return result;
}


/* straightforward copy of a list
 */
DBMList_t* dbmf_copyDBMList(const DBMList_t *dbmList, cindex_t dim,
                            BOOL *valid, DBMAllocator_t *factory)
{
    DBMList_t *result;
    DBMList_t **next = &result;
    dbmf_CHECKDIM(factory, dim);
    assert(valid);

    *valid = TRUE;
    for( ; dbmList != NULL; dbmList = dbmList->next)
    {
        DBMList_t *aCopy = dbmf_allocate(factory);
        *next = aCopy;
        if (!aCopy) /* out-of-memory */
        {
            if (result) dbmf_deallocateList(factory, result); /* clean-up */
            *valid = FALSE;
            return NULL;
        }
        next = &aCopy->next;
        dbm_copy(aCopy->dbm, dbmList->dbm, dim);
    }
    *next = NULL; /* terminate the list */

    return result;
}


/* Insert at the beginning of
 * the DBM list a zeroed DBM.
 */
int dbmf_addZero(DBMFederation_t *fed, DBMAllocator_t *factory)
{
    DBMList_t *newDBM;

    assert(fed);
    dbmf_CHECKDIM(factory, fed->dim);

    newDBM = dbmf_allocate(factory);
    if (newDBM)
    {
        dbmf_addDBM(newDBM, fed);
        dbm_zero(newDBM->dbm, fed->dim);
        return 0;
    }
    else
    {
        return -1;
    }
}


/* Insert at the beginning of
 * the DBM list an initialized DBM.
 */
int dbmf_addInit(DBMFederation_t *fed, DBMAllocator_t *factory)
{
    DBMList_t *newDBM;

    assert(fed);
    dbmf_CHECKDIM(factory, fed->dim);

    newDBM = dbmf_allocate(factory);
    if (newDBM)
    {
        dbmf_addDBM(newDBM, fed);
        dbm_init(newDBM->dbm, fed->dim);
        return 0;
    }
    else
    {
        return -1;
    }
}


/* Computes relation between dbm and all the DBMs in the
 * federation: may remove DBMs in the federation, may add
 * new dbm.
 */
BOOL dbmf_union(DBMFederation_t *fed, DBMList_t *dbmList,
                DBMAllocator_t *factory)
{
    if (fed->dbmList)
    {
        BOOL accepted = FALSE;
        DBMList_t *next;

        while(dbmList)
        {
            DBMList_t **atFirst = &fed->dbmList;
            do {
                switch(dbm_relation(dbmList->dbm, (*atFirst)->dbm, fed->dim))
                {
                    /* just continue */
                case base_DIFFERENT:
                    atFirst = &(*atFirst)->next;
                    break;
                    
                    /* remove dbm in federation */
                case base_SUPERSET:
                    next = (*atFirst)->next;
                    dbmf_deallocate(factory, *atFirst);
                    *atFirst = next;
                    break;
                    
                    /* reject new dbm */
                case base_SUBSET:
                case base_EQUAL:
                    next = dbmList->next;
                    dbmf_deallocate(factory, dbmList);
                    /* break is not enough */
                    goto continueLoopUnion_dbmList;
                }
            } while (*atFirst);

            /* accept current dbmList */
            next = dbmList->next;
            dbmf_addDBM(dbmList, fed);
            accepted = TRUE;

        continueLoopUnion_dbmList: /* next dbmList */
            dbmList = next;
        }

        return accepted;
    }
    else /* empty, just add dbm */
    {
        fed->dbmList = dbmList;
        return TRUE;
    }
}


/* similar to dbmf_union but copy the argument on-the-fly
 * when the DBMs are accepted.
 */
BOOL dbmf_unionWithCopy(DBMFederation_t *fed, const DBMList_t *dbmList,
                        DBMAllocator_t *factory, BOOL *valid)
{
    if (fed->dbmList)
    {
        BOOL accepted = FALSE;
        DBMList_t *next;
        DBMList_t *copyDBM;
        *valid = TRUE;

        while(dbmList)
        {
            DBMList_t **atFirst = &fed->dbmList;
            do {
                switch(dbm_relation(dbmList->dbm, (*atFirst)->dbm, fed->dim))
                {
                    /* just continue */
                case base_DIFFERENT:
                    atFirst = &(*atFirst)->next;
                    break;
                    
                    /* remove dbm in federation */
                case base_SUPERSET:
                    next = (*atFirst)->next;
                    dbmf_deallocate(factory, *atFirst);
                    *atFirst = next;
                    break;
                    
                    /* reject new dbm */
                case base_SUBSET:
                case base_EQUAL:
                    next = dbmList->next;
                    /* break is not enough */
                    goto continueLoopUnionCopy_dbmList;
                }
            } while (*atFirst);

            /* accept and copy current DBM */
            copyDBM = dbmf_copyDBM2List(dbmList->dbm, fed->dim, factory);
            if (!copyDBM)
            {
                *valid = FALSE;
                return accepted;
            }
            dbmf_addDBM(copyDBM, fed);
            next = dbmList->next;
            accepted = TRUE;

        continueLoopUnionCopy_dbmList: /* next dbmList */
            dbmList = next;
        }

        return accepted;
    }
    else /* empty, just add copy the DBMs */
    {
        fed->dbmList = dbmf_copyDBMList(dbmList, fed->dim, valid, factory);
        return TRUE;
    }
}



/* apply convex union with all the DBMs of the federation
 */
void dbmf_convexUnion(raw_t *dbm, cindex_t dim, const DBMList_t *dbmList)
{
    assert(dbm && dim && dbmList);
    do {
        dbm_convexUnion(dbm, dbmList->dbm, dim);
        dbmList = dbmList->next;
    } while(dbmList);
}


/* - compute union first DBM += all the next DBMs
 * - deallocate all the next DBMs
 */
void dbmf_convexUnionWithSelf(DBMFederation_t *fed, DBMAllocator_t *factory)
{
    DBMList_t *first;

    assert(fed && factory);

    first = fed->dbmList;
    
    if (first && first->next)
    {
        dbmf_convexUnion(first->dbm, fed->dim, first->next);
        dbmf_deallocateList(factory, first->next);
        first->next = NULL;
    }
}


/* - apply dbm_intersection on the DBMs of the federation
 * @see dbm_intersection
 */
DBMList_t* dbmf_intersection(const raw_t *dbm, cindex_t dim, const DBMList_t *dbmList,
                             BOOL* valid, DBMAllocator_t *factory)
{
    DBMList_t *result = NULL;
    DBMList_t *tmp = NULL;

    if (!dbmList) return NULL;

    assert(dbm && dim && valid);
    dbmf_CHECKDIM(factory, dim);

    *valid = TRUE;

    if (dim == 1) /* only ref clock */
    {
        result = dbmf_allocate(factory);
        if (!result)
        {
            *valid = FALSE;
            return NULL;
        }
        result->next = NULL;
        *result->dbm = dbm_LE_ZERO;
        return result;
    }

    /* other clocks than reference clock */
    for( ; dbmList != NULL; dbmList = dbmList->next)
    {
        if (!tmp)
        {
            tmp = dbmf_allocate(factory);
            if (!tmp) /* clean-up and error */
            {
                if (result) dbmf_deallocateList(factory, result);
                *valid = FALSE;
                return NULL;
            }
        }
        dbm_copy(tmp->dbm, dbm, dim);
        if (dbm_intersection(tmp->dbm, dbmList->dbm, dim))
        {
            /* add tmp to result */
            tmp->next = result;
            result = tmp;
            tmp = NULL;
        }
    }
    if (tmp) dbmf_deallocate(factory, tmp);

    return result;

    /* Note: we could add inclusion checking but it is
     * expensive and the case of an intersection included
     * in the intersection of several DBMs is unlikely.
     */
}


/* similar to dbmf_intersectsFederation but preserves all the arguments
 */
DBMList_t* dbmf_intersectionWithFederation(const DBMList_t *fed1, const DBMList_t *fed2,
                                           cindex_t dim, BOOL *valid, DBMAllocator_t *factory)
{
    dbmf_CHECKDIM(factory, dim);
    assert(valid);
    
    *valid = TRUE;

    // cases where intersection == NULL
    if (!fed1 || !fed2) return NULL;

    DBMList_t *result = NULL;

    do {
        DBMList_t *partial = dbmf_intersection(fed2->dbm, dim, fed1, valid, factory);

        if (!*valid)
        {
            if (result) dbmf_deallocateList(factory, result);
            return NULL;
        }

        if (partial)
        {
            dbmf_appendList(partial, result);
            result = partial;
        }

        fed2 = fed2->next;
    } while(fed2);

    return result;
}


/* Apply dbm_intersection to all DBMs:
 * if a DBM is empty, remove it from the federation.
 */
BOOL dbmf_intersectsDBM(DBMFederation_t *fed, const raw_t *dbm, DBMAllocator_t *factory)
{
    assert(fed && dbm);
    dbmf_CHECKDIM(factory, fed->dim);

    cindex_t dim = fed->dim;
    DBMList_t **dbmList = &fed->dbmList;

    while(*dbmList)
    {
        if (!dbm_intersection((*dbmList)->dbm, dbm, dim))
        {
            DBMList_t *next = (*dbmList)->next;
            dbmf_deallocate(factory, *dbmList);
            *dbmList = next;
        }
        else
        {
            dbmList = &(*dbmList)->next;
        }
    }

    return fed->dbmList != NULL;
}


/* Compute the union of fed1 intersected with DBMs of fed2
 * for all DBMs in fed2.
 * For the last union we do not need to copy.
 */
int dbmf_intersectsFederation(DBMFederation_t *fed1, const DBMList_t *fed2, DBMAllocator_t *factory)
{
    assert(fed1);
    dbmf_CHECKDIM(factory, fed1->dim);

    /* already empty : nothing to do */
    if (!fed1->dbmList) return 0;

    /* intersect with nothing : result is empty */
    if (!fed2)
    {
        dbmf_deallocateList(factory, fed1->dbmList);
        fed1->dbmList = NULL;
        return 0;
    }

    DBMList_t *result = NULL;
    cindex_t dim = fed1->dim;
    BOOL valid = TRUE;

    /* fed2 != NULL: condition, condition on fed2->next is not a typo */
    for( ; fed2->next != NULL; fed2 = fed2->next)
    {
        DBMList_t *partial = dbmf_intersection(fed2->dbm, dim, fed1->dbmList, &valid, factory);
        if (!valid)
        {
            // clean-up: ensure consistent state of fed1
            dbmf_deallocateList(factory, fed1->dbmList);
            if (result) dbmf_deallocateList(factory, result);
            fed1->dbmList = NULL;
            return -1;
        }

        if (partial)
        {
            dbmf_appendList(partial, result);
            result = partial;
        }
    }

    /* fed2 != NULL is the last one */
    if (dbmf_intersectsDBM(fed1, fed2->dbm, factory))
    {
        dbmf_appendList(fed1->dbmList, result);
    }
    else
    {
        fed1->dbmList = result;
    }

    return 0;
}


/* Test intersection with all the DBMs in the list.
 */
BOOL dbmf_DBMsHaveIntersection(const DBMList_t *dbmList, const raw_t *dbm, cindex_t dim)
{
    for( ; dbmList != NULL; dbmList = dbmList->next)
    {
        if (dbm_haveIntersection(dbmList->dbm, dbm, dim)) return TRUE;
    }
    return FALSE;
}


/* - apply dbm_constrainN to all DBMs
 * - remove those that are empty
 */
void dbmf_constrainN(DBMFederation_t *fed,
                     const constraint_t *constraints, size_t n,
                     DBMAllocator_t *factory)
{
    assert(fed && constraints);
    dbmf_CHECKDIM(factory, fed->dim);
        
    if (fed->dim > 1)
    {
        DBMList_t **atFirst;
        atFirst = &fed->dbmList;
        while(*atFirst)
        {
            /* if empty then remove from list */
            if (!dbm_constrainN((*atFirst)->dbm, fed->dim, constraints, n))
            {
                DBMList_t *next = (*atFirst)->next;
                dbmf_deallocate(factory, *atFirst);
                *atFirst = next;
            }
            else /* else go to next */
            {
                atFirst = &(*atFirst)->next;
            }
        }
    }
}


/* - apply dbm_constrainN to all DBMs using the index table
 * - remove those that are empty
 */
void dbmf_constrainIndexedN(DBMFederation_t *fed, const cindex_t *indexTable,
                            const constraint_t *constraints, size_t n,
                            DBMAllocator_t *factory)
{
    assert(fed && constraints && indexTable);
    dbmf_CHECKDIM(factory, fed->dim);
        
    if (fed->dim > 1)
    {
        DBMList_t **atFirst;
        atFirst = &fed->dbmList;
        while(*atFirst)
        {
            /* if empty then remove from list */
            if (!dbm_constrainIndexedN((*atFirst)->dbm, fed->dim, indexTable, constraints, n))
            {
                DBMList_t *next = (*atFirst)->next;
                dbmf_deallocate(factory, *atFirst);
                *atFirst = next;
            }
            else /* else go to next */
            {
                atFirst = &(*atFirst)->next;
            }
        }
    }
}


/* - apply dbm_constrain1 to all DBMs
 * - remove those that are empty
 */
void dbmf_constrain1(DBMFederation_t *fed,
                     cindex_t i, cindex_t j, raw_t constraint,
                     DBMAllocator_t *factory)
{
    DBMList_t **atFirst;

    assert(fed);
    dbmf_CHECKDIM(factory, fed->dim);

    atFirst = &fed->dbmList;
    while(*atFirst)
    {
        /* if empty then remove from list */
        if (!dbm_constrain1((*atFirst)->dbm, fed->dim, i, j, constraint))
        {
            DBMList_t *next = (*atFirst)->next;
            dbmf_deallocate(factory, *atFirst);
            *atFirst = next;
        }
        else /* else go to next */
        {
            atFirst = &(*atFirst)->next;
        }
    }
}


/* apply dbm_up to all the DBMs
 */
void dbmf_up(DBMFederation_t fed)
{
    APPLY_TO_FEDERATION(fed, dbm_up(_first->dbm, fed.dim));
}


/* apply dbm_down to all the DBMs
 */
void dbmf_down(DBMFederation_t fed)
{
    APPLY_TO_FEDERATION(fed, dbm_down(_first->dbm, fed.dim));
}


/* - first loop (on atFirst) on dbms from 0 to n-1
 *   - second loop (on atSecond) on dbms from 1 to n
 *     switch(relation(1st dbm, 2nd dbm))
 *     case 1st dbm != 2nd dbm: next second
 *     case 1st dbm > 2nd dbm: remove 2nd dbm
 *     case 1st dbm <= 2nd dbm: remove 1st dbm, abort 2nd loop
 */
void dbmf_reduce(DBMFederation_t *fed, DBMAllocator_t *factory)
{
    DBMList_t **atFirst, **atSecond;
    DBMList_t *next;
    cindex_t dim;
    const raw_t *dbm;

    assert(fed);
    dbmf_CHECKDIM(factory, fed->dim);
    
    dim = fed->dim;
    atFirst = &fed->dbmList;

    if (*atFirst && (*atFirst)->next)
    {
        if (dim == 1) /* only ref clock, keep only one */
        {
            dbmf_deallocateList(factory, (*atFirst)->next);
            (*atFirst)->next = NULL;
        }
        else
        {
            do {
                dbm = (*atFirst)->dbm;
                atSecond = &(*atFirst)->next;
                do {
                    switch(dbm_relation(dbm, (*atSecond)->dbm, dim))
                    {
                    case base_DIFFERENT:
                        atSecond = &(*atSecond)->next;
                        break;
                        
                    case base_SUPERSET:
                        next = (*atSecond)->next;
                        dbmf_deallocate(factory, *atSecond);
                        *atSecond = next;
                        break;
                        
                    case base_SUBSET:
                    case base_EQUAL:
                        next = (*atFirst)->next;
                        dbmf_deallocate(factory, *atFirst);
                        *atFirst = next;
                        goto reduce_AbortSecondLoop; /* break is not enough */
                    }
                } while(*atSecond);
                atFirst = &(*atFirst)->next;
            reduce_AbortSecondLoop: ;
            } while(*atFirst && (*atFirst)->next);
        }
    }
}


/* for all DBMi in fed do
 *   f = copy(DBMi)
 *   for all DBMj != DBMi do
 *     f -= DBMj
 *     if f is empty, remove DBMi, continue DBMi
 *   done
 * done
 */
int dbmf_expensiveReduce(DBMFederation_t *fed, DBMAllocator_t *factory)
{
    assert(fed);
    dbmf_CHECKDIM(factory, fed->dim);

    /* do something only if there are at least 2 DBMs in the federation
     */
    if (fed->dbmList && fed->dbmList->next)
    {
        cindex_t dim = fed->dim;
        if (dim == 1) /* only ref clock */
        {
            dbmf_deallocateList(factory, fed->dbmList->next);
            fed->dbmList->next = NULL;
        }
        else
        {
            DBMList_t **atdbmi = &fed->dbmList;
            do {
                DBMFederation_t f = dbmf_copyDBM((*atdbmi)->dbm, dim, factory);
                DBMList_t *dbmj = fed->dbmList;

                if (!f.dbmList) /* out-of-memory */
                {
                    dbmf_clearFactory(factory);
                    return -1;
                }
                
                /* very bad loop with recursive splits */
                do {
                    if (*atdbmi != dbmj)
                    {
                        if (dbmf_substractDBM(&f, dbmj->dbm, factory))
                        {
                            if (f.dbmList) dbmf_deallocateList(factory, f.dbmList);
                            dbmf_clearFactory(factory);
                            return -1;
                        }
                        if (!f.dbmList) /* if empty */
                        {
                            DBMList_t *next = (*atdbmi)->next;
                            dbmf_deallocate(factory, *atdbmi);
                            *atdbmi = next;
                            goto reduce_continueNextDBMi;
                        }
                    }
                    dbmj = dbmj->next;
                } while(dbmj);
                
                assert(f.dbmList);
                dbmf_deallocateList(factory, f.dbmList);
                atdbmi = &(*atdbmi)->next;
                
            reduce_continueNextDBMi: ;

            } while(*atdbmi);
        }
    }

    return 0;
}

/* apply dbm_freeClock to all the DBMs
 */
void dbmf_freeClock(DBMFederation_t fed, cindex_t clock)
{
    assert(clock && clock < fed.dim);
    APPLY_TO_FEDERATION(fed, dbm_freeClock(_first->dbm, fed.dim, clock));
}


/* apply dbm_updateValue to all the DBMs
 */
void dbmf_updateValue(DBMFederation_t fed, cindex_t clock, int32_t value)
{
    assert(clock && clock < fed.dim);
    APPLY_TO_FEDERATION(fed, dbm_updateValue(_first->dbm, fed.dim,
                                             clock, value));
}


/* apply dbm_updateClock to all the DBMs
 */
void dbmf_updateClock(DBMFederation_t fed, cindex_t x, cindex_t y)
{
    assert(x && y && x < fed.dim && y < fed.dim);
    APPLY_TO_FEDERATION(fed, dbm_updateClock(_first->dbm, fed.dim, x, y));
}


/* apply dbm_updateIncrement to all the DBMs
 */
void dbmf_updateIncrement(DBMFederation_t fed, cindex_t clock, int32_t value)
{
    assert(clock && clock < fed.dim);
    APPLY_TO_FEDERATION(fed, dbm_updateIncrement(_first->dbm, fed.dim, clock, value));
}


/* apply dbm_update to all the DBMs
 */
void dbmf_update(DBMFederation_t fed, cindex_t x, cindex_t y, int32_t value)
{
    assert(x && y && x < fed.dim && y < fed.dim);
    APPLY_TO_FEDERATION(fed, dbm_update(_first->dbm, fed.dim, x, y, value));
}


/* test for dbm_satisfy on all the DBMs, return TRUE for the 1st
 * DBM that satisfies the constraint
 */
BOOL dbmf_satisfies(const DBMFederation_t fed,
                    cindex_t i, cindex_t j, raw_t constraint)
{
    assert(i != j && i < fed.dim && j < fed.dim);
    APPLY_TO_FEDERATION(fed,
                        if (dbm_satisfies(_first->dbm, fed.dim, i, j, constraint))
                        return TRUE);
    return FALSE;
}


/* Test on all the DBMs, stop at the 1st unbounded DBM
 */
BOOL dbmf_isUnbounded(const DBMFederation_t fed)
{
    APPLY_TO_FEDERATION(fed,
                        if (dbm_isUnbounded(_first->dbm, fed.dim))
                        return TRUE);
    return FALSE;
}


/* Apply relation on all the DBMs, return conjunction of results
 */
relation_t dbmf_partialRelation(const raw_t *dbm, const DBMFederation_t fed)
{
    assert(dbm);

    if (fed.dbmList)
    {
        const DBMList_t *first = fed.dbmList;
        relation_t subset = 0;
        relation_t superset = base_SUPERSET;
        do {
            relation_t thisRel = dbm_relation(dbm, first->dbm, fed.dim);
            subset |= thisRel;
            superset &= thisRel;
            first = first->next;
        } while(first);

        return (relation_t)(subset|superset);
    }
    else
    {
        return base_SUPERSET;
    }
}


/* Test for base_SUBSET only.
 */
BOOL dbmf_isIncludedIn(const raw_t *dbm, const DBMList_t *dbmList, cindex_t dim)
{
    while(dbmList)
    {
        /* & base_SUBSET: test <= */
        if (dbm_relation(dbm, dbmList->dbm, dim) & base_SUBSET) return TRUE;
        dbmList = dbmList->next;
    }
    return FALSE;
}


/* check that all the dbms of l1 are included in l2
 */
BOOL dbmf_areDBMsIncludedIn(const DBMList_t *l1, const DBMList_t *l2, cindex_t dim)
{
    /* nothing will be included in l2 */
    while(l1)
    {
        if (!dbmf_isIncludedIn(l1->dbm, l2, dim)) return FALSE;
        l1 = l1->next;
    }
    return TRUE;
}


/* check that all the dbms of l1 are really included in l2
 */
BOOL dbmf_areDBMsReallyIncludedIn(const DBMList_t *l1, const DBMFederation_t l2,
                                  BOOL *valid, DBMAllocator_t *factory)
{
    /* nothing will be included in l2 */
    while(l1)
    {
        if (!dbmf_isReallyIncludedIn(l1->dbm, l2, valid, factory)) return FALSE;
        l1 = l1->next;
    }
    return TRUE;
}


/* Apply partial relation to the DBMs of fed and remove the included ones.
 */
BOOL dbmf_removePartialIncluded(DBMFederation_t *fed, const DBMList_t *arg,
                                DBMAllocator_t *factory)
{
    assert(fed && factory && fed->dim <= factory->maxDim);
    if (arg)
    {
        DBMList_t **atCurrent = &fed->dbmList;
        while(*atCurrent)
        {
            DBMList_t *current = *atCurrent;
            const DBMList_t *currentArg = arg;

            do {
                /* true if dbm_SUBSET or dbm_EQUAL */
                if (dbm_relation(current->dbm, currentArg->dbm, fed->dim) & base_SUBSET)
                {
                    DBMList_t *next = current->next;
                    dbmf_deallocate(factory, current);
                    *atCurrent = next;
                    break;
                }
                currentArg = currentArg->next;
            } while(currentArg);

            if (!currentArg) /* loop not broken */
            {
                atCurrent = &current->next;
            }
        }
    }
    return fed->dbmList != NULL;
}


/* r1 = dbm - fed
 * if r1 is empty and !isExact then return dbm <= fed
 * r2 = fed - dbm
 * if r1 and r2 are empty return dbm == fed
 * if r1 and r2 are not empty return dbm != fed
 * if r1 is empty and r2 is not empty return dbm < fed
 * if r1 is not empty and r1 is empty return dbm > fed
 */
relation_t dbmf_relation(const raw_t *dbm, const DBMFederation_t fed,
                         BOOL *valid, DBMAllocator_t *factory)
{
    assert(dbm && valid);
    dbmf_CHECKDIM(factory, fed.dim);

    *valid = TRUE;

    if (fed.dbmList)
    {
        DBMList_t *dbmIncluded;
        DBMFederation_t fedIncluded;

        if (fed.dim == 1) return base_EQUAL;

        /* dbm <= fed <-> dbm-fed = empty */
        dbmIncluded = dbmf_substractFederationFromDBM(dbm, fed, valid, factory);
        if (dbmIncluded) dbmf_deallocateList(factory, dbmIncluded);
        if (!*valid) return base_DIFFERENT;

        /* fed <= dbm <-> fed-dbm = empty */
        fedIncluded = dbmf_substractDBMFromFederation(fed, dbm, valid, factory);
        if (fedIncluded.dbmList) dbmf_deallocateList(factory, fedIncluded.dbmList);
        if (!*valid) return base_DIFFERENT;

        /* combine results */
        assert(base_SUBSET == 2 && base_SUPERSET == 1);
        return (relation_t)( /* subset   */ ((dbmIncluded == NULL) << 1) |
                             /* superset */ ((fedIncluded.dbmList == NULL)) );
    }
    else /* fed is empty */
    {
        return base_SUPERSET;
    }
}


/* apply dbm_stretchUp on all the DBMs
 */
void dbmf_stretchUp(DBMFederation_t fed)
{
    APPLY_TO_FEDERATION(fed, dbm_freeAllUp(_first->dbm, fed.dim));
}


/* apply dbm_strechDown on all the DBMs
 */
void dbmf_stretchDown(DBMFederation_t fed, cindex_t clock)
{
    assert(clock && clock < fed.dim);
    APPLY_TO_FEDERATION(fed, dbm_freeDown(_first->dbm, fed.dim, clock));
}


/* apply dbm_microDelay on all the DBMs
 */
void dbmf_microDelay(DBMFederation_t fed)
{
    APPLY_TO_FEDERATION(fed, dbm_relaxUp(_first->dbm, fed.dim));
}


/* - trivial case for pointer
 * - if different dimension, not equal
 * - compute r1=fed1-fed2 and r2=fed2-fed1
 *   return r1 is empty and r2 is empty
 */
BOOL dbmf_areEqual(const DBMFederation_t fed1, const DBMFederation_t fed2,
                   BOOL *valid, DBMAllocator_t *factory)
{
    dbmf_CHECKDIM(factory, fed1.dim);
    dbmf_CHECKDIM(factory, fed2.dim);
    assert(valid);

    *valid = TRUE;

    if (fed1.dbmList == fed2.dbmList)
    {
        return TRUE;
    }
    else if (fed1.dim != fed2.dim)
    {
        return FALSE;
    }
    else if (fed1.dim == 1)
    {
        return TRUE;
    }
    else
    {
        DBMFederation_t r = dbmf_substractFederationFromFederation(fed1, fed2, valid, factory);
        if (r.dbmList) /* fed1-fed2 != empty */
        {
            dbmf_deallocateList(factory, r.dbmList);
            return FALSE;
        }
        if (!*valid) return FALSE;
        r = dbmf_substractFederationFromFederation(fed2, fed1, valid, factory);
        if (r.dbmList) /* fed2-fed1 != empty */
        {
            dbmf_deallocateList(factory, r.dbmList);
            return FALSE;
        }
        return TRUE;
    }
}


/* try dbm_isPointIncluded on all the DBM, stop when
 * one inclusion is found.
 */
BOOL dbmf_isPointIncluded(const int32_t *pt, const DBMFederation_t fed)
{
    assert(pt);
    APPLY_TO_FEDERATION(fed,
                        if (dbm_isPointIncluded(pt, _first->dbm, fed.dim))
                        return TRUE);
    return FALSE;
}


/* try dbm_isRealPointIncluded on all the DBM, stop when
 * one inclusion is found.
 */
BOOL dbmf_isRealPointIncluded(const double *pt, const DBMFederation_t fed)
{
    assert(pt);
    APPLY_TO_FEDERATION(fed,
                        if (dbm_isRealPointIncluded(pt, _first->dbm, fed.dim))
                        return TRUE);
    return FALSE;
}


/* "quick" test (# of constraints) to see if
 * the substraction substract anything.
 * Similar to _needsToSubstractDBM, but reads
 * the constraints from a minimal graph.
 * @param dbm1,dbm2,dim: DBMs of dimension dim to check
 * (if dbm1-dbm2 != dbm1)
 * @param nbConstraints, bitMatrix: minimal graph with nbConstraints
 * number of constraints
 * @pre dim > 1, nbConstraints > 0
 * @return TRUE if dbm1-dbm2==dbm1, FALSE otherwise
 */
static
BOOL dbmf_isSubstractEqualToIdentity(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim,
                                     const uint32_t *bitMatrix, size_t nbConstraints)
{
    size_t dimdim = dim*dim;
    const raw_t *end = dbm1 + dimdim;
    assert(dbm1 && dbm2 && bitMatrix && dim > 1 && nbConstraints);

    /* go through dbm1[j,i] and dbm2[i,j] */
    for(;;)
    {
        uint32_t b, count;

        for(b = *bitMatrix++, count = 32; b != 0; --count, b >>= 1)
        {
            /* b != 0 => the loop will terminate */
            for( ; (b & 1) == 0; --count, b >>= 1)
            {
                /* next */
                dbm2++;      /* dbm2[i,j] */
                dbm1 += dim; /* dbm1[j,i] */
                assert(count);
            }
            while(dbm1 >= end)
            {
                dbm1 = dbm1 - dimdim + 1; /* +1: next column */
            }

            /* no infinity in the minimal graph */
            assert(*dbm2 != dbm_LS_INFINITY);
            if (dbm_negRaw(*dbm2) >= *dbm1) /* then no effect */
            {
                return TRUE;
            }
            /* see if constraints left to read */
            assert(nbConstraints);
            if (!--nbConstraints)
            {
                return FALSE;
            }
            
            /* next */
            dbm2++;      /* dbm2[i,j] */
            dbm1 += dim; /* dbm1[j,i] */

            assert(count);
        }

        /* jump unread bits */
        dbm2 += count;
        dbm1 += count*dim;
    }
}

#ifndef NMINIMAL_SUBSTRACT_SPLIT

/** Evaluate if a facette i,j (delimited by corner points)
 * of dbm2 intersects dbm1.
 * Algorithm for substraction, given that it is != identity:
 * - remove constraint dbm2[i,j] if it does not tighten dbm1[i,j]
 * - remove constraint dbm2[i,j] if facette dbm2[i,j] does not
 *   intersect dbm1.
 * - result = union of DBMs with dbm1[j,i] constrained with -dbm2[i,j]
 *
 * Facette detection algorithm: projects dbm2[j,i] to
 *   dbm2[i,j], and compute on-the-fly closure. Check for the updated
 *   constraints that there is an intersection or not by checking
 *   dbm2[i,j]' < dbm1[j,i] (one false->no intersection).
 *   tighten: dbm2[j,i]'=-dbm2[i,j]
 *   dbm2[k,i]'=dbm2[k,j]+dbm2[j,i]'
 *   dbm2[j,k]'=dbm2[j,i]'+dbm2[i,k]
 *   check that -dbm2[k,i]' < dbm1[i,k] and
 *   -dbm2[j,k]' < dbm1[k,j] (on-the-fly) for
 *   k!=i,j
 *
 * @param dbm1,dbm2,dim: DBMs of dimension dim
 * @param idim: i*dim, when inlined suppress a multiplication
 * (we already use this from the caller, so reuse it here)
 * @param i,j: facette
 * @return TRUE if facette i,j of dbm2 intersects dbm1, FALSE
 * otherwise
 */
static inline
BOOL dbmf_facetteIntersects(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim,
                            size_t idim, cindex_t i, cindex_t j)
{
#define SUBLOW(A,T) (low-(A) + ((T)&((A)|low)))
#define TESTIN(IDX1, IDX2) (dbm_addRawRaw(dbm1[IDX1],dbm2[IDX2]) > dbm_LE_ZERO)

    raw_t low = dbm2[idim+j];
    cindex_t k = 0;
    size_t kdim = 0;
    size_t jdim = j*dim;
    assert(dim > 1 && dbm2[idim+j] != dbm_LS_INFINITY);
    do {
        if (k != i && k != j
            &&
            ((dbm2[kdim+j] != dbm_LS_INFINITY &&
              SUBLOW(dbm2[kdim+j],TESTIN(idim+k,kdim+i)) >= dbm1[idim+k])
             ||
             (dbm2[idim+k] != dbm_LS_INFINITY &&
              SUBLOW(dbm2[idim+k],TESTIN(kdim+j,jdim+k)) >= dbm1[kdim+j])))
        {
            return FALSE;
        }
        kdim += dim;
    } while(++k < dim);
    
    return TRUE;
}

#endif /* NMINIMAL_SUBSTRACT_SPLIT */


/* Use the minimal graph representation to reduce
 * the number of split DBMs to the minimum. The
 * downside is: we may compute the minimal graph for
 * nothing.
 * @param dbm1,dbm2,dim: DBMs of dimension dim to compute
 * dbm1-dbm2
 * @param factory: compatible factory
 * @pre factory->dim >= dim &&
 * !_isSubstractEqualToIdentity(dbm1, dbm2, dim, bits, nbConstraints)
 * or
 * _needsMinimalSubstract, depending on compilation
 * @pre if compiling with minimal split then
 * bits is the result of _needsMinimalSubstract,
 * valid != NULL and *valid == TRUE
 * @post
 * - the result is not NULL if used on a non empty "minimal split graph"
 */
static
DBMList_t* dbmf_internSubstractDBMFromDBM(const raw_t *dbm1, const raw_t *dbm2,
                                          cindex_t dim,
                                          BOOL *valid,
                                          DBMAllocator_t *factory,
                                          const uint32_t *bits,
                                          size_t nbConstraints)
{
    DBMList_t *result = NULL;
    cindex_t i = 0, j = 0;
    size_t idim = 0;

#ifndef NDISJOINT_DBM_SUBSTRACT
    DBMList_t *remainder = NULL;
    if (nbConstraints >= 2) /* at least 2 constraints to make it useful */
    {
        remainder = dbmf_allocate(factory);
        if (!remainder) /* out-of-memory */
        {
            *valid = FALSE;
            return NULL;
        }
        dbm_copy(remainder->dbm, dbm1, dim);
        dbm1 = remainder->dbm;
    }
#endif

    assert(dim > 1); /* otherwise nothing to do */
    assert(nbConstraints && bits && dbm1 && dbm2);
    dbmf_CHECKDIM(factory, dim);
    assert(!dbmf_isSubstractEqualToIdentity(dbm1, dbm2, dim,
                                            bits, nbConstraints));
    assert(valid && *valid == TRUE);

    for(;;)
    {
        uint32_t b, count;

        for(b = *bits++, count = 32; b != 0; --count, b >>= 1)
        {
            /* b != 0 => the loop will terminate
             * look for one bit set */
            for( ; (b & 1) == 0; --count, b >>= 1)
            {
                /* next */
                ++j;
                assert(count);
            }
            /* correct j */
            while(j >= dim)
            {
                j -= dim;
                i++;
                idim += dim;
            }
            
            assert(i != j && i < dim && j < dim);
            assert(dbm2[idim+j] != dbm_LS_INFINITY); /* no infinity in minimal graph */
#ifdef NDISJOINT_DBM_SUBSTRACT
            assert(dbm_negRaw(dbm2[idim+j]) < dbm1[j*dim+i]); /* it is a tightening */
#endif
            /* if tightening dbm1[j,i] with neg(dbm2[i,j]) is not empty:
             *
             * if (dbm_addRawFinite(dbm1[i*dim+j], negConstraint) >= dbm_LE_ZERO)
             *
             * which we can simplify to:
             *
             * 1-negConstraint < dbm1[i,j] == 1-(1-dbm2[i,j]) < dbm1[i,j] == dbm2[i,j] < dbm1[i,j]
             *
             * Note: if tightening dbm1[j,i] with -dbm2[i,j] is empty
             * then the remainder does not change.
             */
            if (dbm2[idim+j] < dbm1[idim+j])
            {
                raw_t negConstraint = dbm_negRaw(dbm2[idim+j]);

#ifndef NDISJOINT_DBM_SUBSTRACT
                /* since the remainder is constrained,
                 * I don't know what will happen of these
                 * conditions.
                 * In the case of minimal split the last
                 * one may be false, but only the last one.
                 */
                if (negConstraint < dbm1[j*dim+i])
                {
#endif
#ifndef NMINIMAL_SUBSTRACT_SPLIT
                    if (dbmf_facetteIntersects(dbm1, dbm2, dim, idim, i, j))
                    {
#endif
                        DBMList_t *split = dbmf_allocate(factory);
                        if (!split) /* out-of-memory */
                        {
#ifndef NDISJOINT_DBM_SUBSTRACT
                            if (remainder) dbmf_deallocate(factory, remainder);
#endif
                            if (result) dbmf_deallocateList(factory, result);
                            *valid = FALSE;
                            return NULL;
                        }
                        split->next = result;
                        result = split;
                        dbm_copy(result->dbm, dbm1, dim);
                        
                        /* substract */
                        result->dbm[j*dim+i] = negConstraint;
                        dbm_closeij(result->dbm, dim, j, i);
#ifndef NDISJOINT_DBM_SUBSTRACT
                        if (remainder)
                        {
                            remainder->dbm[idim+j] = dbm2[idim+j];
                            dbm_closeij(remainder->dbm, dim, i, j);
                        }
#endif
#ifndef NMINIMAL_SUBSTRACT_SPLIT
                    }
#endif
#ifndef NDISJOINT_DBM_SUBSTRACT
                }
                else
                {
                    /* dbm2[i,j] < dbm1[i,j] => dbm2 tightens dbm1
                     * -dbm2[i,j] >= dbm1[j,i] => substraction == remainder
                     * and next remainder == empty
                     * This code CANNOT be executed if remainder == NULL
                     * because negConstraint < dbm1[j*dim+i] holds because
                     * of preconditions.
                     */
                    assert(remainder);
                    remainder->next = result;
                    return remainder;
                }
#endif
            }
            
            assert(nbConstraints);
            if (!--nbConstraints)
            {
#ifndef NDISJOINT_DBM_SUBSTRACT
                if (remainder) dbmf_deallocate(factory, remainder);
#endif
                return result;
            }
            
            /* next */
            ++j;
            assert(count);
        }
        j += count; /* number of unread bits */
    }
}


/* - evaluate if substraction should be computed or not
 *   if not: return a copy
 * - compute minimal form of dbm2
 * - for all constraints in the minimal form:
 *   - negate the constraint
 *   - tighten a copy of dbm1 with the negated constraint
 */
DBMList_t* dbmf_substractDBMFromDBM(const raw_t *dbm1, const raw_t *dbm2, cindex_t dim,
                                    BOOL *valid, DBMAllocator_t *factory)
{
    assert(dbm1 && dbm2 && valid);
    dbmf_CHECKDIM(factory, dim);

    *valid = TRUE;

    if (dim > 1)
    {
        /* We can avoid calling _isSubstractEqualToIdentity
         * if _needsToSubstractDBM is called.
         * However, we cannot avoid the call to _needsMinimalSubstract.
         */
#ifndef NTRY_SKIP_REDUCE
        if (dbm_haveIntersection(dbm1, dbm2, dim))
        {
#endif
            uint32_t minDBM[bits2intsize(dim*dim)];
            size_t nb = dbm_analyzeForMinDBM(dbm2, dim, minDBM);
            if (nb
#ifdef NTRY_SKIP_REDUCE
                && !dbmf_isSubstractEqualToIdentity(dbm1, dbm2, dim, minDBM, nb)
#endif
                )
            {
                return dbmf_internSubstractDBMFromDBM(dbm1, dbm2, dim,
                                                      valid, factory,
                                                      minDBM, nb);
            }
#ifndef NTRY_SKIP_REDUCE
        }
#endif

        return dbmf_validCopyDBM2List(dbm1, dim, valid, factory);
    }
    else
    {
        return NULL; /* == empty */
    }
}


/* For all DBMs in the federation do
 *  substract dbm from it and add (resulting federation) to result
 */
DBMFederation_t dbmf_substractDBMFromFederation(const DBMFederation_t fed, const raw_t *dbm,
                                                BOOL *valid, DBMAllocator_t *factory)
{
    DBMFederation_t result;
    BOOL localValid = TRUE;
    dbmf_initFederation(&result, fed.dim);

    assert(dbm && valid);
    dbmf_CHECKDIM(factory, fed.dim);

    /* if dim == 1 then empty */
    if (fed.dim > 1 && fed.dbmList)
    {
        uint32_t minDBM[bits2intsize(fed.dim*fed.dim)];
        size_t nbConstraints = dbm_analyzeForMinDBM(dbm, fed.dim, minDBM);
        if (!nbConstraints)
        {
            dbmf_copy(&result, fed, factory);
        }
        else
        {
            DBMList_t *current = fed.dbmList;
            do {
                DBMList_t *partial =
                    dbmf_isSubstractEqualToIdentity(current->dbm, dbm, fed.dim,
                                                    minDBM, nbConstraints) ?
                    dbmf_validCopyDBM2List(current->dbm, fed.dim, &localValid, factory) :
                    dbmf_internSubstractDBMFromDBM(current->dbm, dbm, fed.dim,
                                                   &localValid, factory,
                                                   minDBM, nbConstraints);
                if (!localValid)
                {
                    assert(!partial);
                    if (result.dbmList)
                    {
                        dbmf_deallocateList(factory, result.dbmList);
                        result.dbmList = NULL;
                    }
                    break;
                }
                if (partial)
                {
                    dbmf_appendList(partial, result.dbmList);
                    result.dbmList = partial;
                }
                current = current->next;
            } while(current);
        }
    }

    *valid = localValid;
    return result;
}


/* init result = fed1 - fed2.1st DBM
 * for all DBMs toSubstract in remaining fed2 do
 *   result = result - toSubstract
 * done
 * There is a useless copy per DBM of fed2 that has
 * no constraint at all, which should never occur in practice.
 */
DBMFederation_t dbmf_substractFederationFromFederation(const DBMFederation_t fed1,
                                                       const DBMFederation_t fed2,
                                                       BOOL *valid, DBMAllocator_t *factory)
{
    DBMFederation_t result;
    BOOL localValid = TRUE;

    dbmf_CHECKDIM(factory, fed1.dim);
    assert(fed1.dim == fed2.dim && valid);

    dbmf_initFederation(&result, fed1.dim);

    if (fed1.dbmList)
    {
        if (fed2.dbmList)
        {
            if (fed1.dim > 1)
            {
                const DBMList_t *toSubstract = fed2.dbmList;
                result = dbmf_substractDBMFromFederation(fed1, toSubstract->dbm, &localValid, factory);
                if (localValid)
                {
                    while (result.dbmList && toSubstract->next)
                    {
                        toSubstract = toSubstract->next;
                        if (dbmf_substractDBM(&result, toSubstract->dbm, factory))
                        {
                            localValid = FALSE;
                            break;
                        }
                    }
                }
            } /* else result empty */
        }
        else /* fed2 empty => copy fed1 */
        {
            if (dbmf_copy(&result, fed1, factory)) localValid = FALSE;
        }
    }
    /* else fed1 empty */

    if (!localValid && result.dbmList)
    {
        dbmf_deallocateList(factory, result.dbmList);
        result.dbmList = NULL;
    }
    *valid = localValid;

    return result;
}


/* - if federation empty return copy of dbm
 * - otherwise:
 *   init result = dbm - 1st dbm of fed
 *   if result empty or no more dbms then stop
 * - go through current result and compute new result
 *   by appending result of current result -> dbm - dbm of federation
 *   stop if current result is empty
 */
DBMList_t* dbmf_substractFederationFromDBM(const raw_t *dbm,
                                           const DBMFederation_t fed,
                                           BOOL *valid, DBMAllocator_t *factory)
{
    dbmf_CHECKDIM(factory, fed.dim);
    assert(valid && dbm);

    *valid = TRUE;

    if (fed.dim == 1)
    {
        return NULL; /* empty */
    }
    else if (fed.dbmList)
    {
        DBMFederation_t result;
        BOOL localValid;
        result.dim = fed.dim;
        result.dbmList =
            dbmf_substractDBMFromDBM(dbm, fed.dbmList->dbm, fed.dim, &localValid, factory);

        if (localValid && result.dbmList && fed.dbmList->next)
        {
            if (dbmf_substractFederation(&result, fed.dbmList->next, factory))
            {
                localValid = FALSE;
            }
        }
        if (!localValid)
        {
            if (result.dbmList) dbmf_deallocateList(factory, result.dbmList);
            *valid = FALSE;
            return NULL;
        }
        else
        {
            return result.dbmList;
        }
    }
    else
    {
        return dbmf_validCopyDBM2List(dbm, fed.dim, valid, factory);
    }
}


/* if something to substract:
 * result = NULL
 * for current in DBMs of fed->list do
 *   result += current - dbm
 *   deallocate current
 * done
 * return result
 */
int dbmf_substractDBM(DBMFederation_t *fed, const raw_t *dbm,
                       DBMAllocator_t *factory)
{
    assert(fed && dbm);
    dbmf_CHECKDIM(factory, fed->dim);

    if (fed->dbmList)
    {
        cindex_t dim = fed->dim;
        if (dim > 1)
        {
            uint32_t minDBM[bits2intsize(dim*dim)];
            size_t nbConstraints = dbm_analyzeForMinDBM(dbm, dim, minDBM);
            if (nbConstraints) /* if something to substract */
            {
                DBMList_t *current = fed->dbmList;
                fed->dbmList = NULL; /* result */
                do {
                    DBMList_t *next = current->next;
                    
                    /* if current - dbm == current, take it
                     * otherwise compute substraction
                     */
                    if (dbmf_isSubstractEqualToIdentity(current->dbm, dbm, dim,
                                                        minDBM, nbConstraints))
                    {
                        current->next = fed->dbmList;
                        fed->dbmList = current;
                    }
                    else
                    {
                        BOOL localValid = TRUE;
                        DBMList_t *partial =
                            dbmf_internSubstractDBMFromDBM(current->dbm, dbm, dim,
                                                           &localValid, factory,
                                                           minDBM, nbConstraints);
                        if (!localValid)
                        {
                            assert(!partial);
                            dbmf_deallocateList(factory, current);
                            return -1;
                        }
                        else
                        {
                            /* consume current */
                            dbmf_deallocate(factory, current);
                        }

                        if (partial)
                        {
                            dbmf_appendList(partial, fed->dbmList);
                            fed->dbmList = partial;
                        }
                    }
                    
                    current = next;
                } while(current);
            }
        }
        else /* dim == 1 => empty */
        {
            dbmf_deallocateList(factory, fed->dbmList);
            fed->dbmList = NULL;
        }
    }

    return 0;
}


/* straight-forward since there is no copy
 */
int dbmf_substractFederation(DBMFederation_t *fed1, const DBMList_t *toSubstract,
                             DBMAllocator_t *factory)
{
    assert(fed1);
    dbmf_CHECKDIM(factory, fed1->dim);

    while(toSubstract && fed1->dbmList)
    {
        if (dbmf_substractDBM(fed1, toSubstract->dbm, factory))
        {
            if (fed1->dbmList)
            {
                dbmf_deallocateList(factory, fed1->dbmList);
                fed1->dbmList = NULL;
            }
            return -1;
        }
        toSubstract = toSubstract->next;
    }

    return 0;
}

BOOL simpleBad(const raw_t *good, const raw_t *bad, cindex_t dim,
               DBMList_t **toSubtract, BOOL *valid, DBMAllocator_t *factory)
{
    cindex_t i;
    // test if all lower bounds of bad are strictly tighter than good
    for(i = 1; i < dim; ++i) if (bad[i] >= good[i]) return FALSE;
    if (dbm_haveIntersection(good, bad, dim))
    {
        DBMList_t *copyBad = dbmf_copyDBM2List(bad, dim, factory);
        if (!copyBad)
        {
            *valid = FALSE;
        }
        else
        {
            copyBad->next = *toSubtract;
            *toSubtract = copyBad;
        }
    }
    return TRUE;
}

/* toSubtract = {}
 * for all bad DBMs in badF:
 * - toSubtract += all intersecting parts of bad - goodF
 *
 * This is all wrong: predt(U goods, U bads) = intersec predt(U goods, bads)
 * and the intersection is not computed as it should be.
 */
int dbmf_predt(DBMFederation_t *goodF, DBMFederation_t *badF, DBMAllocator_t *factory)
{
    assert(goodF && badF && goodF->dim == badF->dim);
    dbmf_CHECKDIM(factory, goodF->dim);
    dbmf_checkFactory(factory);

    /* trivial cases */

    if (dbmf_isEmpty(*goodF))
    {
        return 0;
    }
    else if (dbmf_isEmpty(*badF))
    {
        dbmf_down(*goodF);
        return 0;
    }
    else /* not trivial */
    {
#if 1
        BOOL valid = TRUE;
        DBMFederation_t fedG, fedB;
        fedG.dim = goodF->dim;
        fedB.dim = badF->dim;
        fedG.dbmList = dbmf_copyDBMList(goodF->dbmList, goodF->dim, &valid, factory);
        fedB.dbmList = dbmf_copyDBMList(badF->dbmList, badF->dim, &valid, factory);
        dbmf_down(fedG);
        dbmf_down(fedB);
        // fedG and fedB should not be empty
        if (fedG.dbmList && fedB.dbmList &&
            !dbmf_substractFederation(&fedG, fedB.dbmList, factory) &&
            !dbmf_intersectsFederation(goodF, fedB.dbmList, factory) &&
            !dbmf_substractFederation(goodF, badF->dbmList, factory))
        {
            // fedG = down(goodF)-down(badF)
            // fedB = down(badF)
            // goodF = (goodF intersect down(badF))-badF
            
            if (goodF->dbmList)
            {
                dbmf_down(*goodF);
                dbmf_union(goodF, fedG.dbmList, factory);
            }
            else
            {
                goodF->dbmList = fedG.dbmList;
            }
            fedG.dbmList = NULL;
        }
        else
        {
            dbmf_deallocateFederation(factory, goodF);
            valid = FALSE;
        }
        dbmf_deallocateFederation(factory, badF);
        if (fedG.dbmList) dbmf_deallocateList(factory, fedG.dbmList);
        if (fedB.dbmList) dbmf_deallocateList(factory, fedB.dbmList);
        dbmf_checkFactory(factory);
        return valid ? 0 : -1;

#else // former new predt
        BOOL valid = TRUE;
        DBMFederation_t goodDownFed;
        DBMList_t *goodDBMs = goodF->dbmList;
        DBMList_t *goodResult = NULL;
        DBMList_t *copyGood = dbmf_allocate(factory);
        cindex_t dim = goodF->dim;

        if (!copyGood)
        {
            dbmf_deallocateFederation(factory, goodF);
            dbmf_deallocateFederation(factory, badF);
            return -1;
        }
        goodF->dbmList = NULL;
        copyGood->next = NULL;
        goodDownFed.dim = dim;

        DBMFederation_t goodWithoutBad, badDown;
        goodWithoutBad.dim = dim;
        badDown.dim = dim;
        badDown.dbmList = dbmf_copyDBMList(badF->dbmList, dim, &valid, factory);
        if (!valid)
        {
            dbmf_deallocate(factory, copyGood);
            dbmf_deallocateList(factory, goodDBMs);
            dbmf_deallocateFederation(factory, badF);
            return -1;
        }
        dbmf_down(badDown);

        /* loop on DBMs of goodDBMs */
        do {
            BOOL resultOK = TRUE;
            DBMList_t *toSubtract = NULL;
            DBMList_t *goodDown = goodDBMs;
            DBMList_t *currentBad = badF->dbmList;
            DBMList_t *currentBadDown = badDown.dbmList;
            goodDBMs = goodDBMs->next;
            goodDownFed.dbmList = goodDown;
            goodDown->next = NULL;
            
            /* the current DBM of goodDBMs = goodDown
             * and goodDown is put in goodDownFed
             * + goodDBMs is set for the next iteration
             */

            dbm_copy(copyGood->dbm, goodDown->dbm, dim);
            dbm_down(goodDown->dbm, dim);

            /* collect bad DBMs to subtract */
            
            do {
                /* if good is included in bad */
                if (dbm_relation(copyGood->dbm, currentBad->dbm, dim) & base_SUBSET)
                {
                    dbmf_deallocate(factory, goodDown); // no good left
                    resultOK = FALSE;
                    break;
                }

                if (!simpleBad(copyGood->dbm, currentBad->dbm, dim, &toSubtract, &valid, factory))
                {
                    goodWithoutBad.dbmList = dbmf_substractDBMFromDBM(copyGood->dbm, currentBad->dbm, dim, &valid, factory);
                    if (!valid) break;
                    if (dbmf_intersectsDBM(&goodWithoutBad, currentBadDown->dbm, factory))
                    {
                        dbmf_down(goodWithoutBad);
                        DBMList_t *reallyBad = dbmf_substractFederationFromDBM(currentBadDown->dbm, goodWithoutBad, &valid, factory);
                        dbmf_deallocateList(factory, goodWithoutBad.dbmList);
                        if (!valid) break;
                        if (reallyBad)
                        {
                            dbmf_appendList(reallyBad, toSubtract);
                            toSubtract = reallyBad;
                        }
                    }
                    else
                    {
                        DBMList_t *reallyBad = dbmf_copyDBM2List(currentBadDown->dbm, dim, factory);
                        if (!reallyBad) { valid = FALSE; break; }
                        reallyBad->next = toSubtract;
                        toSubtract = reallyBad;
                    }
                }
                if (!valid) break;
                currentBad = currentBad->next;
                currentBadDown = currentBadDown->next;
            } while(currentBad);

            if (resultOK)
            {
                /* subtract bad DBMs to down(good) */

                valid = valid && dbmf_substractFederation(&goodDownFed, toSubtract, factory) == 0;
                /* now use goodDownFed instead of goodDown (same, but use the federation struct) */

                if (toSubtract) dbmf_deallocateList(factory, toSubtract);

                if (!valid)
                {
                    dbmf_deallocate(factory, copyGood);
                    dbmf_deallocateList(factory, badDown.dbmList);
                    if (goodDownFed.dbmList) dbmf_deallocateList(factory, goodDownFed.dbmList);
                    if (goodDBMs) dbmf_deallocateList(factory, goodDBMs);
                    dbmf_deallocateFederation(factory, badF);
                    return -1;
                }

                /* accumulate result */
                if (goodDownFed.dbmList)
                {
                    dbmf_appendList(goodDownFed.dbmList, goodResult);
                    goodResult = goodDownFed.dbmList;
                } /* else don't touch goodResult */
            }
            else
            {
                if (toSubtract) dbmf_deallocateList(factory, toSubtract);
            }

        } while(goodDBMs);

        dbmf_deallocate(factory, copyGood);
        dbmf_deallocateList(factory, badDown.dbmList);
        assert(badF && badF->dbmList);
        dbmf_deallocateFederation(factory, badF);
        dbmf_checkFactory(factory);
        goodF->dbmList = goodResult;
        return 0;
#endif
    }
}


void dbmf_shrinkExpand(DBMFederation_t *fed,
                       const uint32_t *bitSrc,
                       const uint32_t *bitDst,
                       size_t bitSize,
                       cindex_t *table,
                       DBMAllocator_t *factory)
{
    assert(factory && fed && fed->dim <= factory->maxDim);

    /* see dbm_shrinkExpand */
    assert(bitSrc && bitDst && table);
    assert(bitSize && fed->dim && (*bitSrc & *bitDst & 1));
    assert(fed->dim == base_countBitsN(bitSrc, bitSize));
    assert(!base_areBitsEqual(bitSrc, bitDst, bitSize));
    
    if (fed->dbmList) /* if non empty */
    {
        cindex_t dimSrc, dimDst;
        cindex_t cols[factory->maxDim];

        /* compute tables, update dim */
        dimSrc = fed->dim;
        dimDst = dbm_computeTables(bitSrc, bitDst, bitSize, table, cols);
        fed->dim = dimDst;

        assert(dimDst && dimDst <= factory->maxDim);

        DBMList_t *oldList = fed->dbmList;

        if (dimDst <= 1) /* trivial case, can purge the federation */
        {
            assert(*oldList->dbm == dbm_LE_ZERO);
            if (oldList->next)
            {
                dbmf_deallocateList(factory, oldList->next);
                oldList->next = NULL;
            }
        }
        else
        {
            DBMList_t **newList = &fed->dbmList;
            DBMList_t *dbmList = dbmf_allocate(factory);

            /* init new list */
            *newList = NULL;

            do {
                /* add to list */
                dbmList->next = *newList;
                *newList = dbmList;

                /* update DBM */
                *dbmList->dbm = dbm_LE_ZERO;
                dbm_updateDBM(dbmList->dbm, oldList->dbm,
                              dimDst, dimSrc, cols);
                
                /* read next, recycle current */
                dbmList = oldList;
                oldList = oldList->next;
            } while(oldList);

            /* deallocate last */
            dbmf_deallocate(factory, dbmList);
        }
    }
    else
    {
        /* only compute the translation table */
        base_bits2indexTable(bitDst, bitSize, table);
    }
}


/* apply dbm_classicExtrapolateMaxBounds to all the DBMs of a federation.
 */
void dbmf_extrapolateMaxBounds(DBMFederation_t fed, const int32_t *max)
{
    assert(max);
    APPLY_TO_FEDERATION(fed,
                        dbm_extrapolateMaxBounds(_first->dbm, fed.dim, max));
}


/* apply dbm_diagonalExtrapolateMaxBounds to all the DBMs of a federation
 */
void dbmf_diagonalExtrapolateMaxBounds(DBMFederation_t fed, const int32_t *max)
{
    assert(max);
    APPLY_TO_FEDERATION(fed,
                        dbm_diagonalExtrapolateMaxBounds(_first->dbm, fed.dim, max));
}

/* apply dbm_extrapolateLUBounds to all the DBMs of a federation
 */
void dbmf_extrapolateLUBounds(DBMFederation_t fed,
                              const int32_t *lower, const int32_t *upper)
{
    assert(lower && upper);
    APPLY_TO_FEDERATION(fed,
                        dbm_extrapolateLUBounds(_first->dbm, fed.dim, lower, upper));
}

/* apply dbm_diagonalExtrapolateLUBounds to all the DBMs of a federation
 */
void dbmf_diagonalExtrapolateLUBounds(DBMFederation_t fed,
                                      const int32_t *lower, const int32_t *upper)
{
    assert(lower && upper);
    APPLY_TO_FEDERATION(fed,
                        dbm_diagonalExtrapolateLUBounds(_first->dbm, fed.dim, lower, upper));
}
