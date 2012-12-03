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
// Filename : Federation.h (dbm)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: Federation.h,v 1.21 2005/05/03 19:46:52 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DBM_FEDERATION_H
#define INCLUDE_DBM_FEDERATION_H

#include <new>
#include <cstddef>
#include <iostream>
#include "dbm/dbmfederation.h"
#include "base/bitstring.h"

//
// THIS API IS DEPRECATED AND NOT SUPPORTED ANY LONGER.
//

/** Macro to check common assumptions on clock indices
 * to be translated to DBM indices.
 * @param I: index to check.
 */
#define ASSERT_CLOCK_INDEX(I) \
assert(dbmAllocator && (I) < dbmAllocator->maxDim && indexTable[I] < getNumberOfClocks())


/** Macro to declare on the stack a Federation (hack).
 * @param NAME: name of the federation to use (as a pointer)
 * @param NBCLOCKS: number of clocks for the federation
 * @param FACTORY: factory to use for this federation
 * Note: this hack only works because there is no virtual method.
 */
#define LOCAL_FEDERATION(NAME, NBCLOCKS, FACTORY) \
char local_##NAME[Federation::getSizeOfFederationFor((FACTORY)->maxDim)];\
Federation *NAME = (Federation*) local_##NAME; \
NAME->initFederation(FACTORY, NBCLOCKS)


/** Similar macro to LOCAL_FEDERATION with active clocks.
 * @param NAME: name of the federation to use (as a pointer)
 * @param NBCLOCKS: number of clocks for the federation
 * @param FACTORY: factory to use for this federation
 * @param ACTIVE: bitstring marking active clocks
 * @param ACTIVESIZE: size of ACTIVE (in ints)
 * Note: this hack only works because there is no virtual method.
 */
#define LOCAL_ACTIVE_FEDERATION(NAME, NBCLOCKS, FACTORY, ACTIVE, ACTIVESIZE) \
char local_##NAME[Federation::getSizeOfFederationFor((FACTORY)->maxDim)];\
Federation *NAME = (Federation*) local_##NAME; \
NAME->initFederation(FACTORY, NBCLOCKS, ACTIVE, ACTIVESIZE)

// Declaration in namespace to have it friend.
namespace state
{
    class SymbolicState;
}

namespace dbm
{
    class Federation
    {
    public:

        /** Create a federation, given a factory and a number of clocks.
         * @param factory: factory to (de-)allocate DBMs
         * @param nbClocks: number of clocks for this federation (=dimension of the DBMs)
         * @return a new federation
         */
        static Federation* createFederation(DBMAllocator_t *factory, cindex_t nbClocks) throw (std::bad_alloc)
        {
            assert(factory && nbClocks && factory->maxDim >= nbClocks);
            return new (allocateFor(factory->maxDim)) Federation(factory, nbClocks);
        }

        /** Create a federation, given an original federation, ie, copy.
         * @param original: the original federation to copy.
         * @return a new federation
         */
        static Federation* createFederation(const Federation *original) throw (std::bad_alloc)
        {
            assert(original);
            return new (allocateFor(original->dbmAllocator->maxDim)) Federation(*original);
        }

        /** Create a federation, given a factory, a number of clocks, and an active clock mapping (bit string).
         * @param factory: factory to (de-)allocate DBMs
         * @param nbClocks: number of clocks (=dimension of the DBMs)
         * @param activeClocks,size: bitstring (of 'size' ints) marking the active clocks
         * @pre (activeClocks[0] & 1) == 1 (reference clock), size <= factory->maxDim.
         */
        static Federation* createFederation(DBMAllocator_t *factory, cindex_t nbClocks,
                                            const uint32_t *activeClocks, size_t size) throw (std::bad_alloc)
        {
            assert(factory && nbClocks && factory->maxDim >= nbClocks);
            assert(activeClocks && size <= factory->maxDim);
            return new (allocateFor(factory->maxDim)) Federation(factory, nbClocks, activeClocks, size);
        }

        /** Destructor:
         * purge all the zones from this federation,
         * ie, give them back to the factory.
         * Warning: the factory does not belong to this federation
         * so it is not destroyed.
         */
        ~Federation();

        /** Unshare its allocator.
         */
        void unshareAllocator() { dbmf_initFactory(dbmAllocator, dbmFederation.dim); }


