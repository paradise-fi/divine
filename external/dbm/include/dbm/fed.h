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
// Filename : fed.h
//
// C++ federation & DBMs
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: fed.h,v 1.37 2005/10/17 17:11:13 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DBM_FED_H
#define INCLUDE_DBM_FED_H

#include <stdexcept>
#include <vector>
#include "base/pointer.h"
#include "dbm/dbm.h"
#include "dbm/mingraph.h"
#include "dbm/Valuation.h"
#include "dbm/ClockAccessor.h"

/** @file
 * This API offers access to DBMs and federations
 * (of DBMs) through 2 classes: dbm_t and fed_t.
 * These classes hide memory management and reference
 * counting. Copy-on-write semantics is implemented.
 */
namespace dbm
{
    // internal classes
    class idbm_t;
    class ifed_t;
    class fdbm_t;
    class dbmlist_t;

    // public classes
    class dbm_t;
    class fed_t;

    /// Wrapper class for clock operations, @see dbm_t
    template<class TYPE>
    class ClockOperation
    {
    public:
        /// Clock increment and decrement.
        ClockOperation& operator + (int32_t val);
        ClockOperation& operator - (int32_t val);
        ClockOperation& operator += (int32_t val);
        ClockOperation& operator -= (int32_t val);

        /// Execute(update) clock (+value ignored) = clock + value.
        /// @pre this->dbm == op.dbm
        ClockOperation& operator = (const ClockOperation& op);

        /// Execute(updateValue) clock = value.
        ClockOperation& operator = (int32_t val);

        /// Check if clock constraints are satisfied.
        /// Semantics: does there exist a point such that it
        /// satisfies the constraint.
        /// @pre we are using the same dbm_t (or fed_t)

        bool operator <  (const ClockOperation& x) const;
        bool operator <= (const ClockOperation& x) const;
        bool operator >  (const ClockOperation& x) const;
        bool operator >= (const ClockOperation& x) const;
        bool operator == (const ClockOperation& x) const;
        bool operator <  (int32_t v) const;
        bool operator <= (int32_t v) const;
        bool operator >  (int32_t v) const;
        bool operator >= (int32_t v) const;
        bool operator == (int32_t v) const;

        /// The constructor should be used only by dbm_t or fed_t
        /// since ClockOperation is a convenience class.
        /// @param d: dbm that generated this clock operation.
        /// @param c: index of the clock to manipulate.
        /// @pre c < d->getDimension()
        ClockOperation(TYPE* d, cindex_t c);
            
        /// Access to the arguments of the operation.
        TYPE* getPtr()     const { return ptr; }
        cindex_t getClock() const { return clock; }
        int32_t getVal()   const { return incVal; }

    private:
        TYPE* ptr;      /// related dbm_t or fed_t, no reference count
        cindex_t clock;  /// clock to read/write
        int32_t incVal; /// increment value to apply
    };


    /***********************************************************************
     * dbm_t: DBM type. A DBM is basically a matrix of dimension >= 1,
     * ALWAYS >= 1.
     *
     * Special features:
     *
     * - sharing of DBMs (intern()): a cheap way to save memory
     *   for DBMs that are to be kept but not modified is to share
     *   identical DBMs (by using a hash table internally). Different
     *   dbm_t may point to the same idbm_t data. Another effect of
     *   this call is that if 2 dbm_t have had their intern() methods
     *   called, then test for equality is reduced to pointer testing,
     *   which is provided by the sameAs method.
     *
     * - direct access to the DBM matrix: read-only access is
     *   straight-forward (with the () operator). Read-write access
     *   (getDBM()) is more subtle because there may be no DBM allocated or
     *   the internal DBM may be shared with another dbm_t. Furthermore,
     *   the invariant of dbm_t is that it has a DBM matrix iff it is
     *   closed and non empty. Thus, when asking for a read-write
     *   direct access, we may need to allocate and copy internally.
     *   After the direct access, the invariant must also hold.
     *   Direct access must not be mixed with accessing the original
     *   dbm_t in case the matrix is deallocated for some reason.
     *
     * - relation with fed_t (the federation type): relation(xx) and
     *   the operators == != < > ... have the semantics of set inclusion
     *   check with DBMs pair-wise. These relations are exact when a
     *   dbm_t is compared with a dbm_t but are approximate when a dbm_t
     *   is compared with a fed_t, since the semantics is DBM inclusion
     *   check! We do not detect if a union of DBM is equal (semantically
     *   with respect to sets) to another DBM with the relation(xx) methods
     *   and < > <= .. operators. The (semantic) exact relation is provided
     *   by the exactRelation(xx) and le lt ge.. methods. The results from
     *   the approximate relations are safe, in the sense that EQUAL, SUBSET,
     *   and SUPERSET are reliable. However, DIFFERENT could be refined, we
     *   don't know. This is enough for most cases. The special case
     *   dbm_t >= fed_t is an exact comparison.
     *
     * - the closed form is maintained internally.
     *
     * - interaction with "raw matrices" is supported, ie, it is possible
     *   to use DBMs as arrays raw_t[dim*dim] where dim is the dimension
     *   of the DBMs. However, in these cases, it is assumed that these
     *   DBMs are valid, which is, dbm_isValid(dbm, dim) holds.
     *
     * - convenient operations:
     *   Assume you have declared dbm_t a,b;
     *   then you can write the following expressions:
     *   a(2) = a(3) + 3; is the clock update x2 = x3 + 3 for the DBM a
     *   a(1) = 0; is the clock update value (also called reset) x1 = 0
     *   a(1) += 3; is the clock increment x1 += 3
     *   b &= dbm_t(a(2) + 2); is the intersection of the result of
     *   x2 += 2 in for the DBM a and b (though a is not changed).
     *   if (a(2) < a(1) + 2) tests if the clock constraint x2 < x1 + 2 holds
     *   with respect to the DBM a.
     *   See the interface of ClockOperation.
     *   IMPORTANT: YOU ARE NOT SUPPOSED TO USE dbm_t::ClockOperation EXPLICITELY
     *   like declaring dbm_t::ClockOperation = .. and hack with it: this would
     *   give wrong results because of how + and - are implemented. This
     *   is kept simple here for efficiency and simplicity and
     *   let the compiler do its magic and optimize the final compiled
     *   inlined expressions!
     *
     ************************************************************************/

    class dbm_t
    {
        friend class fed_t;
        friend class dbmlist_t;
    public:
        // Define maximal dimension as a bit mask.
        // 2^15-1 means trouble anyway (2^15-1)^2 bytes per DBM!
        enum
        {
            MAX_DIM_POWER = 15,
            MAX_DIM = ((1 << MAX_DIM_POWER) - 1),
        };

        /// Initialize a dbm_t to empty DBM of a given dimension.
        /// @param dim: dimension of the DBM.
        /// @post isEmpty()
        explicit dbm_t(cindex_t dim = 1);

        /// Standard copy constructor.
        dbm_t(const dbm_t& arg);

        /// Copy constructor from a DBM matrix.
        /// @post isEmpty() iff dim == 0
        dbm_t(const raw_t* arg, cindex_t dim);

        ~dbm_t();

        /// @return the dimension of this DBM.
        cindex_t getDimension() const;

        /// @return string representation of the
        /// constraints of this DBM. A clock
        /// is always positive, so "true" simply means
        /// all clocks positive.
        std::string toString(const ClockAccessor&) const;

        /** Make an unbounded DBM with the lower bounds set to low
         * (strict constraints).
         */
        static dbm_t makeUnbounded(const int32_t *low, cindex_t dim);

        /** Wrapper for dbm_getSizeOfMinDBM. Here for other compatibility reasons.
         * @pre dimension == getDimension()
         */
        static size_t getSizeOfMinDBM(cindex_t dim, mingraph_t);
        
        /** Construct a dbm_t from a mingraph_t. Not as a constructor
         * for other compatibility reasons.
         * @pre dimension == getDimension()
         */
        static dbm_t readFromMinDBM(cindex_t dim, mingraph_t);

        /// Change the dimension of this DBM.
        /// The resulting DBM is empty. @post isEmpty()
        void setDimension(cindex_t dim);

        /// @return true if it is empty.
        bool isEmpty() const;

        /// Empty this DBM.
        void setEmpty();

        /// Short for setDimension(1), has the effect of deallocating the DBM.
        void nil();
        
        /// @return true if this DBM contains the zero point.
        bool hasZero() const;

        /// @return a hash value.
        uint32_t hash(uint32_t seed = 0) const;

        /// @return true if arg has the same internal idbmPtr.
        bool sameAs(const dbm_t& arg) const;

        /// Try to share the DBM.
        void intern();

        /// Copy from a DBM.
        void copyFrom(const raw_t *src, cindex_t dim);

        /// Copy to a DBM.
        /// @pre dbm_isValid(dst, dim) and dim == getDimension()
        void copyTo(raw_t *dst, cindex_t dim) const;
        
        // Overload of operators () and []:
        // dbm_t::()    -> DBM matrix
        // dbm_t::(i)   -> Clock access for clock i
        // dbm_t::(i,j) -> read contraint DBM[i,j]
        // dbm_t::[i]   -> return matrix row i.

        /// @return read-only pointer to the internal DBM matrix.
        /// @post non null pointer iff !isEmpty()
        const raw_t* operator () () const;
        
        /// @return DBM[i,j]
        /// @pre !isEmpty() && i < getDimension() && j < getDimension() otherwise segfault.
        raw_t operator () (cindex_t i, cindex_t j) const;

        /// @return row DBM[i]
        /// @pre !isEmpty() && i < getDimension()
        const raw_t* operator [] (cindex_t i) const;

        /// @return a read-write access pointer to the internal DBM.
        /// @post return non null pointer iff getDimension() > 0
        raw_t* getDBM();

        /** Compute the minimal set of constraints to represent
         * this DBM.
         * @see dbm_analyzeForMinDBM.
         * @return the number of constraints of the set.
         * @pre bitMatrix is a uint32_t[bits2intsize(dim*dim)] and !isEmpty()
         */
        size_t analyzeForMinDBM(uint32_t *bitMatrix) const;

        /** Compute & save the minimal set of constraints.
         * @param tryConstraints16: flag to try to save
         * constraints on 16 bits, will cost dim*dim time.
         * @param c_alloc: C allocator wrapper
         * @param offset: offset for allocation.
         * @return allocated memory.
         * @pre !isEmpty().
         */
        int32_t* writeToMinDBMWithOffset(bool minimizeGraph,
                                         bool tryConstraints16,
                                         allocator_t c_alloc,
                                         size_t offset) const;

        /** Similar to writeToMinDBMWithOffset but works with
         * a pre-analyzed DBM.
         * @pre !isEmpty() and bitMatrix corresponds to its
         * analysis (otherwise nonsense will be written).
         * @see dbm_writeAnalyzedDBM.
         * @post bitMatrix is cleaned from the constraints xi>=0.
         */
        int32_t* writeAnalyzedDBM(uint32_t *bitMatrix,
                                  size_t nbConstraints,
                                  BOOL tryConstraints16,
                                  allocator_t c_alloc,
                                  size_t offset) const;

#ifdef ENABLE_STORE_MINGRAPH
        /** @return its minimal set of constraints.
         * The result is cached, so consecutive calls are
         * very cheap unless the DBM is changed in-between.
         * The bit matrix returned is an uint32_t[bits2intsize(dim*dim)].
         * WARNING: You are not supposed to read anything from this
         * pointer when the DBM changes in any way (deallocation too).
         * @param size is the number of constraints in the graph.
         * @pre !isEmpty()
         */
        const uint32_t* getMinDBM(size_t *size) const;
#endif

        /** Relation with a mingraph_t, @see dbm_relationWithMinDBM.
         * @pre unpackBuffer is a raw_t[dim*dim].
         */
        relation_t relation(mingraph_t ming, raw_t* unpackBuffer) const;

        /// Overload of standard operators.
        /// The raw_t* arguments are assumed to be matrices of the same
        /// dimension of this dbm_t (and dbm_isValid also holds).

        dbm_t& operator = (const dbm_t&);
        dbm_t& operator = (const raw_t*);

        /// Comparisons have the semantics of set inclusion.
        /// Equality of dimensions is checked.
        /// Comparisons agains fed_t are approximate and cheap
        /// since done between DBMs pair-wise. See the header.

        bool operator == (const dbm_t&) const;
        bool operator == (const fed_t&) const;
        bool operator == (const raw_t*) const;
        bool operator != (const dbm_t&) const;
        bool operator != (const fed_t&) const;
        bool operator != (const raw_t*) const;
        bool operator <  (const dbm_t&) const;
        bool operator <  (const fed_t&) const;
        bool operator <  (const raw_t*) const;
        bool operator >  (const dbm_t&) const;
        bool operator >  (const fed_t&) const;
        bool operator >  (const raw_t*) const;
        bool operator <= (const dbm_t&) const;
        bool operator <= (const fed_t&) const;
        bool operator <= (const raw_t*) const;
        bool operator >= (const dbm_t&) const;
        bool operator >= (const fed_t&) const; // exact
        bool operator >= (const raw_t*) const;

        /// Relation (wrt inclusion, approximate only for fed_t).
        /// @return this (relation) arg.

        relation_t relation(const dbm_t& arg) const;
        relation_t relation(const fed_t& arg) const;
        relation_t relation(const raw_t* arg, cindex_t dim) const;

        /// Exact (expensive) relations (for fed_t only).

        bool lt(const fed_t& arg) const; // this less than arg
        bool gt(const fed_t& arg) const; // this greater than arg
        bool le(const fed_t& arg) const; // this less (than) or equal arg
        bool ge(const fed_t& arg) const; // this greater (than) or equal arg
        bool eq(const fed_t& arg) const; // this equal arg
        relation_t exactRelation(const fed_t& arg) const;

        /// Set this zone to zero (origin).
        /// @param tau: tau clock, @see dbm.h
        /// @post !isEmpty() iff dim > 0.
        dbm_t& setZero();

        /// (re-)initialize the DBM with no constraint.
        /// @post !isEmpty() iff dim > 0.
        dbm_t& setInit();

        /// @return dbm_isEqualToInit(DBM), @see dbm.h
        bool isInit() const;

        /// @return dbm_isEqualToZero(DBM), @see dbm.h
        bool isZero() const;

        /** Computes the biggest lower cost in the zone.
         *  This corresponds to the value
         *  \f$\sup\{ c \mid \exists v \in Z : c =
         *  \inf \{ c' \mid v[cost\mapsto c'] \in Z \} \}\f$
         */
        int32_t getUpperMinimumCost(int32_t cost) const;

        /// Only for compatibility with priced DBMs.
        int32_t getInfimum() const { return 0; }

        /// Convex union operator (+).
        /// @pre same dimension.

        dbm_t& operator += (const dbm_t&);
        dbm_t& operator += (const fed_t&); // += argument.convexHull()
        dbm_t& operator += (const raw_t*);

        /// Intersection and constraint operator (&).
        /// @pre same dimension, compatible indices,
        /// and i != j for the constraints.

        dbm_t& operator &= (const dbm_t&);
        dbm_t& operator &= (const raw_t*);
        dbm_t& operator &= (const constraint_t&);
        dbm_t& operator &= (const base::pointer_t<constraint_t>&);
        dbm_t& operator &= (const std::vector<constraint_t>&);

        /// Compute the intersection with the axis of a given clock.
        /// @return !isEmpty()
        bool intersectionAxis(cindex_t);

        /** Methods for constraining: with one or more constraints.
         * Variants with @param table: indirection table for the indices.
         * - clock xi == value
         * - clocks xi-xj < cij or <= cij (constraint = c)
         * - clocks xi-xj < or <= b depending on strictness (strictness_t or bool).
         * - or several constraints at once.
         * @pre compatible indices, i != j for the constraints, and
         * table is an cindex_t[getDimension()]
         * @return !isEmpty()
         */
        bool constrain(cindex_t i, int32_t value);
        bool constrain(cindex_t i, cindex_t j, raw_t c);
        bool constrain(cindex_t i, cindex_t j, int32_t b, strictness_t s);
        bool constrain(cindex_t i, cindex_t j, int32_t b, bool isStrict);
        bool constrain(const constraint_t& c);
        bool constrain(const constraint_t *c, size_t n);
        bool constrain(const cindex_t *table, const constraint_t *c, size_t n);
        bool constrain(const cindex_t *table, const base::pointer_t<constraint_t>&);
        bool constrain(const cindex_t *table, const std::vector<constraint_t>&);