        /** Operator =
         * @pre factories are the same.
         */
        Federation& operator = (const Federation& original) throw (std::bad_alloc)
        {
            assert(dbmAllocator == original.dbmAllocator);
            if (dbmf_copy(&dbmFederation, original.dbmFederation, dbmAllocator))
            {
                throw std::bad_alloc();
            }
            base_copySmall(indexTable, original.indexTable,
                           (sizeof(cindex_t) == sizeof(uint32_t)) // removed at compile time (-O2)
                            ? original.dbmAllocator->maxDim
                            : intsizeof(cindex_t[original.dbmAllocator->maxDim]));
            return *this;
        }

        /** Steal DBMs from a federation and computes the union with this federation:
         * the argument federation looses its DBMs.
         * @param fed: federation to steal from.
         * @pre getNumberOfClocks() == fed->getNumberOfClocks()
         */
        void stealDBMs(Federation *fed)
        {
            assert(fed && getNumberOfClocks() == fed->getNumberOfClocks());
            if (fed->dbmFederation.dbmList)
            {
                dbmf_union(&dbmFederation, fed->dbmFederation.dbmList, dbmAllocator);
                fed->dbmFederation.dbmList = NULL;
            }
        }

        /** Serialize this federation to a stream.
         * @param os: output stream.
         */
        std::ostream& serialize(std::ostream& os) const;

        /** Unserialize this federation from a stream.
         * @param is: input stream.
         */
        void unserialize(std::istream& is) throw (std::bad_alloc);

        /** Pretty print this federation to a stream.
         * @param os: output stream.
         */
        std::ostream& prettyPrint(std::ostream& os) const;

        /** Number of clocks used in this federation:
         * all the zones have the same number of clocks.
         * @return number of clocks.
         */
        cindex_t getNumberOfClocks() const { return dbmFederation.dim; }

        /** (Re-)initialize the federation so that it contains
         * only 0. The federation may be empty or not, it does not matter.
         */
        void initToZero();

        /** (Re-)initialize the federation so that it is completely
         * unconstrained. The federation may be empty or not, it does not matter.
         */
        void initUnconstrained();

        /** Intersection with one DBM.
         * @param dbm: DBM to intersect with.
         * @pre DBM dimension == getNumberOfClocks()
         * @return true if federation is not empty.
         */
        bool intersection(const raw_t *dbm)
        {
            assert(dbm);
            return (bool) dbmf_intersectsDBM(&dbmFederation, dbm, dbmAllocator);
        }

        /** Intersection with a federation.
         * @param anotherFed: federation to intersec with.
         * @return true if federation is not empty.
         */
        bool intersection(const Federation *anotherFed) throw (std::bad_alloc)
        {
            assert(anotherFed);
            if (dbmf_intersectsFederation(&dbmFederation, anotherFed->dbmFederation.dbmList, dbmAllocator))
            {
                throw std::bad_alloc();
            }
            return dbmFederation.dbmList != NULL;
        }

        /** Apply a constraint to the federation = tighten a constraint.
         * @param clockIndexI,clockIndexJ,rawBound: bound for the constraint xi-xj (raw format)
         * @pre clockIndexI and clockIndexJ < maximal number of clocks
         * and indexTable[clockIndexI and clockIndexJ] < getNumberOfClocks()
         * @return true if the federation is non empty.
         */
        bool constrain(cindex_t clockIndexI, cindex_t clockIndexJ, raw_t rawBound)
        {
            ASSERT_CLOCK_INDEX(clockIndexI);
            ASSERT_CLOCK_INDEX(clockIndexJ);
            dbmf_constrain1(&dbmFederation,
                            indexTable[clockIndexI], indexTable[clockIndexJ], rawBound,
                            dbmAllocator);
            return dbmFederation.dbmList != NULL;
        }

        /** Apply several constraints to the federation.
         * @param constraints: constraints to apply
         * @param n: number of constraints
         * @pre constraints is a constraint_t[n], indices i must be s.t.
         * indexTable[i] < getNumberOfClocks() and i < dbmAllocator->maxDim
         * @return true if the federation is non empty.
         */
        bool constrainN(const constraint_t *constraints, size_t n)
        {
            dbmf_constrainIndexedN(&dbmFederation, indexTable,
                                   constraints, n,
                                   dbmAllocator);
            return dbmFederation.dbmList != NULL;
        }