        /// @return false if there is no intersection with the argument
        /// or true if there *may* be an intersection.
        /// @pre same dimension.

        bool intersects(const dbm_t&) const;
        bool intersects(const fed_t&) const;
        bool intersects(const raw_t*, cindex_t dim) const; // dim here for safety check

        /// Delay (strongest post-condition). Remove upper bounds.
        /// @return this
        dbm_t& up();

        /// Delay, except for stopped clocks.
        /// @param stopped is a bit array marking which clocks are stopped.
        /// @pre if stopped != NULL then it is a uint32_t[bits2intsize(getDimension())].
        dbm_t& upStop(const uint32_t* stopped);

        /// Inverse delay (weakest pre-condition). Remove lower bounds.
        /// @return this
        dbm_t& down();

        /// Similar to upStop but for inverse delay.
        dbm_t& downStop(const uint32_t* stopped);

        /// Free clock (unconstrain). Remove constraints on a particular
        /// clock, both upper and lower bounds.
        /// @return this. @post freeClock(c) == freeUp(c).freeDown(c)
        dbm_t& freeClock(cindex_t clock);

        /// Free upper or lower bounds only for a particular clock or
        /// for all clocks. @pre 0 < clock < getDimension()
        /// @return this. @see dbm.h

        dbm_t& freeUp(cindex_t clock);
        dbm_t& freeDown(cindex_t clock);
        dbm_t& freeAllUp();
        dbm_t& freeAllDown();

        /** Update methods where x & y are clocks, v an integer value.
         * x := v     -> updateValue
         * x := y     -> updateClock
         * x := x + v -> updateIncrement
         * x := y + v -> update
         * @pre 0 < x and y < getDimension(), v < infinity, and v is
         * s.t. the clocks stay positive.
         */
        void updateValue(cindex_t x, int32_t v);
        void updateClock(cindex_t x, cindex_t y);
        void updateIncrement(cindex_t x, int32_t v);
        void update(cindex_t x, cindex_t y, int32_t v);

        /// Check if the DBM satisfies a constraint c_ij.
        /// @pre i != j, i and j < getDimension()

        bool satisfies(cindex_t i, cindex_t j, raw_t c) const;
        bool satisfies(const constraint_t& c) const;
        bool operator && (const constraint_t& c) const;

        /// @return true if this DBM contains points that can delay arbitrarily.
        bool isUnbounded() const;

        /// Make upper or lower finite bounds non strict.
        /// @see dbm.h.
        /// @return this.
        dbm_t& relaxUp();
        dbm_t& relaxDown();

        /// Make lower bounds strict.
        /// @return this.
        dbm_t& tightenDown();

        /// Make upper bounds strict.
        /// @return this.
        dbm_t& tightenUp();

        /// Similar for all bounds of a particular clock.
        /// @see dbm.h. Special for clock == 0:
        /// relaxUp(0) = relaxDown() and relaxDown(0) = relaxUp().
        dbm_t& relaxUpClock(cindex_t clock);
        dbm_t& relaxDownClock(cindex_t clock);

        /// Make all constraints (except infinity) non strict.
        dbm_t& relaxAll();

        /// Test point inclusion.
        /// @pre same dimension.

        bool contains(const IntValuation& point) const;
        bool contains(const int32_t* point, cindex_t dim) const;
        bool contains(const DoubleValuation& point) const;
        bool contains(const double* point, cindex_t dim) const;

        /** Compute the 'almost min' necessary delay from
         * a point to enter this federation. If this point
         * is already contained in this federation, 0.0 is
         * returned. The result is 'almost min' since we
         * want a discrete value, which is not possible in
         * case of strict constraints.
         * @pre dim == getDimension() and point[0] == 0.0
         * otherwise the computation will not work.
         * @return true if it is possible to reach this DBM
         * by delaying, or false if this DBM is empty or it
         * is not possible to reach it by delaying.
         * The delay is written in t.
         * @param minVal,minStrict: another (optional) output
         * is provided in the form of a delay value and a
         * flag telling if the delay is strict, e.g.,
         * wait >= 2.1 or wait > 2.1.
         * @pre minVal and minStrict are both NULL or non NULL.
         */
        bool getMinDelay(const DoubleValuation& point, double* t,
                         double *minVal = NULL, bool *minStrict = NULL,
                         const uint32_t* stopped = NULL) const;
        bool getMinDelay(const double* point, cindex_t dim, double* t,
                         double *minVal = NULL, bool *minStrict = NULL,
                         const uint32_t* stopped = NULL) const;

        /**
         * Compute the max delay from a point such that it respects the
         * upper bound constraints of the DBM.
         * @return true if the delay is possible, false otherwise.
         */
        bool getMaxDelay(const DoubleValuation& point, double* t,
                         double *minVal = NULL, bool *minStrict = NULL,
                         const uint32_t* stopped = NULL) const;
        bool getMaxDelay(const double* point, cindex_t dim, double* t,
                         double *minVal = NULL, bool *minStrict = NULL,
                         const uint32_t* stopped = NULL) const;

        /** Similarly for the past.
         *  The returned value (in t) is <= max.
         *  @pre max > 0 otherwise this is meaningless.
         */
        bool getMaxBackDelay(const DoubleValuation& point, double* t, double max) const;
        bool getMaxBackDelay(const double* point, cindex_t dim, double* t, double max) const;

        bool isConstrainedBy(cindex_t, cindex_t, raw_t) const;

        /// Extrapolations: @see dbm_##method functions in dbm.h.
        /// @pre max, lower, and upper are int32_t[getDimension()]

        void extrapolateMaxBounds(const int32_t *max);
        void diagonalExtrapolateMaxBounds(const int32_t *max);
        void extrapolateLUBounds(const int32_t *lower, const int32_t *upper);
        void diagonalExtrapolateLUBounds(const int32_t *lower, const int32_t *upper);

        /** Resize this DBM: bitSrc marks the subset of clocks (out from
         * a larger total set) that are in this DBM and bitDst marks the
         * subset of clocks we want to change to. Resizing means keep the
         * constraints of the clocks that are kept, remove the constraints
         * of the clocks that are removed, and add free constraints (infinity)
         * for the new clocks.
         * @see dbm_shrinkExpand in dbm.h.
         * @param bitSrc,bitDst,bitSize: bit strings of (int) size bitSize
         * that mark the source and destination active clocks.
         * @param table: redirection table to write.
         * @pre bitSrc & bitDst are uint32_t[bitSize] and
         * table is a uint32_t[32*bitSize]
         * @post the indirection table is written.
         */
        void resize(const uint32_t *bitSrc, const uint32_t *bitDst,
                    size_t bitSize, cindex_t *table);

        /** Resize and change clocks of this DBM.
         * The updated DBM will have its clocks i coming from target[i]
         * in the original DBM.
         * @param target is the table that says where to put the current
         * clocks in the target DBM. If target[i] = ~0 then a new free
         * clock is inserted.
         * @pre newDim > 0, target is a cindex_t[newDim], and
         * for all i < newDim, target[i] < getDimension().
         */
        void changeClocks(const cindex_t *target, cindex_t newDim);

        /// Swap clocks x and y.
        void swapClocks(cindex_t x, cindex_t y);

        /** Get a clock valuation and change only the clocks
         * that are marked free.
         * @param cval: clock valuation to write.
         * @param freeC: free clocks to write. If freeC = NULL then all
         * clocks are considered free.
         * @return cval
         * @throw std::out_of_range if the generation fails
         * if isEmpty() or cval too constrained.
         * @post if freeC != NULL, forall i < dim: freeC[i] = false
         */
        DoubleValuation& getValuation(DoubleValuation& cval, bool *freeC = NULL) const 
            throw(std::out_of_range);
        
        /// Special constructor to copy the result of a pending operation.
        /// @param op: clock operation.
        dbm_t(const ClockOperation<dbm_t>& op);

        /// @see ClockOperation for more details.
        /// @pre clk > 0 except for clock constraint tests
        /// and clk < getDimension()
        ClockOperation<dbm_t> operator () (cindex_t clk);

        /** @return (this-arg).isEmpty() but it is able to
         * stop the subtraction early if it is not empty and
         * it does not modify itself.
         * @pre same dimension.
         */
        bool isSubtractionEmpty(const raw_t* arg, cindex_t dim) const;
        bool isSubtractionEmpty(const fed_t& arg) const;
        bool isSubtractionEmpty(const dbm_t& arg) const;

        /************************
         * Low-level operations *
         ************************/

        /// Simplified copy with @pre isEmpty()
        void newCopy(const raw_t *arg, cindex_t dim);

        /// Simplified copy with @pre isEmpty() && !arg.isEmpty()
        void newCopy(const dbm_t& arg);

        /// Simplified copy with @pre !isEmpty()
        void updateCopy(const raw_t* arg, cindex_t dim);

        /// Simplified copy with @pre !isEmpty() && !arg.isEmpty()
        void updateCopy(const dbm_t& arg);

        /// Const access to its idbm_t, @pre !isEmpty()
        const idbm_t* const_idbmt() const;

        /// @return idbmPtr as a pointer @pre !isEmpty()
        idbm_t* idbmt();

        /// Explicit const access to the DBM matrix.
        /// Note: const_dbm() and dbm() have different assertions.
        /// @pre !isEmpty()
        const raw_t* const_dbm() const;

        /// Mutable access to the DBM matrix.
        /// @pre isMutable()
        raw_t* dbm();

        /// @return dimension with @pre isEmpty()
        cindex_t edim() const;

        /// @return dimension with @pre !isEmpty()
        cindex_t pdim() const;
        
        /// @return true if this fed_t can be modified, @pre isPointer()
        bool isMutable() const;

        /// Set and return a new writable DBM, @pre !isEmpty()
        raw_t* getNew();

        /// Set and return a writable copy of this DBM, @pre !isEmpty()
        raw_t* getCopy();
        
    private:

        /// @return idbmPtr as an int.
        uintptr_t uval() const;

        /// Wrapper for idbmPtr.
        void incRef() const;

        /// Wrapper for idbmPtr.
        void decRef() const;

        /// Specialized versions to remove idbmPtr and setEmpty(dim), @pre !isEmpty()
        void empty(cindex_t dim);
        void emptyImmutable(cindex_t dim);
        void emptyMutable(cindex_t dim);

        /// Set idbmPtr to empty with dimension dim.
        void setEmpty(cindex_t dim);

        /// Set a pointer for idbmPtr.
        void setPtr(idbm_t* ptr);

        /// Check and try to make idbmPtr mutable cheaply (eg if reference
        /// counter is equal to 1 and the DBM is in the hash, then it is cheap).
        /// @pre !isEmpty()
        bool tryMutable();

        /// Set idbmPtr to a newly allocated DBM with explicit dimension.
        /// @pre getDimension() > 0
        raw_t* setNew(cindex_t dim);

        /// Allocate new DBM and return the matrix.
        /// @pre !isEmpty() && !tryMutable()
        raw_t* inew(cindex_t dim);

        /// Copy its DBM and return the matrix.
        /// @pre !isEmpty() && !tryMutable()
        raw_t* icopy(cindex_t dim);

        /// Widen a DBM w.r.t. drift, see fed_t.
        dbm_t& driftWiden();

        /// Implementations of previous methods with @pre isPointer()
        /// useful for fed_t since the invariant states that there is
        /// no empty dbm_t in fed_t.

        void ptr_intern();
        dbm_t& ptr_convexUnion(const raw_t *arg, cindex_t dim);
        bool ptr_intersectionIsArg(const raw_t *arg, cindex_t dim);
        bool ptr_constrain(cindex_t i, cindex_t j, raw_t c); // pre i != j
        bool ptr_constrain(cindex_t k, int32_t value);  // pre k != 0
        bool ptr_constrain(const constraint_t *cnstr, size_t n);
        bool ptr_constrain(const cindex_t *table, const constraint_t *cnstr, size_t n);
        void ptr_up();
        void ptr_upStop(const uint32_t *stopped);
        void ptr_downStop(const uint32_t *stopped);
        void ptr_down();
        void ptr_freeClock(cindex_t k); // pre k != 0
        void ptr_updateValue(cindex_t i, int32_t v); // pre i != 0
        void ptr_updateClock(cindex_t i, cindex_t j); // pre i != j, i !=0, j != 0
        void ptr_update(cindex_t i, cindex_t j, int32_t v); // pre i != 0, j != 0
        void ptr_freeUp(cindex_t k); // pre k != 0
        void ptr_freeDown(cindex_t k); // pre k != 0
        void ptr_freeAllUp();
        void ptr_freeAllDown();
        void ptr_relaxDownClock(cindex_t k);
        void ptr_relaxUpClock(cindex_t k);
        void ptr_relaxAll();
        void ptr_tightenDown();
        void ptr_tightenUp();
        bool ptr_getValuation(DoubleValuation& cval, bool *freeC) const;
        void ptr_swapClocks(cindex_t x, cindex_t y);

        // Coding of dimPtr:
        // - if dbm_t is empty, idbmPtr = (dim << 1) | 1
        // - if dbm_t is not empty idbmPtr = pointer to idbm_t and (idbmPtr & 3) == 0

    /// Internal pointer or special coding for empty.
        idbm_t* idbmPtr;
    };


    /***************************************************************
     * fed_t: federation type.
     *
     * Special features:
     *
     * - direct reference transfer: similarly to dbm_t.
     *
     * - sharing of DBMs: this is a call-back for dbm_t.
     *
     * - raw_t* argument must satisfy dbm_isValid(..)
     *
     * - relations: see dbm_t, the exact relation operations (wrt
     *   set inclusion) are exactRelation and the methods le lt gt ge eq.
     *   Approximate relations are relation, < > <= >= == !=.
     *
     * - convenient operations: like dbm_t but for all the internal
     *   DBMs. This is a call-back for all the dbm_t that are in fed_t.
     *
     * - methods marked as dummy wrappers: these are for copy arguments.
     *   In the cases where the compiler wants a fed_t&, it
     *   won't be happy to find a fed_t, so we define a number of
     *   dummy wrappers to make it understand that the thing on the
     *   stack may be taken as a reference argument too.
     *
     ***************************************************************/

    class fed_t
    {
    public:
        /// Initialize a fed_t to empty federation of a given dimension.
        /// @param dim: dimension of the federation.
        /// @post isEmpty()
        explicit fed_t(cindex_t dim = 1);

        /// Standard copy constructor.
        fed_t(const fed_t& arg);

        /// Wrap a DBM in a federation.
        fed_t(const dbm_t& arg);

        /// Copy a DBM matrix in a federation.
        fed_t(const raw_t* arg, cindex_t dim);

        ~fed_t();

        /// @return the number of DBMs in this federation.
        size_t size() const;

        /// @return the dimension of this federation.
        cindex_t getDimension() const;

        /// Change the dimension of this federation.
        /// The resulting federation is empty. @post isEmpty()
        void setDimension(cindex_t dim);

        /// @return true if it is empty.
        bool isEmpty() const;

        /// Empty this federation.
        void setEmpty();

        /// Short for setDimension(1), has the effect of deallocating the DBMs.
        void nil();
        
        /// @return true if this DBM contains the zero point.
        bool hasZero() const;

        /// @return string representation of the
        /// constraints of this federation. A clock
        /// is always positive, so "true" simply means
        /// all clocks positive.
        std::string toString(const ClockAccessor&) const;

        /** Computes the biggest lower cost in the zone.
         *  This corresponds to the value
         *  \f$\sup\{ c \mid \exists v \in Z : c =
         *  \inf \{ c' \mid v[cost\mapsto c'] \in Z \} \}\f$
         */
        int32_t getUpperMinimumCost(int cost) const;

        /// Only for compatibility with priced federations.
        int32_t getInfimum() const { return 0; }

        /// Compute the intersection with the axis on clock and returns the upper bound.
        /// Returns 0 if empty intersection.
        int32_t maxOnZero(cindex_t clock);

        /// @return a hash value that does not depend on the order of the DBMs
        /// but call reduce before to get a reliable value.
        uint32_t hash(uint32_t seed = 0) const;

        /// @return true if arg has the same internal ifedPtr.
        bool sameAs(const fed_t& arg) const;