        /** Wrapper for constrain(uint,uint,raw_t)
         * @param c: constraint to set
         */
        bool constrain(constraint_t c)
        {
            return constrain(c.i, c.j, c.value);
        }

        /** Wrapper for constrain(uint,uint,raw_t)
         * @param clockIndexI,clockIndexJ,bound,isStrict: bound with a isStrict flag for
         * the constraint xi-xj.
         * @pre clockIndexI and clockIndexJ < maximal number of clocks
         * and indexTable[clockIndexI and clockIndexJ] < getNumberOfClocks()
         */
        bool constrain(cindex_t clockIndexI, cindex_t clockIndexJ, int32_t bound, bool isStrict)
        {
            return constrain(clockIndexI, clockIndexJ, dbm_boundbool2raw(bound, (BOOL) isStrict));
        }

        /** Wrapper for constrain(uint,uint,raw_t)
         * @param clockIndexI,clockIndexJ,bound,strictness: bound with a strictness for
         * the constraint xi-xj.
         * @pre clockIndexI and clockIndexJ < maximal number of clocks
         * and indexTable[clockIndexI and clockIndexJ] < getNumberOfClocks()
         */
        bool constrain(cindex_t clockIndexI, cindex_t clockIndexJ, int32_t bound, strictness_t strictness)
        {
            return constrain(clockIndexI, clockIndexJ, dbm_bound2raw(bound, strictness));
        }

        /** @return true if the federation satisfies the constraint.
         * @param clockIndexI,clockIndexJ,rawConstraint: constraint (raw format) to
         * be satisfied by xi-xj.
         * @pre clockIndexI and clockIndexJ < maximal number of clocks
         * and indexTable[clockIndexI and clockIndexJ] < getNumberOfClocks()
         */
        bool satisfies(cindex_t clockIndexI, cindex_t clockIndexJ, raw_t rawConstraint) const
        {
            ASSERT_CLOCK_INDEX(clockIndexI);
            ASSERT_CLOCK_INDEX(clockIndexJ);
            return (bool) dbmf_satisfies(dbmFederation,
                                         indexTable[clockIndexI], indexTable[clockIndexJ], rawConstraint);
        }

        /** Wrapper for satisfies(uint,uint,raw_t)
         * @param constraint: the constraint to test
         * @pre see satisfies(cindex_t,cindex_t,raw_t)
          */
        bool satisfies(constraint_t constraint) const
        {
            return satisfies(constraint.i, constraint.j, constraint.value);
        }

        /** Wrapper for satisfies(uint,uint,raw_t)
         * @param clockIndexI,clockIndexJ,bound,isStrict: bound with a isStrict flag for
         * the constraint xi-xj.
         * @pre see satisfies(cindex_t,cindex_t,raw_t)
         */
        bool satisfies(cindex_t clockIndexI, cindex_t clockIndexJ, int32_t bound, bool isStrict) const
        {
            return satisfies(clockIndexI, clockIndexJ, dbm_boundbool2raw(bound, (BOOL) isStrict));
        }

        /** Wrapper for satisfies(uint,uint,raw_t)
         * @param clockIndexI,clockIndexJ,bound,strictness: bound with a strictness for
         * the constraint xi-xj.
         * @pre see satisfies(cindex_t,cindex_t,raw_t)
         */
        bool satisfies(cindex_t clockIndexI, cindex_t clockIndexJ, int32_t bound, strictness_t strictness) const
        {
            return satisfies(clockIndexI, clockIndexJ, dbm_bound2raw(bound, strictness));
        }

        /** Up operation = strongest post-condition
         * = delay = future computation.
         */
        void up()
        {
            dbmf_up(dbmFederation);
        }

        /** Down operation = weakest pre-condition
         * = past computation.
         */
        void down()
        {
            dbmf_down(dbmFederation);
        }

        /** @return true if the federation is unbounded,
         * ie, if one zone is unbounded.
         */
        bool isUnbounded() const
        {
            return (bool) dbmf_isUnbounded(dbmFederation);
        }
        
        /** @return true if the federation is empty.
         */
        bool isEmpty() const
        {
            return dbmFederation.dbmList == NULL;
        }

        /** "Micro" delay operation: render upper
         * bounds xi <= bound non strict: xi < bound
         */
        void microDelay()
        {
            dbmf_microDelay(dbmFederation);
        }