        /// Try to share the DBMs. Side-effect: affects all copies of this fed_t.
        void intern();

        /// Overload of standard operators.
        /// The raw_t* arguments are assumed to be matrices of the same
        /// dimension of this dbm_t (and dbm_isValid also holds).
        
        fed_t& operator = (const fed_t&);
        fed_t& operator = (const dbm_t&);
        fed_t& operator = (const raw_t*);

        /// Comparisons have the semantics of set inclusion.
        /// Comparisons agains fed_t are approximate and cheap
        /// since done between DBMs pair-wise. See dbm_t.
        /// @pre same dimension for the operators < > <= >=, use
        /// relation if you don't know.

        bool operator == (const fed_t&) const;
        bool operator == (const dbm_t&) const;
        bool operator == (const raw_t*) const;
        bool operator != (const fed_t&) const;
        bool operator != (const dbm_t&) const;
        bool operator != (const raw_t*) const;
        bool operator <  (const fed_t&) const;
        bool operator <  (const dbm_t&) const;
        bool operator <  (const raw_t*) const;
        bool operator >  (const fed_t&) const;
        bool operator >  (const dbm_t&) const;
        bool operator >  (const raw_t*) const;
        bool operator <= (const fed_t&) const;
        bool operator <= (const dbm_t&) const; // exact
        bool operator <= (const raw_t*) const; // exact
        bool operator >= (const fed_t&) const;
        bool operator >= (const dbm_t&) const;
        bool operator >= (const raw_t*) const;
        
        /// Relation (wrt inclusion, approximate).
        /// @return this (relation) arg.

        relation_t relation(const fed_t& arg) const;
        relation_t relation(const dbm_t& arg) const;
        relation_t relation(const raw_t* arg, cindex_t dim) const;

        /// Specialized relation test: >= arg (approximate).
        bool isSupersetEq(const raw_t* arg, cindex_t dim) const;

        /// Exact (expensive) relations. See comments on dbm_t. eq: equal,
        /// lt: less than, gt: greater than, le: less or equal, ge: greater or equal.
        /// @pre same dimension for eq,lt,le,gt,ge

        bool eq(const fed_t& arg) const;
        bool eq(const dbm_t& arg) const;
        bool eq(const raw_t* arg, cindex_t dim) const;
        bool lt(const fed_t& arg) const;
        bool lt(const dbm_t& arg) const;
        bool lt(const raw_t* arg, cindex_t dim) const;
        bool gt(const fed_t& arg) const;
        bool gt(const dbm_t& arg) const;
        bool gt(const raw_t* arg, cindex_t dim) const;
        bool le(const fed_t& arg) const;
        bool le(const dbm_t& arg) const;
        bool le(const raw_t* arg, cindex_t dim) const;
        bool ge(const fed_t& arg) const;
        bool ge(const dbm_t& arg) const;
        bool ge(const raw_t* arg, cindex_t dim) const;

        relation_t exactRelation(const fed_t& arg) const;
        relation_t exactRelation(const dbm_t& arg) const;
        relation_t exactRelation(const raw_t* arg, cindex_t dim) const;

        /// Set this federation to zero (origin).
        /// @post size() == 1 if dim > 1, 0 otherwise.
        fed_t& setZero();

        /// (re-)initialize the federation with no constraint.
        /// @post size() == 1 if dim > 1, 0 otherwise.
        fed_t& setInit();
        
        /// Convex union of its DBMs.
        fed_t& convexHull();
        
        /** Similar to dbm_t::makeUnbounded but only w.r.t. the future
         * of this DBM, i.e., take max(c[i],max all lower bounds[i])
         * as lower bounds (and keep strictness).
         */
        dbm_t makeUnboundedFrom(const int32_t* c) const;

        /// Widen with respect to a small drift. This is to implement
        /// drift w.r.t. Mani's semantics. The operation consists of
        /// - widening the closed contraints by 1 and make them open,
        /// - make all lower bounds strict,
        /// - return a union with the original federation.
        /// @pre up() was called before, namely this makes sense only
        /// after delay and it is correct w.r.t. the algorithm only
        /// if the upper bounds are removed.
        fed_t& driftWiden();

        /// (Set) union operator (|). Inclusion is checked and the
        /// operation has the effect of reduce() on the argument.
        /// @pre same dimension.
        
        fed_t& operator |= (const fed_t&);
        fed_t& operator |= (const dbm_t&);
        fed_t& operator |= (const raw_t*);

        /// Union of 2 fed_t. @post arg.isEmpty()
        fed_t& unionWith(fed_t& arg);
        fed_t& unionWithC(fed_t arg); // dummy wrapper

        /// Simply add (list concatenation) DBMs to this federation.
        /// @pre same dimension.

        fed_t& add(const fed_t& arg);
        fed_t& add(const dbm_t& arg);
        fed_t& add(const raw_t* arg, cindex_t dim);

        /// Append arg to 'this', @post arg.isEmpty()
        fed_t& append(fed_t& arg);
        fed_t& appendC(fed_t arg); // dummy wrapper
        void append(fdbm_t* arg); // low level

        /// Like append but guarantee where the argument is
        /// inserted (beginning or end).
        fed_t& appendBegin(fed_t &arg);
        fed_t& appendEnd(fed_t &arg);

        /// Combination of appendEnd + incremental mergeReduce.
        fed_t& steal(fed_t &arg);
        fed_t& stealC(fed_t arg); // dummy wrapper

        /// Swap this federation with another.
        void swap(fed_t&);

        /// Convex union operator (+). Every DBM of the federation
        /// is unified with the argument. To get the convex hull of
        /// everything, call this->convexHull() += arg;
        /// @pre same dimension.

        fed_t& operator += (const fed_t&);
        fed_t& operator += (const dbm_t&);
        fed_t& operator += (const raw_t*);

        /// Intersection and constraint operator (&).
        /// @pre same dimension, compatible indices,
        /// and i != j for the constraints.

        fed_t& operator &= (const fed_t&);
        fed_t& operator &= (const dbm_t&);
        fed_t& operator &= (const raw_t*);
        fed_t& operator &= (const constraint_t&);
        fed_t& operator &= (const base::pointer_t<constraint_t>&);
        fed_t& operator &= (const std::vector<constraint_t>&);

        /// (Set) subtraction operator (-).
        /// @pre same dimension.

        fed_t& operator -= (const fed_t&);
        fed_t& operator -= (const dbm_t&);
        fed_t& operator -= (const raw_t*);

        /// Compute (*this -= arg).down(). The interest of this
        /// call is that some subtractions can be avoided if the
        /// following down() negates their effects.

        fed_t& subtractDown(const fed_t&);
        fed_t& subtractDown(const dbm_t&);
        fed_t& subtractDown(const raw_t*);

        /// Methods for constraining: with one or more constraints.
        /// Variants with @param table: indirection table for the indices.
        /// @pre compatible indices, i != j for the constraints, and
        /// table is an cindex_t[getDimension()]. @see dbm_t.

        bool constrain(cindex_t i, int32_t value);
        bool constrain(cindex_t i, cindex_t j, raw_t c);
        bool constrain(cindex_t i, cindex_t j, int32_t b, strictness_t s);
        bool constrain(cindex_t i, cindex_t j, int32_t b, bool isStrict);
        bool constrain(const constraint_t& c);
        bool constrain(const constraint_t *c, size_t n);
        bool constrain(const cindex_t *table, const constraint_t *c, size_t n);
        bool constrain(const cindex_t *table, const base::pointer_t<constraint_t>&);
        bool constrain(const cindex_t *table, const std::vector<constraint_t>&);
        
        /// @return false if there is no intersection with the argument
        /// or true if there *may* be an intersection.
        /// @pre same dimensions.

        bool intersects(const fed_t&) const;
        bool intersects(const dbm_t&) const;
        bool intersects(const raw_t*, cindex_t dim) const;
        
        /// Delay (strongest post-condition) for all the DBMs.
        fed_t& up();

        /// Delay, except for stopped clocks.
        /// @param stopped is a bit array marking which clocks are stopped.
        /// @pre if stopped != NULL then it is a uint32_t[bits2intsize(getDimension())].
        fed_t& upStop(const uint32_t* stopped);

        /// Inverse delay (weakest pre-condition) for all the DBMs.
        fed_t& down();

        /// Similar to upStop but for inverse delay.
        fed_t& downStop(const uint32_t* stopped);

        /// Free clock (unconstraint) for all the DBMs.
        fed_t& freeClock(cindex_t clock);

        /// Free upper or lower bounds only for a particular clock or
        /// for all clocks. @pre 0 < clock < getDimension()
        /// @return this. @see dbm.h

        fed_t& freeUp(cindex_t clock);
        fed_t& freeDown(cindex_t clock);
        fed_t& freeAllUp();
        fed_t& freeAllDown();

        /// Update methods where x & y are clocks, v an integer value.
        /// x := v     -> updateValue
        /// x := y     -> updateClock
        /// x := x + v -> updateIncrement
        /// x := y + v -> update
        /// @pre 0 < x and y < getDimension(), v < infinity, and v is
        /// s.t. the clocks stay positive.

        void updateValue(cindex_t x, int32_t v);
        void updateClock(cindex_t x, cindex_t y);
        void updateIncrement(cindex_t x, int32_t v);
        void update(cindex_t x, cindex_t y, int32_t v);
        
        /// @return true if this federation satisfies a constraint c_ij.
        /// @pre i != j, i and j < getDimension()
        
        bool satisfies(cindex_t i, cindex_t j, raw_t c) const;
        bool satisfies(const constraint_t& c) const;
        bool operator && (const constraint_t& c) const;

        /// @return true if this federation has points that can delay infinitely.
        bool isUnbounded() const;
        
        /// @return the part of the federation that is unbounded (maybe empty).
        fed_t getUnbounded() const;

        /// @return the part of the federation that is bounded (maybe empty).
        fed_t getBounded() const;

        /// Make upper or lower finite bounds non strict for all the DBMs.
        /// @see dbm.h.
        /// @return this.
        fed_t& relaxUp();
        fed_t& relaxDown();

        /// Make lower bounds strict.
        /// @return this.
        fed_t& tightenDown();

        /// Make upper bounds strict.
        /// @return this.
        fed_t& tightenUp();

        /// Similar for all bounds of a particular clock for all the DBMs.
        /// @see dbm.h. Special for clock == 0:
        /// relaxUp(0) = relaxDown() and relaxDown(0) = relaxUp().
        fed_t& relaxUpClock(cindex_t clock);
        fed_t& relaxDownClock(cindex_t clock);

        /// Make all constraints (except infinity) non strict for all the DBMs.
        fed_t& relaxAll();

        /// Remove redundant DBMs (if included in ONE other DBM).
        /// @post side effect: all copies of this fed_t are affected so
        /// do not mix iterators and reduce().
        /// @return this.
        fed_t& reduce();

        /// This method is useful only for experiments.
        fed_t& noReduce() { return *this; }

        /// Remove redundant DBMs (if included in the UNION of the other DBMs).
        /// @post same side effect as reduce().
        /// @return this.
        fed_t& expensiveReduce();

        /// Try to merge DBMs by pairs.
        /// @post same side effect as reduce().
        /// @param skip is the number of DBMs to skip for the
        /// reduction, useful for incremental reductions.
        /// @param level is how expensive it can be: 0=default,
        /// 1=more expensive (more effective), 2=much more expensive
        /// (hopefully even more effective).
        /// @return this.
        fed_t& mergeReduce(size_t skip = 0, int level = 0);

        /// The mergeReduce implementation uses a heuristic
        /// to avoid computing (possibly) useless subtractions.
        /// It is activated by default but it may miss reductions.
        /// If you want to spend more time but get a better result
        /// set it to false.
        static void heuristicMergeReduce(bool active);

        /// Use a heuristic to recompute parts of the federation as
        /// part=convexHull(part)-(convexHull(part)-part)
        /// @return this.
        fed_t& convexReduce();

        /// Try to replace this by convexHull(this)-(convexHull(this)-this)
        /// @return this.
        fed_t& expensiveConvexReduce();

        /// Find partitions in the federation and reduce them separately.
        /// @return this.
        fed_t& partitionReduce();

        /// @return true if a point (discrete or "real") is included
        /// in this federation (ie in one of its DBMs).
        /// @pre same dimension.

        bool contains(const IntValuation& point) const;
        bool contains(const int32_t* point, cindex_t dim) const;
        bool contains(const DoubleValuation& point) const;
        bool contains(const double* point, cindex_t dim) const;

        /** @return the 'almost max' possible delay backward from
         * a point while still staying inside the federation. It
         * is 'almost max' since we want a discrete value, which
         * cannot me the max when we have strict constraints.
         * The precision is 0.5. 0.0 may be returned if the point
         * is too close to a border.
         * @param point: the point to go backward from.
         * @pre dim = getDimension() && contains(point)
         */
        double possibleBackDelay(const DoubleValuation& point) const;
        double possibleBackDelay(const double* point, cindex_t dim) const;

        /** Compute the 'almost min' necessary delay from
         * a point to enter this federation. If this point
         * is already contained in this federation, 0.0 is
         * returned. The result is 'almost min' since we
         * want a discrete value, which is not possible in
         * case of strict constraints.
         * @pre dim == getDimension() and point[0] == 0.0
         * otherwise the computation will not work.
         * @return true if it is possible to reach this federation
         * by delaying, or false if this federation is empty or it
         * is not possible to reach it by delaying.
         * The delay is written in t.
         * @param minVal,minStrict: another (optional) output
         * is provided in the form of a delay value and a
         * flag telling if the delay is strict, e.g.,
         * wait >= 2.1 or wait > 2.1.
         * @pre minVal and minStrict are both NULL or non NULL.
         */
        bool getMinDelay(const DoubleValuation& point, double* t,
                         double *minVal = NULL, bool *minStrict = NULL,
                         const uint32_t* stopped = NULL) const;
        bool getMinDelay(const double* point, cindex_t dim, double* t,
                         double *minVal = NULL, bool *minStrict = NULL,
                         const uint32_t* stopped = NULL) const;

        /** Similarly for the past.
         *  The returned value (in t) is <= max, where max.
         *  @pre max > 0 otherwise this is meaningless.
         */
        bool getMaxBackDelay(const DoubleValuation& point, double* t, double max) const;
        bool getMaxBackDelay(const double* point, cindex_t dim, double* t, double max) const;

        /** Compute the approximate interval delay from
         * a point to enter this federation and to stay
         * inside continuously.
         * @return true if it is possible to reach this federation
         * by delaying, false otherwise.
         * @param min and max give the interval, max can be
         * HUGE_VAL to mean infinity.
         */
        bool getDelay(const DoubleValuation& point, double* min, double* max,
                      double *minVal = NULL, bool *minStrict = NULL,
                      double *maxVal = NULL, bool *maxStrict = NULL,
                      const uint32_t* stopped = NULL) const;
        bool getDelay(const double* point, cindex_t dim, double* min, double* max,
                      double *minVal = NULL, bool *minStrict = NULL,
                      double *maxVal = NULL, bool *maxStrict = NULL,
                      const uint32_t* stopped = NULL) const;

        bool isConstrainedBy(cindex_t, cindex_t, raw_t) const;

        /// Extrapolations: @see dbm_##method functions in dbm.h.
        /// @pre max, lower, and upper are int32_t[getDimension()]

        void extrapolateMaxBounds(const int32_t *max);
        void diagonalExtrapolateMaxBounds(const int32_t *max);
        void extrapolateLUBounds(const int32_t *lower, const int32_t *upper);
        void diagonalExtrapolateLUBounds(const int32_t *lower, const int32_t *upper);
        
        /** "Split-extrapolation". Split the DBMs with the diagonal
         * constraints given in argument, apply extrapolateMaxBounds
         * on the result, and make sure that the resulting DBMs are
         * still constrained by these diagonals.
         * @param begin .. end give the diagonal constraints for
         * splitting (from begin (inclusive) to end (exclusive)).
         * @param max is the array of maximal bounds.
         */
        void splitExtrapolate(const constraint_t *begin, const constraint_t *end,
                              const int32_t *max);
        