        /** Substract a DBM from this federation.
         * @param dbm: DBM to substract
         * @pre dimension of the DBM = getNumberOfClocks()
         * @return true if this federation is non empty
         */
        bool substract(const raw_t *dbm) throw (std::bad_alloc)
        {        
            if (dbmf_substractDBM(&dbmFederation, dbm, dbmAllocator))
            {
                throw std::bad_alloc();
            }
            return dbmFederation.dbmList != NULL;
        }
        
        /** Substract a federation from this federation.
         * @param fed: federation to substract
         * @pre fed->getNumberOfClocks() == getNumberOfClocks()
         * and fed has the same factory.
         * @return true if this federation is non empty
         */
        bool substract(const Federation *fed) throw (std::bad_alloc)
        {
            assert(fed && fed->getNumberOfClocks() == getNumberOfClocks() &&
                   fed->dbmAllocator == dbmAllocator);
            if (dbmf_substractFederation(&dbmFederation, fed->dbmFederation.dbmList, dbmAllocator))
            {
                throw std::bad_alloc();
            }
            return dbmFederation.dbmList != NULL;
        }

        /** For convenience: -= operator.
         * @see substract
         */
        Federation& operator -= (const Federation& fed) throw (std::bad_alloc)
        {
            assert(fed.getNumberOfClocks() == getNumberOfClocks() &&
                   fed.dbmAllocator == dbmAllocator);
            if (dbmf_substractFederation(&dbmFederation, fed.dbmFederation.dbmList, dbmAllocator))
            {
                throw std::bad_alloc();
            }
            return *this;
        }

        /** Approximate inclusion check: if it is included
         * then it is for sure but if it is not, then maybe
         * it could have been.
         * @param fed: federation to test
         * @return true if this federation is maybe included in fed
         */
        bool isMaybeIncludedIn(const Federation *fed)
        {
            assert(fed && fed->getNumberOfClocks() == getNumberOfClocks() &&
                   fed->dbmAllocator == dbmAllocator);
            return dbmf_areDBMsIncludedIn(dbmFederation.dbmList, fed->dbmFederation.dbmList, getNumberOfClocks());
        }

        /** Exact inclusion check. EXPENSIVE.
         * @param fed: federation to test
         * @return true if this federation is really included in fed
         */
        bool isReallyIncludedIn(const Federation *fed)
        {
            assert(fed && fed->getNumberOfClocks() == getNumberOfClocks() &&
                   fed->dbmAllocator == dbmAllocator);
            BOOL valid;
            BOOL isIn = dbmf_areDBMsReallyIncludedIn(dbmFederation.dbmList, fed->dbmFederation, &valid, dbmAllocator);
            if (!valid) throw std::bad_alloc();
            return isIn;
        }

        /** Free a given clock: remove all the constraints (except >= 0)
         * for a given clock.
         * @param clockIndex: index of the clock to free
         * @pre clockIndex < dbmAllocator->maxDim && indexTable[clockIndex] < getNumberOfClocks()
         */
        void freeClock(cindex_t clockIndex)
        {
            ASSERT_CLOCK_INDEX(clockIndex);
            dbmf_freeClock(dbmFederation, indexTable[clockIndex]);
        }

        /** Update of the form x := value, where x is a clock and value
         * an integer.
         * @param clockIndex: index of the clock (x) to update.
         * @param value: value to set.
         * @pre clockIndex < dbmAllocator->maxDim && indexTable[clockIndex] < getNumberOfClocks()
         */
        void updateValue(cindex_t clockIndex, int32_t value)
        {
            ASSERT_CLOCK_INDEX(clockIndex);
            dbmf_updateValue(dbmFederation, indexTable[clockIndex], value);
        }

        /** Update of the form xi := xj, where xi and xj are clocks.
         * @param clockIndexI,clockIndexJ: clock indices of xi and xj.
         * @pre clockIndexI/J < dbmAllocator->maxDim && indexTable[clockIndexI/J] < getNumberOfClocks()
         */
        void updateClock(cindex_t clockIndexI, cindex_t clockIndexJ)
        {
            ASSERT_CLOCK_INDEX(clockIndexI);
            ASSERT_CLOCK_INDEX(clockIndexJ);
            dbmf_updateClock(dbmFederation, indexTable[clockIndexI], indexTable[clockIndexJ]);
        }