        /** Resize all the DBMs of this federation, @see dbm_t.
         * @see dbm_shrinkExpand in dbm.h.
         * @param bitSrc,bitDst,bitSize: bit strings of (int) size bitSize
         * that mark the source and destination active clocks.
         * @param table: redirection table to write.
         * @pre bitSrc & bitDst are uint32_t[bitSize] and
         * table is a uint32_t[32*bitSize]
         * @post the indirection table is written.
         */
        void resize(const uint32_t *bitSrc, const uint32_t *bitDst,
                    size_t bitSize, cindex_t *table);

        /** Resize and change clocks of all the DBMs of this federation.
         * The updated DBMs will have its clocks i coming from target[i]
         * in the original DBM.
         * @param target is the table that says where to put the current
         * clocks in the target DBMs. If target[i] = ~0 then a new free
         * clock is inserted.
         * @pre newDim > 0, target is a cindex_t[newDim], and
         * for all i < newDim, target[i] < getDimension().
         */
        void changeClocks(const cindex_t *target, cindex_t newDim);

        /// Swap clocks x and y.
        void swapClocks(cindex_t x, cindex_t y);

        /** Get a clock valuation and change only the clocks
         * that are marked free. The point will belong to one
         * DBM of this federation, it is unspecified which one.
         * @param cval: clock valuation to write.
         * @param freeC: free clocks to write, if freeC == NULL, then
         * all clocks are considered free.
         * @return cval
         * @throw std::out_of_range if the generation fails
         * if isEmpty() or cval too constrained.
         * @post if freeC != NULL, forall i < dim: freeC[i] = false
         * @pre same dimension.
         */
        DoubleValuation& getValuation(DoubleValuation& cval, bool *freeC = NULL) const 
            throw(std::out_of_range);

        /** predt operation: temporal predecessor of this federation
         * avoiding 'bad'. The extra argument 'restrict' is to apply
         * intersection (thus restricting) to the result, which *may*
         * improve the algorithm. The argument is optional and its
         * dimension must match getDimension().
         * @post the points in the resulting federation may delay
         * (and stay in the result) until they belong to this federation
         * without entering bad.
         * @pre same dimension.
         */
        fed_t& predt(const fed_t& bad, const raw_t* restrict = NULL);
        fed_t& predt(const dbm_t& bad, const raw_t* restrict = NULL);
        fed_t& predt(const raw_t* bad, cindex_t dim, const raw_t* restrict = NULL);

        /// @return true if this fed_t is included in predt(good,bad)
        /// This test may terminate earlier than calling le(predt(good,bad))
        /// because predt does not have to be computed in full sometimes.
        bool isIncludedInPredt(const fed_t& good, const fed_t& bad) const;

        /// Identify test to know if this federation has a specific DBM.
        /// If (dbm_t) arg is empty, then it is trivially true (if same dimension).
        bool has(const dbm_t& arg) const;
        bool has(const raw_t* arg, cindex_t dim) const;

        /// Similar but test with exact same dbm_t. Note: an empty federation
        /// is an empty list and an empty DBM is an empty zone. Both are
        /// compatible since they contain no point but empty_fed.hasSame(empty_dbm)
        /// will return false even if the dimensions are the same since an empty
        /// fed_t contains no dbm_t at all.
        bool hasSame(const dbm_t& arg) const;

        /** Remove the DBMs that are included in DBMs of arg (pair-wise
         * inclusion checking). WARNING: If sameAs(arg) then you will
         * empty this federation *and* the argument.
         * @pre same dimension.
         * @return !(arg <= *this) if arg is a dbm_t.
         */
        void removeIncludedIn(const fed_t& arg);
        bool removeIncludedIn(const dbm_t& arg);
        bool removeIncludedIn(const raw_t* arg, cindex_t dim);
        
        /// Special constructor to copy the result of a pending operation.
        /// @param op: clock operation.
        fed_t(const ClockOperation<fed_t>& op);

        /** Overload of operator (): () or (i,j) make no sense here.
         * fed_t::(i) -> clock access for clock i.
         * @see ClockOperation for more details.
         * @pre clk > 0 except for clock constraint tests
         * and clk < getDimension().
         */
        ClockOperation<fed_t> operator () (cindex_t clk);

        /** @return (this-arg).isEmpty() but it is able to
         * stop the subtraction early if it is not empty and
         * it does not modify itself.
         * @pre dbm_isValid(arg, dim) and dim == getDimension()
         */
        bool isSubtractionEmpty(const raw_t* arg, cindex_t dim) const;
        bool isSubtractionEmpty(const dbm_t& arg) const;
        bool isSubtractionEmpty(const fed_t& arg) const;
        static bool isSubtractionEmpty(const raw_t* dbm, cindex_t dim, const fed_t& fed);

        /// Subtract DBM arg1 - DBM arg2 wrapper functions.
        static fed_t subtract(const raw_t* arg1, const raw_t* arg2, cindex_t dim);
        static fed_t subtract(const dbm_t& arg1, const raw_t* arg2);
        static fed_t subtract(const dbm_t& arg1, const dbm_t& arg2);

        /** @return a new fed_t made of all the (weak) lower bounds of the original
         * fed_t (i.e. no diagonals) but without overlapping.
         */
        fed_t toLowerBounds() const;

        /// Similar but for upper bounds.
        fed_t toUpperBounds() const;

        /** @return the max upper bound (raw) of a clock.
         */
        raw_t getMaxUpper(cindex_t) const;
        
        /** @return the max lower bound (raw) of a clock.
         */
        raw_t getMaxLower(cindex_t) const;

        /** Clean-up the federation of its empty dbm_t.
         * Normally this is never needed except if the mutable
         * iterator is used and makes some dbm_t empty.
         */
        void removeEmpty();

        /** @return true if this fed_t has an empty dbm_t in its list.
         * Normally this should never occur, unless you play with dbm_t
         * manually with the iterator. This is used mainly for testing.
         */
        bool hasEmpty() const;

        /// Mutable iterator -> iterate though dbm_t
        class iterator
        {
        public:
            /// End of list.
            static const fdbm_t *ENDF;

            /// Special constructor to end iterations.
            iterator();

            /// Initialize the iterator of a federation.
            /// @param fed: federation.
            iterator(ifed_t* fed);

            /// Dereference to dbm_t, @pre !null()
            dbm_t& operator *() const;

            /// Dereference to dbm_t*, @pre !null()
            dbm_t* operator ->() const;
            
            /// Mutable access to the matrix as for fed_t, @pre !null()
            raw_t* operator () () const;
            raw_t operator () (cindex_t i, cindex_t j) const;

            /// Increment iterator, @pre !null()
            iterator& operator ++();

            /// Test if there are DBMs left on the list.
            bool null() const;

            /// @return true if there is another DBM after, @pre !null()
            bool hasNext() const;

            /// Equality test of the internal fdbm_t*
            bool operator == (const iterator& arg) const;
            bool operator != (const iterator& arg) const;

            /// Remove (and deallocate) current dbm_t.
            void remove();

            /// Remove (and deallocate) current empty dbm_t.
            void removeEmpty();

            /// Extract the current DBM from the list.
            /// The result->getNext() points to the rest of the list.
            fdbm_t* extract();

            /// Insert a DBM in the list at the current position.
            void insert(fdbm_t* dbm);

        private:
            fdbm_t **fdbm; /// list of DBMs
            ifed_t *ifed;  /// to update the size
        };

        /// Const iterator -> iterate though dbm_t
        class const_iterator
        {
        public:
            /// Const iterator for end of list.
            static const const_iterator ENDI;
            
            /// Constructor: @param fed: federation.
            const_iterator(const fdbm_t* fed);
            const_iterator(const fed_t& fed);
            const_iterator();

            /// Dereference to dbm_t
            const dbm_t& operator *() const;

            /// Dereference to dbm_t*, @pre !null()
            const dbm_t* operator ->() const;

            /// Access to the matrix as for fed_t
            const raw_t* operator () () const;
            raw_t operator () (cindex_t i, cindex_t j) const;

            /// Increment iterator, @pre !null()
            const_iterator& operator ++();

            /// Test if there are DBMs left on the list.
            bool null() const;

            /// @return true if there is another DBM after, @pre !null()
            bool hasNext() const;

            /// Equality test of the internal fdbm_t*
            bool operator == (const const_iterator& arg) const;
            bool operator != (const const_iterator& arg) const;

        private:
            const fdbm_t *fdbm; /// list of DBMs
        };

        /// Access to iterators. Limitation: you cannot modify the original
        /// fed_t object otherwise the iterator will be invalidated. In
        /// addition, you cannot copy the original either if the non const
        /// iterator is used.

        const_iterator begin() const;
        const const_iterator end() const;
        iterator beginMutable();
        const iterator endMutable() const;

        // Standard erase method for the iterator.
        iterator erase(iterator& iter);

        /** Dump its list of ifed_t and reload them. This is
         * useful for testing mainly but can be extended later
         * for saving or loading a fed_t.
         * @param mem: a ifed_t[size()]
         * @post isEmpty()
         * @return size()
         */
        size_t write(fdbm_t** mem);

        /** Symmetric: read.
         * @param fed,size: a ifed_t[size]
         * @post the ifed list is re-linked and belongs to this fed_t.
         */
        void read(fdbm_t** fed, size_t size);

        /// @return its first dbm_t, @pre size() > 0
        const dbm_t& const_dbmt() const;

        /// @return its dbm_t, @pre size() >= 1
        /// This is a dangerous access, whatever you do
        /// with this dbm_t *never* have it empty.
        dbm_t& dbmt();

        /// Remove a dbm_t from this fed_t. The match uses dbm_t::sameAs(..)
        /// @return true if arg was removed, false otherwise.
        bool removeThisDBM(const dbm_t &dbm);

        /// Ensure this ifed_t is mutable.
        void setMutable();

        /// @return ifedPtr with basic checks.
        ifed_t* ifed();
        const ifed_t* ifed() const;

    private:
        // You are not supposed to read this part :)

        // Internal constructor.
        fed_t(ifed_t* ifed);

        /// Call-backs to ifed_t.
        bool isMutable() const;
        bool isOK() const;
        void incRef() const;
        void decRef() const;
        void decRefImmutable() const;

        /// Convert its linked list to an array.
        /// @pre ar is of size size()
        void toArray(const raw_t** ar) const;

        /// Internal subtraction implemention (*this - arg).
        /// @pre !isEmpty() && isMutable()
        void ptr_subtract(const raw_t* arg, cindex_t dim);
        
#ifdef ENABLE_STORE_MINGRAPH
        /// Internal subtraction implemention (*this - arg).
        /// @pre !isEmpty() && isMutable() && !arg.isEmpty()
        void ptr_subtract(const dbm_t& arg);
#endif

        /// Similarly with a DBM. @pre isPointer()
        relation_t ptr_relation(const raw_t* arg, cindex_t dim) const;

        /// @return true if this DBM can be ignored in subtractDown.
        bool canSkipSubtract(const raw_t*, cindex_t) const;

        ifed_t *ifedPtr;
    };


    /**********************************************
     * Operator overloads with const arguments.
     * + : convex union (result = always DBM)
     * & : intersection/constraining
     * | : set union
     * - : set subtraction
     * @pre same dimension for the arguments!
     * @see dbm_t and fed_t
     **********************************************/
    
    dbm_t operator + (const dbm_t& a, const raw_t* b);
    dbm_t operator + (const fed_t& a, const raw_t* b);
    dbm_t operator + (const dbm_t& a, const dbm_t& b);
    dbm_t operator + (const dbm_t& a, const fed_t& b);
    dbm_t operator + (const fed_t& a, const dbm_t& b);
    dbm_t operator + (const fed_t& a, const fed_t& b);

    dbm_t operator & (const dbm_t& a, const raw_t* b);
    fed_t operator & (const fed_t& a, const raw_t* b);
    dbm_t operator & (const dbm_t& a, const dbm_t& b);
    fed_t operator & (const dbm_t& a, const fed_t& b);
    fed_t operator & (const fed_t& a, const dbm_t& b);
    fed_t operator & (const fed_t& a, const fed_t& b);

    dbm_t operator & (const dbm_t& a, const constraint_t& c);
    dbm_t operator & (const constraint_t& c, const dbm_t& a);
    fed_t operator & (const fed_t& a, const constraint_t& c);
    fed_t operator & (const constraint_t& c, const fed_t& a);

    dbm_t operator & (const dbm_t& a, const base::pointer_t<constraint_t>& c);
    dbm_t operator & (const base::pointer_t<constraint_t>& c, const dbm_t& a);
    fed_t operator & (const fed_t& a, const base::pointer_t<constraint_t>& c);
    fed_t operator & (const base::pointer_t<constraint_t>& c, const fed_t& a);

    dbm_t operator & (const dbm_t& a, const std::vector<constraint_t>& vec);
    dbm_t operator & (const std::vector<constraint_t>& vec, const dbm_t& a);
    fed_t operator & (const fed_t& a, const std::vector<constraint_t>& vec);
    fed_t operator & (const std::vector<constraint_t>& vec, const fed_t& a);

    fed_t operator | (const dbm_t& a, const raw_t* b);
    fed_t operator | (const fed_t& a, const raw_t* b);
    fed_t operator | (const dbm_t& a, const dbm_t& b);
    fed_t operator | (const fed_t& a, const dbm_t& b);
    fed_t operator | (const dbm_t& a, const fed_t& b);
    fed_t operator | (const fed_t& a, const fed_t& b);

    fed_t operator - (const dbm_t& a, const raw_t* b);
    fed_t operator - (const fed_t& a, const raw_t* b);
    fed_t operator - (const dbm_t& a, const dbm_t& b);
    fed_t operator - (const fed_t& a, const dbm_t& b);
    fed_t operator - (const dbm_t& a, const fed_t& b);
    fed_t operator - (const fed_t& a, const fed_t& b);

    fed_t operator ! (const dbm_t&);
    fed_t operator ! (const fed_t&);

    /// Create zero or init dbm_t with a given dimension.

    dbm_t zero(cindex_t dim);
    dbm_t init(cindex_t dim);

    /// Straight-forward wrapper functions:
    /// @return dbm_t(arg).function(other arguments)

    dbm_t up(const dbm_t& arg);
    dbm_t down(const dbm_t& arg);
    dbm_t freeClock(const dbm_t& arg, cindex_t clock);
    dbm_t freeUp(const dbm_t& arg, cindex_t k);
    dbm_t freeDown(const dbm_t& arg, cindex_t k);
    dbm_t freeAllUp(const dbm_t& arg);
    dbm_t freeAllDown(const dbm_t& arg);
    dbm_t relaxUp(const dbm_t& arg);
    dbm_t relaxDown(const dbm_t& arg);
    dbm_t relaxUpClock(const dbm_t& arg, cindex_t k);
    dbm_t relaxDownClock(const dbm_t& arg, cindex_t k);

    /// Straight-forward wrapper functions:
    /// @return fed_t(arg).function(other arguments)

    fed_t up(const fed_t& arg);
    fed_t down(const fed_t& arg);
    fed_t freeClock(const fed_t& arg, cindex_t x);
    fed_t freeUp(const fed_t& arg, cindex_t k);
    fed_t freeDown(const fed_t& arg, cindex_t x);
    fed_t freeAllUp(const fed_t& arg);
    fed_t freeAllDown(const fed_t& arg);
    fed_t relaxUp(const fed_t& arg);
    fed_t relaxDown(const fed_t& arg);
    fed_t relaxUpClock(const fed_t& arg, cindex_t k);
    fed_t relaxDownClock(const fed_t& arg, cindex_t k);
    fed_t reduce(const fed_t& arg);
    fed_t expensiveReduce(const fed_t& arg);
    fed_t predt(const fed_t& good, const fed_t& bad);
    fed_t predt(const fed_t& good, const dbm_t& bad);
    fed_t predt(const fed_t& good, const raw_t* bad, cindex_t dim);
    fed_t predt(const dbm_t& good, const fed_t& bad);
    fed_t predt(const dbm_t& good, const dbm_t& bad);
    fed_t predt(const dbm_t& good, const raw_t* bad, cindex_t dim);
    fed_t predt(const raw_t* good, cindex_t dim, const fed_t& bad);
    fed_t predt(const raw_t* good, cindex_t dim, const dbm_t& bad);
    fed_t predt(const raw_t* good, const raw_t* bad, cindex_t dim);

    /// Clean-up function useful for testing. Deallocate internally
    /// allocated DBMs that are currently free.
    void cleanUp();
}

#include "dbm/inline_fed.h"

#endif // INCLUDE_DBM_FED_H