        /** Update of the form x += inc, where x is a clock and inc
         * an integer. Warning: inc may be < 0. The operation may
         * be wrong when used on an unbounded federation, or if the
         * "decrement" is too big (negative time). The operation is
         * there but be careful in its usage. Decidability is at stake.
         * @param clockIndex: index of the clock (x) to increment
         * @param increment: increment to apply
         * @pre clockIndex < dbmAllocator->maxDim && indexTable[clockIndex] < getNumberOfClocks()
         */
        void updateIncrement(cindex_t clockIndex, int32_t increment)
        {
            ASSERT_CLOCK_INDEX(clockIndex);
            dbmf_updateIncrement(dbmFederation, indexTable[clockIndex], increment);
        }

        /** General update of the form: xi := xj + inc, where xi and xj
         * are clocks and inc an integer. Warning: same remark as for updateIncrement.
         * @param clockIndexI, clockIndexJ: clock indices of clocks (xi,xj)
         * @param increment: increment to apply
         * @pre clockIndexI/J < dbmAllocator->maxDim && indexTable[clockIndexI/J] < getNumberOfClocks()
         */
        void update(cindex_t clockIndexI, cindex_t clockIndexJ, int32_t increment)
        {
            ASSERT_CLOCK_INDEX(clockIndexI);
            ASSERT_CLOCK_INDEX(clockIndexJ);
            dbmf_update(dbmFederation, indexTable[clockIndexI], indexTable[clockIndexJ], increment);
        }

        /** Partial relation with a DBM (from DBM point of view).
         * The result is partial in the sense that it can be
         * dbm_DIFFERENT although it could be refined as dbm_SUBSET.
         * @param dbm: DBM to test
         * @pre dimension of dbm == getNumberOfClocks()
         * @return relation dbm (?) this federation where
         * (?) can be
         * - dbm_EQUAL: this federation represents exactly the zone
         *   of dbm
         * - dbm_SUBSET: dbm is a subset of this federation, ie,
         *   is included in one of the DBMs of this federation
         * - dbm_SUPERSET: dbm is a superset of this federation, ie,
         *   includes all the DBMs of this federation
         * - dbm_DIFFERENT: dbm is not (easily) comparable to this federation.
         */
        relation_t partialRelation(const raw_t *dbm) const
        {
            return dbmf_partialRelation(dbm, dbmFederation);
        }

        /** Exact (and very expensive) relation with a DBM, in contrast
         * to partialRelation.
         * @param dbm: DBM to test
         * @pre dimension of dbm == getNumberOfClocks()
         * @return relation dbm (?) this federation as partialRelation.
         */
        relation_t relation(const raw_t *dbm) const throw (std::bad_alloc)
        {
            BOOL valid;
            relation_t result = dbmf_relation(dbm, dbmFederation, &valid, dbmAllocator);
            if (!valid) throw std::bad_alloc();
            return result;
        }

        /** Compute union of federations with inclusion checking.
         * Argument is deallocated or belongs to this federation.
         * @param fed: list of DBMs to add to this federation.
         */
        void unionWith(DBMList_t *fed)
        {
            dbmf_union(&dbmFederation, fed, dbmAllocator);
        }

        /** As unionWith but copy the argument when needed.
         * @param fed: list of DBMs to add.
         */
        void unionWithCopy(const DBMList_t *fed) throw (std::bad_alloc)
        {
            BOOL valid;
            dbmf_unionWithCopy(&dbmFederation, fed, dbmAllocator, &valid);
            if (!valid) throw std::bad_alloc();
        }

        /** Compute convex union of the DBMs of this federation.
         */
        void convexUnion()
        {
            dbmf_convexUnionWithSelf(&dbmFederation, dbmAllocator);
        }

        /** Stretch-up: remove all upper bounds for all clocks.
         */
        void stretchUp()
        {
            dbmf_stretchUp(dbmFederation);
        }

        /** Stretch-down: remove all lower bounds for a given clock.
         * @param clockIndex: index of the concerned clock.
         * @pre clockIndex < dbmAllocator->maxDim && indexTable[clockIndex] < getNumberOfClocks()
         */
        void stretchDown(cindex_t clockIndex)
        {
            ASSERT_CLOCK_INDEX(clockIndex);
            dbmf_stretchDown(dbmFederation, indexTable[clockIndex]);
        }

        /** Operation similar to down but constrained:
         * predecessors of fed avoiding the states that
         * could reach bad.
         * @param bad: the list of DBMs to avoid.
         * @pre the list of DBMs has the same number of clocks
         * as this federation and the active clocks match.
         * @post DBMs of bad are deallocated.
         */
        void predt(Federation *bad)
        {
            if (dbmf_predt(&dbmFederation, &bad->dbmFederation, dbmAllocator)) throw std::bad_alloc();
        }

        /** Add DBMs to this federation.
         * @arg dbmList: list of DBMs
         * @pre DBMs have the same dimension as this federation.
         */
        void add(const DBMList_t *dbmList) throw (std::bad_alloc)
        {
            if (dbmf_addCopy(&dbmFederation, dbmList, dbmAllocator)) throw std::bad_alloc();
        }

        /** Add DBMs of a federation to this federation.
         * @pre DBMs have the same dimension as this federation.
         */
        Federation* add(const Federation *arg) throw (std::bad_alloc)
        {
            assert(arg && arg->getNumberOfClocks() == getNumberOfClocks());
            add(arg->dbmFederation.dbmList);
            return this;
        }

        /** Operator +=: add DBMs of another federation.
         */
        Federation& operator += (const Federation& arg) throw (std::bad_alloc)
        {
            assert(arg.getNumberOfClocks() == getNumberOfClocks());
            add(arg.dbmFederation.dbmList);
            return *this;
        }

        /** Simple reduce: try to remove redundant DBMs.
         */
        void reduce()
        {
            dbmf_reduce(&dbmFederation, dbmAllocator);
        }

        /** Very EXPENSIVE reduce, follows full semantics of federation.
         */
        void expensiveReduce()
        {
            dbmf_expensiveReduce(&dbmFederation, dbmAllocator);
        }

        /** Remove partially included DBMs in the argument federation.
         * @return TRUE if this federation is not empty afterwards.
         * @param arg: argument federation.
         */
        bool removePartialIncludedIn(const Federation *arg)
        {
            assert(arg);
            return dbmf_removePartialIncluded(&dbmFederation, arg->dbmFederation.dbmList, dbmAllocator);
        }

        /** Access to the factory: for allocation/deallocation.
         * @return this factory.
         */
        DBMAllocator_t *getAllocator()
        {
            return dbmAllocator;
        }

        /** const access to factory, only for debug (use in assert).
         */
        const DBMAllocator_t* getAllocator() const
        {
            return dbmAllocator;
        }

        /** const access to the DBMs.
         * @return the head of the list of DBMs.
         */
        const DBMList_t* getDBMList() const
        {
            return dbmFederation.dbmList;
        }

        /** Access to the DBMs.
         * @return the head of the list of DBMs.
         */
        DBMList_t* getDBMList()
        {
            return dbmFederation.dbmList;
        }

        /** Access to the DBMs.
         * @return the pointer to the head of the list of DBMs.
         */
        DBMList_t** getAtDBMList()
        {
            return &dbmFederation.dbmList;
        }

        /** Access to the first DBM.
         * @pre federation not empty.
         */
        raw_t *getFirstDBM()
        {
            assert(dbmFederation.dbmList);
            return dbmFederation.dbmList->dbm;
        }
          
        /** Size of federation
         * @return the number of zones in the federation.
         */
        size_t getSize() const
        {
            return dbmf_getSize(dbmFederation.dbmList);
        }

        /** Allocate and add a DBM.
         * @return newly added and allocated DBM.
         * @post the DBM is not initialized and is invalid. It *must* be 
         * written afterwards.
         */
        raw_t *newDBM() throw (std::bad_alloc())
        {
            DBMList_t *freshNew = dbmf_allocate(dbmAllocator);
            if (!freshNew) throw std::bad_alloc();
            dbmf_addDBM(freshNew, &dbmFederation);
            return freshNew->dbm;
        }

        /** Reset the federation to empty.
         */
        void reset()
        {
            if (dbmFederation.dbmList)
            {
                dbmf_deallocateList(dbmAllocator, dbmFederation.dbmList);
                dbmFederation.dbmList = NULL;
            }
        }

        /** Classical extrapolation based on maximal bounds,
         * formerly called k-normalization.
         * @see dbm_classicExtrapolateMaxBounds
         * @param max: table of maximal constants to use for the active clocks.
         * - max is a int32_t[getNumberOfClocks()]
         * - max[0] = 0 (reference clock)
         */
        void extrapolateMaxBounds(const int32_t *max)
        {
            dbmf_extrapolateMaxBounds(dbmFederation, max);
        }

        /** Diagonal extrapolation based on maximal bounds.
         * @see dbm_diagonalExtrapolateMaxBounds
         * @param max: table of maximal constants for the active clocks.
         * @pre
         * - max is a int32_t[getNumberOfClocks()]
         * - max[0] = 0 (reference clock)
         */
        void diagonalExtrapolateMaxBounds(const int32_t *max)
        {
            dbmf_diagonalExtrapolateMaxBounds(dbmFederation, max);
        }

        /** Extrapolation based on lower-upper bounds.
         * @see dbm_extrapolateLUBounds
         * @param lower: lower bounds for the active clocks.
         * @param upper: upper bounds for the active clocks.
         * @pre
         * - lower and upper are int32_t[getNumberOfClocks()]
         * - lower[0] = upper[0] = 0 (reference clock)
         */
        void extrapolateLUBounds(const int32_t *lower, const int32_t *upper)
        {
            dbmf_extrapolateLUBounds(dbmFederation, lower, upper);
        }

        /** Diagonal extrapolation based on lower-upper bounds.
         * Most general approximation.
         * @see dbm_diagonalExtrapolateLUBounds
         * @param lower: lower bounds for the active clocks.
         * @param upper: upper bounds for the active clocks.
         * @pre
         * - lower and upper are a int32_t[getNumberOfClocks()]
         * - lower[0] = upper[0] = 0
         */
        void diagonalExtrapolateLUBounds(const int32_t *lower, const int32_t *upper)
        {
            dbmf_diagonalExtrapolateLUBounds(dbmFederation, lower, upper);
        }

        /** Initialize the indirection table to identity.
         * Warning: the DBMs are all deallocated.
         * @param newDim: new dimension of all the DBMs
         */
        void initIndexTable(cindex_t newDim)
        {
            assert(dbmAllocator && dbmAllocator->maxDim && dbmAllocator->maxDim >= newDim);
            
            dbmFederation.dim = newDim;
            reset();

            cindex_t i = 0, max = dbmAllocator->maxDim;
            cindex_t *table = indexTable;
            do { table[i] = i; } while(++i < max);
        }

        /** Initialize the indirection table according to a bitstring
         * marking active clocks. Warning: the DBMs are all deallocated.
         * @param activeClocks,size: bitstring (of 'size' ints) marking active clocks
         * @pre (activeClocks[0] & 1) == 1 (always reference clock)
         * and n <= getBitTableSize(), and size > 0 (reference clock).
         */
        void initIndexTable(const uint32_t *activeClocks, size_t size)
        {
            assert(activeClocks && size);
            dbmFederation.dim = base_countBitsN(activeClocks, size);
            reset();
            base_bits2indexTable(activeClocks, size, indexTable);
        }

        /** Initialize a federation, given a factory and a number of clocks.
         * @param dbmf: DBM factory
         * @param nbClocks: number of clocks (=dimension of the DBMs)
         * @pre nbClocks < dbmf->maxDim
         */
        void initFederation(DBMAllocator_t *dbmf, cindex_t nbClocks)
        {
            dbmAllocator = dbmf;
            dbmFederation.dbmList = NULL;
            initIndexTable(nbClocks);
        }

        /** Initialize a federation, given a factory, a number of clocks,
         * and a bitstring marking the active clocks.
         * @param dbmf: DBM factory
         * @param nbClocks: number of clocks (=dimension of the DBMs)
         * @param activeClocks,size: bitstring (of 'size' ints) marking the active clocks
         * @pre nbClocks < dbmf->maxDim and (activeClocks[0] & 1) == 1 (reference clock),
         * size <= dbmf->maxDim/32 rounded up, and the index of the highest bit in activeClocks
         * < dbmAllocator->maxDim.
         */
        void initFederation(DBMAllocator_t *dbmf, cindex_t nbClocks,
                            const uint32_t *activeClocks, size_t size)
        {
            assert(dbmf && activeClocks && nbClocks < dbmf->maxDim &&
                   size <= (size_t) ((dbmf->maxDim + 31) >> 5));
            dbmf_initFederation(&dbmFederation, nbClocks);
            dbmAllocator = dbmf;
            initIndexTable(activeClocks, size);
        }

        /** Change the set of active clocks.
         * @param oldClocks: old set of active clocks, MUST correspond to the
         * current index table.
         * @param newClocks: new set of active clocks, the index table will be
         * updated according to these clocks
         * @param size: size (int ints) of the bitstrings
         */
        void changeClocks(const uint32_t *oldClocks, const uint32_t *newClocks, size_t size)
        {
            if (!base_areBitsEqual(oldClocks, newClocks, size))
            {
                dbmf_shrinkExpand(&dbmFederation, oldClocks, newClocks, size, indexTable, dbmAllocator);
            }
        }

    private: // Constructors are private: use the static methods createXX.

        friend class state::SymbolicState;

        /** Copy the federation without the index table.
         * @param original: original federation.
         */
        void copyFromFederation(const Federation *original) throw (std::bad_alloc)
        {
            assert(original && dbmAllocator == original->dbmAllocator);
            if (dbmf_copy(&dbmFederation, original->dbmFederation, dbmAllocator))
            {
                throw std::bad_alloc();
            }
        }

        /** Allocate memory for a Federation of maxDim clocks. The
         * index table is allocated rounded up to the next 32 multiple
         * of maxDim.
         */
        static void* allocateFor(cindex_t maxDim)
        {
            return new char[getSizeOfFederationFor(maxDim)];
        }

        /** @return needed size for a federation with a given maximal dimension.
         */
        static size_t getSizeOfFederationFor(cindex_t maxDim)
        {
            return sizeof(Federation)+sizeof(cindex_t[maxDim]);
        }

        /** @return size needed in ints in addition to this class.
         * If pointers are on 64 bits then we pad on 64 bits and not 32.
         * The Federation object must be padded on 64 bits too on 64 bits,
         * which is done by the compiler since Federation contains pointers.
         */
        static size_t getPaddedIntOffsetFor(cindex_t maxDim)
        {
            // the tests are removed at compile time
            return (sizeof(uintptr_t) == sizeof(uint32_t) || (intsizeof(Federation) & 1))
                ? ((sizeof(cindex_t) == sizeof(uint32_t)) ? maxDim : intsizeof(cindex_t[maxDim]))
                : ((sizeof(cindex_t[maxDim]) + 7) >> 3);
        }

        /** Constructor.
         * @param dbmf: zone factory
         * @param nbClocks: number of clocks for all the DBMs in this federation.
         * @post the federation is empty
         */
        Federation(DBMAllocator_t *dbmf, cindex_t nbClocks)
        {
            initFederation(dbmf, nbClocks);
        }

        /** Copy constructor.
         * Warning: this calls copy, which means, be careful, therefor it is private.
         */
        Federation(const Federation& original) throw (std::bad_alloc)
        {
            dbmAllocator = original.dbmAllocator;
            /* reset this because the copy operates
             * on a valid federation */
            dbmFederation.dim = original.dbmFederation.dim;
            dbmFederation.dbmList = NULL;
            *this = original;
        }

        /** Constructor with active clocks given.
         * @param dbmf: zone factory
         * @param nbClocks: number of clocks for all the DBMs in this federation.
         * @param activeClocks,size: bitstring (of 'size' ints) marking the active clocks.
         * @post the federation is empty
         */
        Federation(DBMAllocator_t *dbmf, cindex_t nbClocks,
                   const uint32_t *activeClocks, size_t size)
        {
            initFederation(dbmf, nbClocks, activeClocks, size);
        }


        DBMFederation_t dbmFederation; ///< wrapped federation (C struct)
        DBMAllocator_t *dbmAllocator;  ///< allocator to (de-)allocate DBMs
        cindex_t indexTable[];         ///< indirection index table
    };

} // namespace dbm


// Wrappers

static inline
std::ostream& operator << (std::ostream& os, const dbm::Federation& fed)
{
    return fed.serialize(os);
}

static inline
void operator >> (std::istream& is, dbm::Federation& fed)
{
    fed.unserialize(is);
}

#endif // INCLUDE_DBM_FEDERATION_H

