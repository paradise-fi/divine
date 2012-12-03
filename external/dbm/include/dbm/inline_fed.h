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
// Filename : inline_fed.h (dbm)
//
// Internal classes and inlined implementation of fed.h
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: inline_fed.h,v 1.34 2005/10/24 18:34:09 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#if defined(INCLUDE_DBM_INLINE_FED_H) || !defined(INCLUDE_DBM_FED_H)
#error "dbm/inline_fed.h wrongly included!"
#endif
#define INCLUDE_DBM_INLINE_FED_H

#include "base/exceptions.h"
#include "base/ItemAllocator.h"
#include "base/stats.h"
#include "hash/tables.h"
#include "debug/macros.h"

/** @file
 * This file contains internal classes and inlined implementation of fed.h.
 */
namespace dbm
{
    /// @return true if ptr is a non null pointer, mainly for debugging.
    static inline bool isPointer(const void *ptr)
    {
        return (((uintptr_t)ptr) & 3) == 0 && ptr != NULL;
    }

    /********************************************************
     * DBMTable : hash table of idbm_t to internalize them. *
     ********************************************************/

    class DBMTable : public hash::TableDouble<idbm_t>
    {
    public:
        /// Default constructor, @see table.h
        DBMTable(): hash::TableDouble<idbm_t>(dbm_t::MAX_DIM_POWER, false){}

#ifdef ENABLE_MONITOR
        /// this hash table should be empty now unless there have been leaks
        ~DBMTable() {
            if (nbBuckets) {
                std::cerr << RED(BOLD) << nbBuckets
                          << (nbBuckets > 1 ? " DBMs are" : " DBM is")
                          << " left in the internal hash table!"NORMAL"\n";
            }
        }
#endif
    };
    
    /// One instance for the DBM table.
    extern DBMTable dbm_table;

    /****************************************************
     * Allocation of DBMs: by new or internal allocator *
     ****************************************************/

#ifdef ENABLE_DBM_NEW

    /// Clean-up function does nothing
    static inline void cleanUp() {}

    /** Allocate memory with new.
     * @param dim: dimension of the idbm_t to allocate.
     */
    static inline
    void* dbm_new(cindex_t dim)
    {
#ifdef ENABLE_STORE_MINGRAPH
        return new int32_t[intsizeof(idbm_t)+dim*dim+bits2intsize(dim*dim)];
#else
        return new int32_t[intsizeof(idbm_t)+dim*dim];
#endif
    }

    /** Deallocate memory as allocated by dbm_new.
     * @param dbm: idbm_t to deallocate.
     * @pre dbm != NULL
     */
    static inline
    void dbm_delete(idbm_t *dbm)
    {
        assert(dbm);
        delete [] reinterpret_cast<int32_t*>(dbm);
    }

#else // ifndef ENABLE_DBM_NEW

    /** Allocate memory with a local allocator.
     * @param dim: dimension of the idbm_t to allocate.
     * DON'T INLINE because array_t will expand
     * every call too badly.
     */
    void* dbm_new(cindex_t dim);

    /** Deallocate memory as allocated by dbm_new.
     * @param dbm: idbm_t to deallocate.
     * @pre dbm != NULL
     * DON'T INLINE because array_t will expand
     * every call too badly.
     */
    void dbm_delete(idbm_t *dbm);

#endif // ENABLE_DBM_NEW

    /**************************************************
     * idbm_t: internal DBM -- to be used in DBMTable *
     **************************************************/
  
    class idbm_t : public hash::TableDouble<idbm_t>::Bucket_t
    {
    public:
        /** Maximal dimension == 2^15-1
         * because (2^15)^2 == 2^30 constraints == 2^32 bytes
         * and we are in big big trouble then.
         * MAX_DIM is the maximum dimension *and* the access mask.
         * HASH_MASK contains the remaining bits for the hash value
         * HASHED_BIT mark if this DBM is in a hash tabled (hashed).
         */
        enum
        {
            HASHED_BIT = (1 << 31),
            HASH_MASK = ~(HASHED_BIT | dbm_t::MAX_DIM),
            DIM_MASK = dbm_t::MAX_DIM
        };

        /// @return dimension of this DBM.
        cindex_t getDimension() const {
            return info & DIM_MASK;
        }

        /// @return a freshly computed hash value
        uint32_t hash(uint32_t seed = 0) const {
            uint32_t dim = getDimension();
            return hash_computeI32(matrix, dim*dim, seed);
        }

        /// @return true if this DBM is in a hash table
        bool isHashed() const {
            return (info & HASHED_BIT) != 0;
        }

#ifdef ENABLE_STORE_MINGRAPH
        /// Invalidate its mingraph.
        void invalidate() { minSize = ~0; }

        /// Compute/update the mingraph.
        const uint32_t *getMinGraph(size_t *size);

        /// Test if the mingraph is valid.
        bool isValid() const { return minSize != (size_t) ~0; }

        /// Copy its mingraph.
        /// @pre isValid() && !arg->isValid() && same DBMs.
        void copyMinGraphTo(idbm_t *arg);
#endif

        /// @return true if this dbm can be modified.
        bool isMutable() const {
            assert(refCounter > 0);
            return refCounter == 1 && !isHashed();
        }

        /// Check if this dimPtr is mutable and try to make it so cheaply.
        bool tryMutable() {
            assert(refCounter > 0);
            if (refCounter > 1) return false;
#ifdef ENABLE_STORE_MINGRAPH
            invalidate(); // We're going to change this DBM.
#endif
            if (isHashed()) unhash();
            return true;
        }

        /// Unhash itself.
        /// @pre refCounter == 1 && isHashed()
        void unhash() {
            assert(refCounter == 1 && isHashed());
            dbm_table.remove(this);
            unmarkHashed();
        }

        /// @return recomputed hash value for this DBM
        /// and save it partially.
        uint32_t updateHash(uint32_t seed = 0) {
            uint32_t hashValue = hash(seed);
            info = (info & ~HASH_MASK) | (hashValue & HASH_MASK);
            return hashValue;
        }
        
        /// Mark 'this' as hashed
        void markHashed() {
            assert(!isHashed());
            info |= HASHED_BIT;
        }
        
        /// Unmark 'this' as hashed
        void unmarkHashed() {
            assert(isHashed());
            info &= ~HASHED_BIT;
        }

        /// Increment reference counter.
        void incRef() { refCounter++; }

        /// Decrement reference counter.
        /// @post 'this' may be deallocated.
        void decRef() {
            assert(refCounter > 0);
            if (--refCounter == 0) remove();
        }

        /// Simple decRef without remove() @pre refCounter > 1
        void decRefImmutable() {
            assert(refCounter > 1);
            refCounter--;
        }

        /// Simple remove for mutable idbm_t. @pre isMutable()
        void removeMutable() {
            assert(isMutable());
            dbm_delete(this);
        }
        
        /** Deallocate this idbm_t.
         * @pre refCounter == 0
         * Not inlined since the call is present very often
         * for every decRef, where decRef is called for
         * garbage collection.
         */
        void remove();

        /// @return writable DBM matrix, @pre isMutable()
        raw_t* dbm() {
            assert(isMutable());
            return matrix;
        }

        /// @return read-only DBM matrix.
        const raw_t* const_dbm() const {
            return matrix;
        }

        /// @return DBM matrix without pre-condition, careful...
        raw_t* getMatrix() {
            return matrix;
        }

        /// @return newly allocated idbm_t, @param dim: DBM dimension.
        static idbm_t* create(cindex_t dim) {
            return new (dbm_new(dim)) idbm_t(dim); // placement constructor
        }

        /// @return newly allocated idbm_t, @param arg: original to copy.
        static idbm_t* create(const idbm_t& arg) {
            return new (dbm_new(arg.getDimension())) idbm_t(arg); // placement constructor
        }

        /** Constructor: use placement constructor
         * to instantiate.
         * @param dim: dimension of the DBM.
         * @pre dim < 2^16, reasonable since such a
         * DBM would have 2^32 elements and we cannot
         * use a single such DBM.
         * @post DBM is not initialized!
         */
        idbm_t(cindex_t dim) {
            assert(dim > 0 && dim <= DIM_MASK);
            info = dim;
            refCounter = 1;
#ifdef ENABLE_STORE_MINGRAPH
            invalidate();
#endif
        }
        
        /** Constructor by copy: useful to get a mutable copy
         * of this DBM.
         * @param original: DBM to copy.
         */
        idbm_t(const idbm_t& original) {
            info = original.getDimension();
            refCounter = 1;
#ifdef ENABLE_STORE_MINGRAPH
            invalidate();
#endif
            dbm_copy(matrix, original.matrix, info);
        }

    private:
        /// Must never be called
        ~idbm_t() {
            throw RuntimeException("~idbm_t() called!");
        }

        /* Inherited variables from parent class:
         * idbm_t **previous, *next: for collision list of
         * the internal hash table.
         * uint32_t info: special coding is as follows
         * info & 0x80000000 = mutable bit = is not hashed
         * info & 0x7fff8000 = higher bits of hash value
         * info & 0x00007fff = dimension (default DBM_MAX_DIM)
         */
        uint32_t refCounter; //< reference counter
#ifdef ENABLE_STORE_MINGRAPH
        size_t minSize;
#endif
        raw_t matrix[];      //< DBM matrix
    };

    /*********************************
     * Allocation of fdbm_t & ifed_t *
     *********************************/

    /// Use a struct here to instantiate an ItemAllocator (and
    /// cheat gravely with the allocator). fdbm_t has a dbm_t as
    /// member, which implies a constructor, which bugs us very much.
    struct alloc_fdbm_t
    {
        ifed_t *next; //< list of DBMs for the federation
        idbm_t *idbm; //< DBM itself
    };

    /// Similarly for ifed_t
    struct alloc_ifed_t
    {
        size_t fedSize;
        fdbm_t* fhead;
        uint32_t refCounter;
        cindex_t dim;
    };

    /// Allocator instance.
    extern base::ItemAllocator<alloc_fdbm_t> fdbm_allocator;
    extern base::ItemAllocator<alloc_ifed_t> ifed_allocator;

    /********************************************************
     * Federation of DBMs: list containing individual DBMs. *
     ********************************************************/

    class fdbm_t
    {
    public:
        /// Copy a DBM into a newly created fdbm_t.
        /// @param adbm: the DBM to copy.
        /// @param nxt: list of DBMs to append.
        static fdbm_t* create(const raw_t* adbm, cindex_t dim, fdbm_t* nxt = NULL) {
            fdbm_t* fdbm = create(nxt);
            fdbm->idbm.newCopy(adbm, dim);
            return fdbm;
        }
        static fdbm_t* create(const dbm_t& adbm, fdbm_t* nxt = NULL) {
            fdbm_t* fdbm = create(nxt);
            fdbm->idbm.newCopy(adbm);
            return fdbm;
        }

        /// Copy start and append end to the copy.
        static fdbm_t* copy(const fdbm_t* start, fdbm_t* end = NULL);

        /// Wrapper methods.
        fdbm_t* copy()         const { return copy(this); }
        bool    isEmpty()      const { return idbm.isEmpty(); }
        cindex_t getDimension() const { return idbm.getDimension(); }

        /// Remove the list starting at fhead.
        static void removeAll(fdbm_t *fhead);

        /// Remove this fdbm_t and its DBM.
        void remove() {
            idbm.nil();
            fdbm_allocator.deallocate(reinterpret_cast<alloc_fdbm_t*>(this));
        }

        /// Remove this fdbm_t with @pre isEmpty()
        void removeEmpty() {
            assert(isEmpty());
            fdbm_allocator.deallocate(reinterpret_cast<alloc_fdbm_t*>(this));
        }

        /// Compute list size.
        size_t size() const {
            const fdbm_t* f = this;
            size_t s = 0;
            do { f = f->next; ++s; } while(f);
            return s;
        }

        /// @return the internal DBM.
        const dbm_t& const_dbmt() const { return idbm; }
        dbm_t& dbmt() { return idbm; }

        /// Unchecked access.
        raw_t* getMatrix() {
            return idbm.idbmt()->getMatrix();
        }

        /// @return start appended with end.
        static fdbm_t* append(fdbm_t *start, fdbm_t *end);

        /// @return next for iterations.
        fdbm_t** getNextMutable() { return &next; }
        const fdbm_t* getNext() const { return next; }
        fdbm_t* getNext() { return next; }

        /// Test its next pointer.
        bool hasNext(fdbm_t** nxt) const { return &next == nxt; }

        /// Change next for iterations.
        void setNext(fdbm_t* nxt) { next = nxt; }

        /// Remove this and return next;
        fdbm_t* removeAndNext() {
            fdbm_t *nxt = next;
            remove();
            return nxt;
        }
        /// Remove this and return next;
        fdbm_t* removeEmptyAndNext() {
            fdbm_t *nxt = next;
            removeEmpty();
            return nxt;
        }

    private:
        /// Creation of fdbm_t using an allocator.
        static fdbm_t* create(fdbm_t* nxt = NULL);

        /// Must never be called
        ~fdbm_t() {
            throw RuntimeException("~fdbm_t called!");
        }

        fdbm_t *next; //< next DBM in the list.
        dbm_t idbm;   //< (internal) DBM.
    };

    /***************************************
     * ifed_t: head of the federation list *
     ***************************************/
    
    /// Simple list of DBMs for light-weight computations.
    class dbmlist_t
    {
    public:
        dbmlist_t()
            : fedSize(0), fhead(NULL) {}
        dbmlist_t(size_t size, fdbm_t *flist)
            : fedSize(size), fhead(flist) {}
        dbmlist_t(const raw_t* arg, cindex_t dim)
            : fedSize(1), fhead(fdbm_t::create(arg, dim)) {}
        dbmlist_t(const dbm_t& arg)
            : fedSize(1), fhead(fdbm_t::create(arg)) {}

        /// Append a list of fdbm_t.
        dbmlist_t& append(dbmlist_t& arg) {
            fhead = fedSize > arg.fedSize
                ? fdbm_t::append(arg.fhead, fhead)
                : fdbm_t::append(fhead, arg.fhead);
            fedSize += arg.fedSize;
            return *this;
        }
        dbmlist_t& appendBegin(dbmlist_t& arg) {
            fhead = fdbm_t::append(arg.fhead, fhead);
            fedSize += arg.fedSize;
            return *this;
        }
        dbmlist_t& appendEnd(dbmlist_t& arg) {
            fhead = fdbm_t::append(fhead, arg.fhead);
            fedSize += arg.fedSize;
            return *this;
        }
        /// Append just one fdbm_t, not the whole list!
        dbmlist_t& append(fdbm_t* arg) {
            assert(arg);
            arg->setNext(fhead);
            fhead = arg;
            fedSize++;
            return *this;
        }
        /// Append a copy of arg, @pre dimension is the same as the other DBMs.
        raw_t* append(const raw_t* arg, cindex_t dim) {
            fhead = fdbm_t::create(arg, dim, fhead);
            fedSize++;
            return fhead->getMatrix();
        }

        /// Remove DBMs of 'this' that are included in arg
        /// and DBMs of arg that are included in 'this'.
        void removeIncluded(dbmlist_t& arg);

        /// Union of arg with this dbmlist_t, does inclusion checking
        /// @post dbmlist_t arg is invalid.
        dbmlist_t& unionWith(dbmlist_t& arg) {
            removeIncluded(arg);
            append(arg);
            return *this;
        }

        /// Simple reduction by inclusion check of DBMs.
        void reduce(cindex_t dim);

        /// Reduction by inclusion check + merge (by pairs) of DBMs.
        void mergeReduce(cindex_t dim, size_t jumpj = 0, int expensiveTry = 0);

        /// @return the federation size.
        size_t size() const {
            assert((fedSize == 0) == (fhead == NULL));
            assert(fhead == NULL || fedSize == fhead->size());
            return fedSize;
        }

        /// Head of the list.
        const fdbm_t* const_head() const { return fhead; }
        fdbm_t* head() { return fhead; }
        fdbm_t** atHead() { return &fhead; }

        /// Update the federation size.
        void incSize(size_t n = 1) { fedSize+= n; }
        void decSize(size_t n = 1) { assert(fedSize >= n); fedSize-= n; }

        /// Brutal re-set of the list.
        void reset(fdbm_t *dbms = NULL, size_t size = 0) {
            fedSize = size;
            fhead = dbms;
        }
        void reset(dbmlist_t& l) { reset(l.fhead, l.fedSize); }

        /// @return 'this' intersected with arg. @pre same dimension.
        dbmlist_t& intersection(const raw_t* arg, cindex_t dim);
        dbmlist_t& intersection(const dbm_t& arg, cindex_t dim);

        /// @return a simple copy of this list of DBMs.
        dbmlist_t copyList() const {
            return dbmlist_t(fedSize, fdbm_t::copy(fhead));
        }

        /// Steal one DBM of a dbmlist_t.
        void steal(fdbm_t **dbm, dbmlist_t &owner) {
            steal(&fhead, dbm, owner);
        }
        /// @pre head is somewhere in this list of DBMs
        fdbm_t** steal(fdbm_t **head, fdbm_t **dbm, dbmlist_t &owner) {
            assert(owner.fedSize && dbm && *dbm && head);
            fdbm_t **atNext = (*dbm)->getNextMutable();
            fdbm_t *next = *atNext;
            *atNext = *head;
            *head = *dbm;
            *dbm = next;
            incSize();
            owner.decSize();
            return atNext;
        }

        /// Steal a list and append it at the end.
        void stealFromToEnd(fdbm_t **next, dbmlist_t &dbmList) {
            while(*next) next = (*next)->getNextMutable();
            *next = dbmList.fhead;
            incSize(dbmList.fedSize);
            dbmList.reset();
        }

        /// Swap DBM lists.
        void swap(dbmlist_t &arg) {
            size_t s = fedSize;
            fedSize = arg.fedSize;
            arg.fedSize = s;
            fdbm_t* h = fhead;
            fhead = arg.fhead;
            arg.fhead = h;
        }

        /// Copy ref.
        void copyRef(dbmlist_t &arg) {
            fedSize = arg.fedSize;
            fhead = arg.fhead;
        }

        /// Remove head of list. @pre size() > 0.
        void removeHead() {
            assert(fedSize && fhead);
            fhead = fhead->removeAndNext();
            decSize();
        }

#ifndef NDEBUG
        /// Print for debugging only, use operator << on fed_t instead.
        void print(std::ostream& os = std::cerr) const;
        void err() const;
        void out() const;
#endif

    protected:
        size_t fedSize; //< size of the federation
        fdbm_t* fhead;  //< federation head of the list (1st DBM)
    };
    
    class ifed_t : public dbmlist_t
    {
    public:
        /// Creation and initialization of ifed_t. @pre the dbm is not empty.
        static ifed_t* create(const raw_t* adbm, cindex_t dim, size_t nxtSize = 0, fdbm_t* nxt = NULL) {
            return create(dim, 1+nxtSize, fdbm_t::create(adbm, dim, nxt));
        }
        static ifed_t* create(const dbm_t& adbm, size_t nxtSize = 0, fdbm_t* nxt = NULL) {
            return create(adbm.const_idbmt()->getDimension(), 1+nxtSize, fdbm_t::create(adbm, nxt));
        }
        static ifed_t* create(cindex_t dim) { // initial empty
            return create(dim, 0, NULL);
        }
        static ifed_t* create(cindex_t dim, dbmlist_t dbmlist) {
            return create(dim, dbmlist.size(), dbmlist.head());
        }

        /// Update the 1st DBM, @pre size() > 0 and same dimension.
        void update(const raw_t* adbm, cindex_t adim) {
            assert(size() > 0 && adim == dim);
            fhead->dbmt().updateCopy(adbm, adim);
        }
        void update(const dbm_t& adbm) {
            assert(size() > 0 && adbm.getDimension() == dim);
            fhead->dbmt().updateCopy(adbm);
        }

        /// Check invariants.
        bool isOK() const {
            size_t n = 0;
            for(const fdbm_t* f = fhead; f != NULL; f = f->getNext(), n++) {
                if (f->isEmpty() || f->getDimension() != dim) {
                    return false;
                }
            }
            return fedSize == n;
        }

        /// @return dimension of this federation.
        cindex_t getDimension() const { return dim; }

        /// Change the dimension @pre isEmpty()
        void setDimension(cindex_t d) {
            assert(isEmpty());
            dim = d;
        }

        /// Set this federation to one DBM.
        void setToDBM(const dbm_t& arg) {
            assert(refCounter == 1);
            // Add arg before deallocating DBMs.
            dim = arg.pdim();
            insert(arg);
            // Now fix the rest of the list.
            fdbm_t::removeAll(fhead->getNext());
            fhead->setNext(NULL);
            fedSize = 1;
        }

        /// @return true if this federation is empty.
        bool isEmpty() const {
            assert((size() == 0) == (fhead == NULL));
            return fhead == NULL;
        }

        /// Compute a hash value that does not depend on the order of the DBMs.
        uint32_t hash(uint32_t seed = 0) const;

        /// Decrement reference count, maybe deallocate.
        void decRef() {
            assert(refCounter > 0);
            if (!--refCounter) remove();
        }

        /// Decrement reference count with @pre !isMutable()
        void decRefImmutable() {
            assert(refCounter > 1);
            --refCounter;
        }

        /// Increment reference count.
        void incRef() { ++refCounter; }

        /// @return if this ifed_t can be modified.
        bool isMutable() const {
            assert(refCounter > 0);
            return refCounter == 1;
        }

        /// Remove this ifed_t with @pre isMutable()
        void removeMutable() {
            assert(--refCounter == 0); // For debugging purposes.
            remove();
        }

        /// @return copy of this ifed_t with appended list.
        /// @pre other is mutable.
        ifed_t* copy(ifed_t *other) const {
            assert(other && other->isMutable() && other->dim == dim && other->isOK());
            other->fhead = fdbm_t::copy(fhead, other->fhead);
            other->fedSize += fedSize;
            return other;
        }
        ifed_t* copy(fdbm_t *end = NULL, size_t endSize = 0) const {
            assert(endSize == (end ? end->size() : 0));
            return create(getDimension(), size() + endSize, fdbm_t::copy(fhead, end));
        }

        /// Insert a dbm, @pre same dimension & not empty.
        void insert(const dbm_t& adbm) {
            assert(adbm.getDimension() == getDimension() && !adbm.isEmpty());
            fhead = fdbm_t::create(adbm, fhead);
            incSize();
        }
        void insert(const raw_t* adbm, cindex_t adim) {
            assert(adim == getDimension());
            fhead = fdbm_t::create(adbm, adim, fhead);
            incSize();
        }

        /// Deallocate the list + update state.
        void setEmpty() {
            assert(refCounter <= 1);
            fdbm_t::removeAll(fhead);
            reset();
        }

        /// Deallocate the list and set the new list to 1 DBM.
        void setDBM(fdbm_t *fdbm) {
            fdbm_t::removeAll(fhead);
            fhead = fdbm;
            fedSize = 1;
            fdbm->setNext(NULL);
        }
        // Similar but for a ready list.
        void setFed(fdbm_t *fdbm, size_t nb) {
            fdbm_t::removeAll(fhead);
            fhead = fdbm;
            fedSize = nb;
        }

    private:
        friend class fed_t;

        /// Update the dimension - called when DBMs
        /// have been resized.
        void updateDimension(cindex_t d) {
            dim = d;
            assert(isOK());
        }

        /// Allocation of an ifed_t (and a fdbm_t) with an allocator.
        static ifed_t* create(cindex_t dim, size_t size, fdbm_t* head);

        /// Deallocate this ifed and its list of DBMs.
        void remove();

        uint32_t refCounter; //< reference counter
        cindex_t dim;         //< dimension
    };
    
    /***********************************************************
     * Inlined implementation of template ClockOperation<TYPE> *
     ***********************************************************/

#define TEMPLATE template<class TYPE> inline
#define CLOCKOP ClockOperation<TYPE>

    TEMPLATE CLOCKOP::ClockOperation(TYPE* d, cindex_t c)
        : ptr(d), clock(c), incVal(0)
    {
        assert(ptr && c < d->getDimension());
    }

    TEMPLATE CLOCKOP& CLOCKOP::operator + (int32_t val)
    {
        incVal += val; // intended to be used that way
        return *this;
    }

    TEMPLATE CLOCKOP& CLOCKOP::operator - (int32_t val)
    {
        incVal -= val; // intended to be used that way
        return *this;
    }

    TEMPLATE CLOCKOP& CLOCKOP::operator += (int32_t val)
    {
        return (*this = *this + val);
    }

    TEMPLATE CLOCKOP& CLOCKOP::operator -= (int32_t val)
    {
        return (*this = *this - val);
    }

    TEMPLATE CLOCKOP& CLOCKOP::operator = (const CLOCKOP& op)
    {
        assert(ptr == op.ptr); // don't mix-up operations
        ptr->update(clock, op.clock, op.incVal);
        incVal = 0;
        return *this;
    }

    TEMPLATE CLOCKOP& CLOCKOP::operator = (int32_t val)
    {
        ptr->updateValue(clock, val);
        incVal = 0;
        return *this;
    }

    TEMPLATE bool CLOCKOP::operator <  (const CLOCKOP& x) const
    {
        assert(ptr == x.ptr); // clock - x.clock < x.incVal - incVal
        return ptr->satisfies(clock, x.clock, dbm_bound2raw(x.incVal-incVal, dbm_STRICT));
    }

    TEMPLATE bool CLOCKOP::operator <= (const CLOCKOP& x) const
    {
        assert(ptr == x.ptr); // clock - x.clock <= x.incVal - incVal
        return ptr->satisfies(clock, x.clock, dbm_bound2raw(x.incVal-incVal, dbm_WEAK));
    }

    TEMPLATE bool CLOCKOP::operator >  (const CLOCKOP& x) const
    {
        assert(ptr == x.ptr); // x.clock - clock < incVal - x.incVal
        return ptr->satisfies(x.clock, clock, dbm_bound2raw(incVal-x.incVal, dbm_STRICT));
    }

    TEMPLATE bool CLOCKOP::operator >= (const CLOCKOP& x) const
    {
        assert(ptr == x.ptr); // x.clock - clock <= incVal - x.incVal
        return ptr->satisfies(x.clock, clock, dbm_bound2raw(incVal-x.incVal, dbm_WEAK));
    }

    TEMPLATE bool CLOCKOP::operator == (const CLOCKOP& x) const
    {
        return (*this >= x) && (*this <= x);
    }

    TEMPLATE bool CLOCKOP::operator <  (int32_t v) const
    {
        return ptr->satisfies(clock, 0, dbm_bound2raw(v-incVal, dbm_STRICT));
    }

    TEMPLATE bool CLOCKOP::operator <= (int32_t v) const
    {
        return ptr->satisfies(clock, 0, dbm_bound2raw(v-incVal, dbm_WEAK));
    }

    TEMPLATE bool CLOCKOP::operator >  (int32_t v) const
    {
        return ptr->satisfies(0, clock, dbm_bound2raw(incVal-v, dbm_STRICT));
    }

    TEMPLATE bool CLOCKOP::operator >= (int32_t v) const
    {
        return ptr->satisfies(0, clock, dbm_bound2raw(incVal-v, dbm_WEAK));
    }

    TEMPLATE bool CLOCKOP::operator == (int32_t v) const
    {
        return (*this <= v) && (*this >= v);
    }

#undef CLOCKOP
#undef TEMPLATE
    
    /***********************************
     * Inlined implementation of dbm_t *
     ***********************************/
    
    inline dbm_t::dbm_t(cindex_t dim)
    {
        assert(dim);
        setEmpty(dim);
    }

    inline dbm_t::dbm_t(const raw_t* arg, cindex_t dim)
    {
        assert(arg && dim); 
        assertx(dbm_isValid(arg, dim));
        dbm_copy(setNew(dim), arg, dim);
    }

    inline dbm_t::dbm_t(const dbm_t& arg)
    {
        idbmPtr = arg.idbmPtr;
        incRef();
    }

    inline dbm_t::~dbm_t()
    {
        decRef();
    }

    inline cindex_t dbm_t::getDimension() const
    {
        return isEmpty() ? edim() : pdim();
    }

    inline void dbm_t::setDimension(cindex_t dim)
    {
        decRef();
        setEmpty(dim);
    }

    inline bool dbm_t::isEmpty() const
    {
        return uval() & 1;
    }

    inline void dbm_t::setEmpty()
    {
        setDimension(getDimension());
    }

    inline void dbm_t::nil()
    {
        setDimension(1);
    }

    inline uint32_t dbm_t::hash(uint32_t seed) const
    {
        return isEmpty() ? uval() : const_idbmt()->hash(seed);
    }
    
    inline bool dbm_t::sameAs(const dbm_t& arg) const
    {
        return idbmPtr == arg.idbmPtr;
    }

    inline void dbm_t::intern()
    {
        if (!isEmpty()) ptr_intern();
    }

    inline const raw_t* dbm_t::operator () () const
    {
        return isEmpty() ? NULL : const_dbm();
    }

    inline raw_t dbm_t::operator () (cindex_t i, cindex_t j) const
    {
        assert(i < getDimension() && j < getDimension() && !isEmpty());
        return const_dbm()[i*pdim()+j];
    }

    inline const raw_t* dbm_t::operator [] (cindex_t i) const
    {
        assert(i < getDimension() && !isEmpty());
        return const_dbm() + i*pdim();
    }

    inline raw_t* dbm_t::getDBM()
    {
        return isEmpty() ? setNew(edim()) : getCopy();
    }

#ifdef ENABLE_STORE_MINGRAPH
    inline const uint32_t* dbm_t::getMinDBM(size_t *size) const
    {
        assert(!isEmpty());
        return idbmPtr->getMinGraph(size);
    }

    inline size_t dbm_t::analyzeForMinDBM(uint32_t *bitMatrix) const
    {
        size_t result;
        size_t dim = pdim();
        base_copySmall(bitMatrix, getMinDBM(&result), bits2intsize(dim*dim));
        return result;
    }
#else
    inline size_t dbm_t::analyzeForMinDBM(uint32_t *bitMatrix) const
    {
        assert(!isEmpty());
        return dbm_analyzeForMinDBM(const_dbm(), pdim(), bitMatrix);
    }
#endif

    inline int32_t* dbm_t::writeToMinDBMWithOffset(bool minimizeGraph,
                                                   bool tryConstraints16,
                                                   allocator_t c_alloc,
                                                   size_t offset) const
    {
        assert(!isEmpty());
        return dbm_writeToMinDBMWithOffset(const_dbm(), pdim(),
                                           (BOOL)minimizeGraph, (BOOL)tryConstraints16,
                                           c_alloc, offset);
    }

    inline int32_t* dbm_t::writeAnalyzedDBM(uint32_t *bitMatrix,
                                            size_t nbConstraints,
                                            BOOL tryConstraints16,
                                            allocator_t c_alloc,
                                            size_t offset) const
    {
        assert(!isEmpty());
        return dbm_writeAnalyzedDBM(const_dbm(), pdim(),
                                    bitMatrix, nbConstraints,
                                    (BOOL)tryConstraints16, c_alloc, offset);
    }

    inline dbm_t dbm_t::readFromMinDBM(cindex_t dim, mingraph_t ming)
    {
        assert(ming);
        assert(dim == dbm_getDimOfMinDBM(ming));
        dbm_t result(dim);
        dbm_readFromMinDBM(result.getDBM(), ming);
        return result;
    }

    inline size_t dbm_t::getSizeOfMinDBM(cindex_t dim, mingraph_t ming)
    {
        assert(dim == dbm_getDimOfMinDBM(ming));
        return dbm_getSizeOfMinDBM(ming);
    }

    inline relation_t dbm_t::relation(mingraph_t ming, raw_t* unpackBuffer) const
    {
        // A mingraph_t never represents empty DBMs.
               return isEmpty() ? base_SUBSET : dbm_relationWithMinDBM(const_dbm(), pdim(), ming, unpackBuffer);
    }

    inline dbm_t& dbm_t::operator = (const dbm_t& arg)
    {
        arg.incRef(); // first in case a = a;
        decRef();
        idbmPtr = arg.idbmPtr;
        return *this;
    }

    inline dbm_t& dbm_t::operator = (const raw_t* arg)
    {
        copyFrom(arg, getDimension());
        return *this;
    }

    inline bool dbm_t::operator == (const fed_t& arg) const
    {
        return arg == *this; // fed_t has the implementation
    }

    inline bool dbm_t::operator != (const dbm_t& arg) const
    {
        return !(*this == arg);
    }

    inline bool dbm_t::operator != (const raw_t* arg) const
    {
        return !(*this == arg);
    }

    inline bool dbm_t::operator != (const fed_t& arg) const
    {
        return !(*this == arg);
    }
    
    inline bool dbm_t::operator < (const dbm_t& arg) const
    {
        return relation(arg) == base_SUBSET;
    }

    inline bool dbm_t::operator < (const fed_t& arg) const
    {
        return arg > *this; // fed_t has the implementation
    }

    inline bool dbm_t::operator > (const dbm_t& arg) const
    {
        return relation(arg) == base_SUPERSET;
    }

    inline bool dbm_t::operator > (const fed_t& arg) const
    {
        return arg < *this; // fed_t has the implementation
    }

    inline bool dbm_t::operator <= (const fed_t& arg) const
    {
        return arg >= *this;  // fed_t has the implementation
    }

    inline bool dbm_t::operator <= (const raw_t* arg) const
    {
        return isEmpty() || dbm_isSubsetEq(const_dbm(), arg, pdim());
    }

    inline bool dbm_t::operator >= (const dbm_t& arg) const
    {
        return arg <= *this;
    }

    inline bool dbm_t::operator >= (const fed_t& arg) const
    {
        return arg <= *this; // fed_t has the implementation
    }

    inline bool dbm_t::operator >= (const raw_t* arg) const
    {
        return !isEmpty() && dbm_isSubsetEq(arg, const_dbm(), pdim());
    }

    inline relation_t dbm_t::relation(const fed_t& arg) const
    {
        return base_symRelation(arg.relation(*this)); // symmetric -- fed_t has the implementation
    }

    inline bool dbm_t::lt(const fed_t& arg) const
    {
        return arg.gt(*this); // fed_t has the implementation
    }

    inline bool dbm_t::gt(const fed_t& arg) const
    {
        return arg.lt(*this); // fed_t has the implementation
    }

    inline bool dbm_t::le(const fed_t& arg) const
    {
        return arg.ge(*this); // fed_t has the implementation
    }

    inline bool dbm_t::ge(const fed_t& arg) const
    {
        return arg.le(*this); // fed_t has the implementation
    }

    inline bool dbm_t::eq(const fed_t& arg) const
    {
        return arg.eq(*this); // fed_t has the implementation
    }
    
    inline relation_t dbm_t::exactRelation(const fed_t& arg) const
    {
        return base_symRelation(arg.exactRelation(*this)); // fed_t has the implementation
    }

    inline bool dbm_t::isInit() const
    {
        return !isEmpty() && dbm_isEqualToInit(const_dbm(), pdim());
    }

    inline bool dbm_t::isZero() const
    {
        return !isEmpty() && dbm_isEqualToZero(const_dbm(), pdim());
    }

    inline dbm_t& dbm_t::operator &= (const constraint_t& c)
    {
        constrain(c.i, c.j, c.value);
        return *this;
    }

    inline dbm_t& dbm_t::operator &= (const base::pointer_t<constraint_t>& c)
    {
        constrain(c.begin(), c.size());
        return *this;
    }

    inline dbm_t& dbm_t::operator &= (const std::vector<constraint_t>& vec)
    {
        constrain(&vec[0], vec.size());
        return *this;
    }

    inline bool dbm_t::constrain(cindex_t i, int32_t value)
    {
        return !isEmpty() && ptr_constrain(i, value);
    }
    
    inline bool dbm_t::constrain(cindex_t i, cindex_t j, raw_t c)
    {
        return !isEmpty() && ptr_constrain(i, j, c);
    }

    inline bool dbm_t::constrain(cindex_t i, cindex_t j, int32_t b, strictness_t s)
    {
        return !isEmpty() && ptr_constrain(i, j, dbm_bound2raw(b,s));
    }

    inline bool dbm_t::constrain(cindex_t i, cindex_t j, int32_t b, bool isStrict)
    {
        return !isEmpty() && ptr_constrain(i, j, dbm_boundbool2raw(b,(BOOL)isStrict));
    }

    inline bool dbm_t::constrain(const constraint_t& c)
    {
        return !isEmpty() && ptr_constrain(c.i, c.j, c.value);
    }

    inline bool dbm_t::constrain(const constraint_t *cnstr, size_t n)
    {
        assert(n == 0 || cnstr);
        return !isEmpty() && (n == 1 ? ptr_constrain(cnstr->i, cnstr->j, cnstr->value) : ptr_constrain(cnstr, n));
    }

    inline bool dbm_t::constrain(const cindex_t *table, const constraint_t *cnstr, size_t n)
    {
        return !isEmpty() && ptr_constrain(table, cnstr, n);
    }

    inline bool dbm_t::constrain(const cindex_t *table, const base::pointer_t<constraint_t>& vec)
    {
        return !isEmpty() && ptr_constrain(table, vec.begin(), vec.size());
    }

    inline bool dbm_t::constrain(const cindex_t *table, const std::vector<constraint_t>& vec)
    {
        return !isEmpty() && constrain(table, &vec[0], vec.size());
    }

    inline dbm_t& dbm_t::up()
    {
        if (!isEmpty()) ptr_up();
        return *this;
    }

    inline dbm_t& dbm_t::upStop(const uint32_t* stopped)
    {
        if (!isEmpty())
        {
            if (stopped) ptr_upStop(stopped);
            else ptr_up();
        }
        return *this;
    }

    inline dbm_t& dbm_t::down()
    {
        if (!isEmpty()) ptr_down();
        return *this;
    }
    
    inline dbm_t& dbm_t::downStop(const uint32_t* stopped)
    {
        if (!isEmpty())
        {
            if (stopped) ptr_downStop(stopped);
            else ptr_down();
        }
        return *this;
    }

    inline dbm_t& dbm_t::freeClock(cindex_t k)
    {
        if (!isEmpty()) ptr_freeClock(k);
        return *this;
    }

    inline dbm_t& dbm_t::freeUp(cindex_t k)
    {
        if (!isEmpty()) ptr_freeUp(k);
        return *this;
    }

    inline dbm_t& dbm_t::freeDown(cindex_t k)
    {
        if (!isEmpty()) ptr_freeDown(k);
        return *this;
    }

    inline dbm_t& dbm_t::freeAllUp()
    {
        if (!isEmpty()) ptr_freeAllUp();
        return *this;
    }

    inline dbm_t& dbm_t::freeAllDown()
    {
        if (!isEmpty()) ptr_freeAllDown();
        return *this;
    }

    inline void dbm_t::updateValue(cindex_t k, int32_t v)
    {
        if (!isEmpty()) ptr_updateValue(k, v);
    }

    inline void dbm_t::updateClock(cindex_t i, cindex_t j)
    {
        if (i != j && !isEmpty()) ptr_updateClock(i, j);
    }

    inline void dbm_t::updateIncrement(cindex_t i, int32_t v)
    {
        // There is no point in even trying not to copy because the lower
        // bounds are always going to be incremented.
        if (v != 0 && !isEmpty()) dbm_updateIncrement(getCopy(), pdim(), i, v);
    }

    inline bool dbm_t::satisfies(cindex_t i, cindex_t j, raw_t c) const
    {
        assert(i < getDimension() && j < getDimension());
        return !isEmpty() && dbm_satisfies(const_dbm(), pdim(), i, j, c);
    }
    
    inline bool dbm_t::satisfies(const constraint_t& c) const
    {
        return satisfies(c.i, c.j, c.value);
    }

    inline bool dbm_t::operator && (const constraint_t& c) const
    {
        return satisfies(c);
    }
    
    inline bool dbm_t::isUnbounded() const
    {
        return !isEmpty() && dbm_isUnbounded(const_dbm(), pdim());
    }

    inline dbm_t& dbm_t::relaxUp()
    {
        return relaxDownClock(0);
    }

    inline dbm_t& dbm_t::relaxDown()
    {
        return relaxUpClock(0);
    }

    inline dbm_t& dbm_t::tightenDown()
    {
        if (!isEmpty()) ptr_tightenDown();
        return *this;
    }

    inline dbm_t& dbm_t::tightenUp()
    {
        if (!isEmpty()) ptr_tightenUp();
        return *this;
    }

    inline dbm_t& dbm_t::relaxUpClock(cindex_t k)
    {
        if (!isEmpty()) ptr_relaxUpClock(k);
        return *this;
    }

    inline dbm_t& dbm_t::relaxDownClock(cindex_t k)
    {
        if (!isEmpty()) ptr_relaxDownClock(k);
        return *this;
    }

    inline dbm_t& dbm_t::relaxAll()
    {
        if (!isEmpty()) ptr_relaxAll();
        return *this;
    }

    inline bool dbm_t::getMinDelay(const DoubleValuation& point, double* t, double *minVal, bool *minStrict,
                                   const uint32_t *stopped) const
    {
        return getMinDelay(point.begin(), point.size(), t, minVal, minStrict, stopped);
    }

    inline bool dbm_t::getMaxDelay(const DoubleValuation& point, double* t, double *minVal, bool *minStrict,
                                   const uint32_t* stopped) const
    {
        return getMaxDelay(point.begin(), point.size(), t, minVal, minStrict, stopped);
    }

    inline bool dbm_t::getMaxBackDelay(const DoubleValuation& point, double* t, double max) const
    {
        return getMaxBackDelay(point.begin(), point.size(), t, max);
    }

    inline void dbm_t::swapClocks(cindex_t x, cindex_t y)
    {
        if (!isEmpty()) ptr_swapClocks(x, y);
    }

    inline void dbm_t::extrapolateMaxBounds(const int32_t *max)
    {
        if (!isEmpty()) dbm_extrapolateMaxBounds(getCopy(), pdim(), max);
    }

    inline void dbm_t::diagonalExtrapolateMaxBounds(const int32_t *max)
    {
        if (!isEmpty()) dbm_diagonalExtrapolateMaxBounds(getCopy(), pdim(), max);
    }

    inline void dbm_t::extrapolateLUBounds(const int32_t *lower, const int32_t *upper)
    {
        if (!isEmpty()) dbm_extrapolateLUBounds(getCopy(), pdim(), lower, upper);
    }

    inline void dbm_t::diagonalExtrapolateLUBounds(const int32_t *lower, const int32_t *upper)
    {
        if (!isEmpty()) dbm_diagonalExtrapolateLUBounds(getCopy(), pdim(), lower, upper);
    }

    inline dbm_t::dbm_t(const ClockOperation<dbm_t>& op)
    {
        idbmPtr = op.getPtr()->idbmPtr;
        incRef();
        updateIncrement(op.getClock(), op.getVal());
    }

    inline ClockOperation<dbm_t> dbm_t::operator () (cindex_t clk)
    {
        assert(clk < getDimension());
        return ClockOperation<dbm_t>(this, clk);
    }

    inline bool dbm_t::isSubtractionEmpty(const raw_t* arg, cindex_t dim) const
    {
        assert(getDimension() == dim);
        assertx(dbm_isValid(arg, dim));
        // this - arg == empty if all DBMs of this <= arg because arg is a DBM.
        return isEmpty() || (dbm_relation(const_dbm(), arg, dim) & base_SUBSET) != 0;
    }

    inline bool dbm_t::isSubtractionEmpty(const dbm_t& arg) const
    {
        assert(getDimension() == arg.getDimension());
        return *this <= arg;
    }

    inline void dbm_t::newCopy(const raw_t* arg, cindex_t dim)
    {
        assert(arg && dim > 0);
        assertx(dbm_isValid(arg, dim));
        dbm_copy(setNew(dim), arg, dim);
    }

    inline void dbm_t::newCopy(const dbm_t& arg)
    {
        arg.idbmPtr->incRef();
        setPtr(arg.idbmPtr);
    }
    
    inline void dbm_t::updateCopy(const raw_t* arg, cindex_t dim)
    {
        assert(arg && dim > 0);
        assertx(dbm_isValid(arg, dim));
        idbmt()->decRef();
        dbm_copy(setNew(dim), arg, dim);
    }

    inline void dbm_t::updateCopy(const dbm_t& arg)
    {
        arg.idbmPtr->incRef();
        idbmt()->decRef();
        setPtr(arg.idbmPtr);
    }

    inline const idbm_t* dbm_t::const_idbmt() const
    {
        assert(isPointer(idbmPtr)); // stronger than !isEmpty()
        return idbmPtr;
    }

    inline idbm_t* dbm_t::idbmt()
    {
        assert(isPointer(idbmPtr)); // stronger than !isEmpty()
        return idbmPtr;
    }
    
    inline uintptr_t dbm_t::uval() const
    {
        return (uintptr_t) idbmPtr;
    }

    inline void dbm_t::incRef() const
    {
        if (!isEmpty()) idbmPtr->incRef();
    }

    inline void dbm_t::decRef() const
    {
        if (!isEmpty()) idbmPtr->decRef();
    }

    inline void dbm_t::empty(cindex_t dim)
    {
        idbmt()->decRef();
        setEmpty(dim);
    }

    inline void dbm_t::emptyImmutable(cindex_t dim)
    {
        idbmt()->decRefImmutable();
        setEmpty(dim);
    }

    inline void dbm_t::emptyMutable(cindex_t dim)
    {
        idbmt()->removeMutable();
        setEmpty(dim);
    }

    inline void dbm_t::setEmpty(cindex_t dim)
    {
        idbmPtr = (idbm_t*) ((dim << 1) | 1);
    }

    inline void dbm_t::setPtr(idbm_t* ptr)
    {
        assert(isPointer(ptr));
        idbmPtr = ptr;
    }

    inline cindex_t dbm_t::edim() const
    {
        assert(isEmpty());
        return uval() >> 1;
    }

    inline cindex_t dbm_t::pdim() const
    {
        return const_idbmt()->getDimension();
    }

    inline bool dbm_t::isMutable() const
    {
        return const_idbmt()->isMutable();
    }

    inline bool dbm_t::tryMutable()
    {
        return idbmt()->tryMutable();
    }

    inline const raw_t* dbm_t::const_dbm() const
    {
        return const_idbmt()->const_dbm();
    }
    
    inline raw_t* dbm_t::dbm()
    {
        assert(isMutable());
#ifdef ENABLE_STORE_MINGRAPH
        assert(!idbmt()->isValid());
#endif
        return idbmt()->dbm();
    }

    inline raw_t* dbm_t::setNew(cindex_t dim)
    {
        assert(dim);
        setPtr(new (dbm_new(dim)) idbm_t(dim));
        return dbm();
    }

    inline raw_t* dbm_t::inew(cindex_t dim)
    {
        assert(!tryMutable());
        idbmt()->decRefImmutable();
        return setNew(dim);
    }

    inline raw_t* dbm_t::icopy(cindex_t dim)
    {
        assert(!tryMutable() && pdim() == dim);
        idbmt()->decRefImmutable();
        setPtr(new (dbm_new(dim)) idbm_t(*idbmPtr));
        return dbm();
    }

    inline raw_t* dbm_t::getNew()
    {
        RECORD_STAT();
        RECORD_SUBSTAT(tryMutable() ? "mutable" : "new");
        return tryMutable() ? dbm() : inew(pdim());
    }

    inline raw_t* dbm_t::getCopy()
    {
        RECORD_STAT();
        RECORD_SUBSTAT(tryMutable() ? "mutable" : "copy");
        return tryMutable() ? dbm() : icopy(pdim());
    }

    // Wrapper functions
 
    inline dbm_t zero(cindex_t dim)                         { return dbm_t(dim).setZero();       }
    inline dbm_t init(cindex_t dim)                         { return dbm_t(dim).setInit();       }
    inline dbm_t up(const dbm_t& d)                        { return dbm_t(d).up();              }
    inline dbm_t down(const dbm_t& d)                      { return dbm_t(d).down();            }
    inline dbm_t freeClock(const dbm_t& d, cindex_t x)      { return dbm_t(d).freeClock(x);      }
    inline dbm_t freeUp(const dbm_t& d, cindex_t k)         { return dbm_t(d).freeUp(k);         }
    inline dbm_t freeDown(const dbm_t& d, cindex_t k)       { return dbm_t(d).freeDown(k);       }
    inline dbm_t freeAllUp(const dbm_t& d)                 { return dbm_t(d).freeAllUp();       }
    inline dbm_t freeAllDown(const dbm_t& d)               { return dbm_t(d).freeAllDown();     }
    inline dbm_t relaxUp(const dbm_t& d)                   { return dbm_t(d).relaxUp();         }
    inline dbm_t relaxDown(const dbm_t& d)                 { return dbm_t(d).relaxDown();       }
    inline dbm_t relaxUpClock(const dbm_t& d, cindex_t k)   { return dbm_t(d).relaxUpClock(k);   }
    inline dbm_t relaxDownClock(const dbm_t& d, cindex_t k) { return dbm_t(d).relaxDownClock(k); }

    /***********************************************
     *  Inlined implementations of fed_t::iterator *
     ***********************************************/

    inline fed_t::iterator::iterator()
        : fdbm(const_cast<fdbm_t**>(&ENDF)), ifed(NULL) {}

    inline fed_t::iterator::iterator(ifed_t *ifd)
        : fdbm(ifd->atHead()), ifed(ifd) {}

    inline dbm_t& fed_t::iterator::operator *() const
    {
        assert(fdbm && *fdbm);
        return (*fdbm)->dbmt();
    }

    inline dbm_t* fed_t::iterator::operator ->() const
    {
        assert(fdbm && *fdbm);
        return &(*fdbm)->dbmt();
    }

    inline raw_t* fed_t::iterator::operator () () const
    {
        assert(fdbm && *fdbm);
        return (*fdbm)->dbmt().getDBM();
    }

    inline raw_t fed_t::iterator::operator () (cindex_t i, cindex_t j) const
    {
        assert(fdbm && *fdbm);
        return (*fdbm)->dbmt()(i,j);
    }

    inline fed_t::iterator& fed_t::iterator::operator ++()
    {
        assert(fdbm && *fdbm);
        fdbm = (*fdbm)->getNextMutable();
        return *this;
    }

    inline bool fed_t::iterator::null() const
    {
        assert(fdbm);
        return *fdbm == NULL;
    }

    inline bool fed_t::iterator::hasNext() const
    {
        assert(fdbm);
        return (*fdbm)->getNext() != NULL;
    }

    inline bool fed_t::iterator::operator == (const iterator& arg) const
    {
        assert(fdbm && arg.fdbm);
        return *fdbm == *arg.fdbm; // don't check ifed
    }

    inline bool fed_t::iterator::operator != (const iterator& arg) const
    {
        return !(*this == arg);
    }

    inline void fed_t::iterator::remove()
    {
        assert(fdbm && *fdbm);
        *fdbm = (*fdbm)->removeAndNext();
        ifed->decSize();
    }

    inline void fed_t::iterator::removeEmpty()
    {
        assert(fdbm && *fdbm);
        *fdbm = (*fdbm)->removeEmptyAndNext();
        ifed->decSize();
    }

    inline fdbm_t* fed_t::iterator::extract()
    {
        assert(fdbm && *fdbm);
        fdbm_t *current = *fdbm;
        *fdbm = current->getNext();
        ifed->decSize();
        return current;
    }

    inline void fed_t::iterator::insert(fdbm_t* dbm)
    {
        assert(fdbm);
        dbm->setNext(*fdbm);
        *fdbm = dbm;
        ifed->incSize();
    }
    
    inline fed_t::iterator fed_t::beginMutable()
    {
        setMutable();
        return fed_t::iterator(ifed());
    }

    inline const fed_t::iterator fed_t::endMutable() const
    {
        return fed_t::iterator();
    }

    inline fed_t::iterator fed_t::erase(iterator& iter)
    {
        assert(hasSame(*iter));
        iter.remove();
        return iter;
    }

    /*****************************************************
     *  Inlined implementations of fed_t::const_iterator *
     *****************************************************/

    inline fed_t::const_iterator::const_iterator(const fdbm_t* fed)
        : fdbm(fed) {}
    inline fed_t::const_iterator::const_iterator(const fed_t& fed)
        : fdbm(fed.ifed()->const_head()) {}
    inline fed_t::const_iterator::const_iterator()
        : fdbm(NULL) {}

    inline const dbm_t& fed_t::const_iterator::operator *() const
    {
        assert(fdbm);
        return fdbm->const_dbmt();
    }

    inline const dbm_t* fed_t::const_iterator::operator ->() const
    {
        assert(fdbm);
        return &fdbm->const_dbmt();
    }

    inline const raw_t* fed_t::const_iterator::operator () () const
    {
        assert(fdbm);
        return fdbm->const_dbmt()();
    }

    inline raw_t fed_t::const_iterator::operator () (cindex_t i, cindex_t j) const
    {
        assert(fdbm);
        return fdbm->const_dbmt()(i,j);
    }

    inline fed_t::const_iterator& fed_t::const_iterator::operator ++()
    {
        assert(fdbm);
        fdbm = fdbm->getNext();
        return *this;
    }

    inline bool fed_t::const_iterator::null() const
    {
        return fdbm == NULL;
    }

    inline bool fed_t::const_iterator::hasNext() const
    {
        assert(fdbm);
        return fdbm->getNext() != NULL;
    }

    inline bool fed_t::const_iterator::operator == (const const_iterator& arg) const
    {
        return fdbm == arg.fdbm;
    }

    inline bool fed_t::const_iterator::operator != (const const_iterator& arg) const
    {
        return fdbm != arg.fdbm;
    }

    inline fed_t::const_iterator fed_t::begin() const
    {
        return fed_t::const_iterator(ifed()->const_head());
    }

    inline const fed_t::const_iterator fed_t::end() const
    {
        return fed_t::const_iterator::ENDI;
    }

    /***********************************
     * Inlined implementation of fed_t *
     ***********************************/

    inline fed_t::fed_t(cindex_t dim)
        : ifedPtr(ifed_t::create(dim))
    {
        assert(dim >= 1);
    }

    inline fed_t::fed_t(const fed_t& arg)
        : ifedPtr(arg.ifedPtr)
    {
        incRef();
    }

    inline fed_t::fed_t(ifed_t* ifed)
        : ifedPtr(ifed)
    {}

    inline fed_t::fed_t(const dbm_t& arg)
        : ifedPtr(arg.isEmpty()
                  ? ifed_t::create(arg.edim())
                  : ifed_t::create(arg))
    {}

    inline fed_t::fed_t(const raw_t* arg, cindex_t dim)
        : ifedPtr(ifed_t::create(arg, dim))
    {}

    inline fed_t::~fed_t()
    {
        decRef();
    }

    inline size_t fed_t::size() const
    {
        return ifed()->size();
    }

    inline cindex_t fed_t::getDimension() const
    {
        return ifed()->getDimension();
    }

    inline bool fed_t::isEmpty() const
    {
        return ifed()->isEmpty();
    }

    inline void fed_t::nil()
    {
        setDimension(1);
    }

    inline fed_t& fed_t::reduce()
    {
        assert(isOK());
        ifed()->reduce(getDimension());
        return *this;
    }

    inline fed_t& fed_t::mergeReduce(size_t skip, int expensiveTry)
    {
        assert(isOK());
        ifed()->mergeReduce(getDimension(), skip, expensiveTry);
        return *this;
    }

    inline fed_t& fed_t::unionWithC(fed_t arg)
    {
        return unionWith(arg);
    }

    inline fed_t& fed_t::appendC(fed_t arg)
    {
        return append(arg);
    }

    inline fed_t& fed_t::steal(fed_t& arg)
    {
        size_t s = size();
        return appendEnd(arg).mergeReduce(s);
    }

    inline fed_t& fed_t::stealC(fed_t arg)
    {
        return steal(arg);
    }

    inline void fed_t::swap(fed_t& arg)
    {
        ifed_t *tmp = ifedPtr;
        ifedPtr = arg.ifedPtr;
        arg.ifedPtr = tmp;
    }

    inline uint32_t fed_t::hash(uint32_t seed) const
    {
        return ifed()->hash(seed);
    }

    inline bool fed_t::sameAs(const fed_t& arg) const
    {
        return ifedPtr == arg.ifedPtr;
    }

    inline fed_t& fed_t::operator = (const fed_t& arg)
    {
        arg.incRef();
        decRef();
        ifedPtr = arg.ifedPtr;
        return *this;
    }

    inline bool fed_t::operator == (const fed_t& arg) const
    {
        return relation(arg) == base_EQUAL;
    }

    inline bool fed_t::operator == (const dbm_t& arg) const
    {
        return relation(arg) == base_EQUAL;
    }

    inline bool fed_t::operator == (const raw_t* arg) const
    {
        return relation(arg, getDimension()) == base_EQUAL;
    }

    inline bool fed_t::operator != (const fed_t& arg) const
    {
        return !(*this == arg);
    }

    inline bool fed_t::operator != (const dbm_t& arg) const
    {
        return !(*this == arg);
    }

    inline bool fed_t::operator != (const raw_t* arg) const
    {
        return !(*this == arg);
    }

    inline bool fed_t::operator <  (const fed_t& arg) const
    {
        return relation(arg) == base_SUBSET;
    }

    inline bool fed_t::operator <  (const dbm_t& arg) const
    {
        return relation(arg) == base_SUBSET;
    }

    inline bool fed_t::operator <  (const raw_t* arg) const
    {
        return relation(arg, getDimension()) == base_SUBSET;
    }

    inline bool fed_t::operator >  (const fed_t& arg) const
    {
        return relation(arg) == base_SUPERSET;
    }

    inline bool fed_t::operator >  (const dbm_t& arg) const
    {
        return relation(arg) == base_SUPERSET;
    }

    inline bool fed_t::operator >  (const raw_t* arg) const
    {
        return relation(arg, getDimension()) == base_SUPERSET;
    }

    inline bool fed_t::operator <= (const fed_t& arg) const
    {
        return (relation(arg) & base_SUBSET) != 0;
    }

    inline bool fed_t::operator <= (const raw_t* arg) const
    {
        return le(arg, getDimension());
    }

    inline bool fed_t::operator >= (const fed_t& arg) const
    {
        return (relation(arg) & base_SUPERSET) != 0;
    }

    inline bool fed_t::operator >= (const raw_t* arg) const
    {
        return isSupersetEq(arg, getDimension());
    }
    
    inline bool fed_t::eq(const fed_t& arg) const
    {
        return exactRelation(arg) == base_EQUAL;
    }

    inline bool fed_t::eq(const dbm_t& arg) const
    {
        return exactRelation(arg) == base_EQUAL;
    }

    inline bool fed_t::eq(const raw_t* arg, cindex_t dim) const
    {
        return exactRelation(arg, dim) == base_EQUAL;
    }

    inline bool fed_t::lt(const fed_t& arg) const
    {
        return exactRelation(arg) == base_SUBSET;
    }

    inline bool fed_t::lt(const dbm_t& arg) const
    {
        return exactRelation(arg) == base_SUBSET;
    }

    inline bool fed_t::lt(const raw_t* arg, cindex_t dim) const
    {
        return exactRelation(arg, dim) == base_SUBSET;
    }

    inline bool fed_t::gt(const fed_t& arg) const
    {
        return exactRelation(arg) == base_SUPERSET;
    }

    inline bool fed_t::gt(const dbm_t& arg) const
    {
        return exactRelation(arg) == base_SUPERSET;
    }

    inline bool fed_t::gt(const raw_t* arg, cindex_t dim) const
    {
        return exactRelation(arg, dim) == base_SUPERSET;
    }

    inline bool fed_t::ge(const fed_t& arg) const
    {
        return arg.le(*this);
    }

    inline bool fed_t::ge(const dbm_t& arg) const
    {
        return arg.isSubtractionEmpty(*this);
    }   

    inline bool fed_t::ge(const raw_t* arg, cindex_t dim) const
    {
        return isSubtractionEmpty(arg, dim, *this);
    }

    inline bool fed_t::le(const fed_t& arg) const
    {
        return isSubtractionEmpty(arg);
    }

    inline bool fed_t::le(const dbm_t& arg) const
    {
        return *this <= arg; // only case exact
    }

    inline fed_t& fed_t::add(const fed_t& arg)
    {
        fed_t f(arg);
        return append(f);
    }

    inline fed_t& fed_t::add(const dbm_t& arg)
    {
        assert(isOK() && arg.getDimension() == getDimension());
        if (!arg.isEmpty())
        {
            ifed()->insert(arg);
        }
        return *this;
    }

    inline fed_t& fed_t::add(const raw_t* arg, cindex_t dim)
    {
        assert(isOK() && dim == getDimension());
        assertx(dbm_isValid(arg, dim));
        ifed()->insert(arg, dim);
        return *this;
    }

    inline fed_t& fed_t::operator &= (const constraint_t& c)
    {
        constrain(c.i, c.j, c.value);
        return *this;
    }

    inline fed_t& fed_t::operator &= (const base::pointer_t<constraint_t>& c)
    {
        constrain(c.begin(), c.size());
        return *this;
    }

    inline fed_t& fed_t::operator &= (const std::vector<constraint_t>& vec)
    {
        constrain(&vec[0], vec.size());
        return *this;
    }

    inline bool fed_t::constrain(cindex_t i, cindex_t j, int32_t b, strictness_t s)
    {
        return constrain(i, j, dbm_bound2raw(b, s));
    }

    inline bool fed_t::constrain(cindex_t i, cindex_t j, int32_t b, bool isStrict)
    {
        return constrain(i, j, dbm_boundbool2raw(b, (BOOL)isStrict));
    }
    
    inline bool fed_t::constrain(const constraint_t& c)
    {
        return constrain(c.i, c.j, c.value);
    }

    inline bool fed_t::satisfies(const constraint_t& c) const
    {
        return satisfies(c.i, c.j, c.value);
    }

    inline bool fed_t::operator && (const constraint_t& c) const
    {
        return satisfies(c.i, c.j, c.value);
    }

    inline fed_t& fed_t::relaxUp()
    {
        return relaxDownClock(0); // see dbm.h
    }

    inline fed_t& fed_t::relaxDown()
    {
        return relaxUpClock(0); // see dbm.h
    }

    inline bool fed_t::removeIncludedIn(const dbm_t& arg)
    {
        assert(isOK());
        assert(getDimension() == arg.getDimension());
        return !arg.isEmpty() && removeIncludedIn(arg.const_dbm(), getDimension());
    }

    inline bool fed_t::contains(const IntValuation& point) const
    {
        return contains(point.begin(), point.size());
    }

    inline bool fed_t::contains(const DoubleValuation& point) const
    {
        return contains(point.begin(), point.size());
    }

    inline double fed_t::possibleBackDelay(const DoubleValuation& point) const
    {
        return possibleBackDelay(point.begin(), point.size());
    }

    inline bool fed_t::getMinDelay(const DoubleValuation& point, double* t, double *minVal, bool *minStrict,
                                   const uint32_t* stopped) const
    {
        return getMinDelay(point.begin(), point.size(), t, minVal, minStrict, stopped);
    }

    inline bool fed_t::getMaxBackDelay(const DoubleValuation& point, double *t, double max) const
    {
        return getMaxBackDelay(point.begin(), point.size(), t, max);
    }

    inline bool fed_t::getDelay(const DoubleValuation& point, double* min, double* max,
                                double *minVal, bool *minStrict, double *maxVal, bool *maxStrict,
                                const uint32_t* stopped) const
    {
        return getDelay(point.begin(), point.size(), min, max, minVal, minStrict, maxVal, maxStrict, stopped);
    }

    inline fed_t::fed_t(const ClockOperation<fed_t>& op)
        : ifedPtr(op.getPtr()->ifed()->copy())
    {
        updateIncrement(op.getClock(), op.getVal());
    }

    inline ClockOperation<fed_t> fed_t::operator () (cindex_t clk)
    {
        assert(clk < getDimension());
        return ClockOperation<fed_t>(this, clk);
    }

    inline bool fed_t::isSubtractionEmpty(const dbm_t& arg) const
    {
        return *this <= arg; // only exact case with "normal" relation
    }

    inline ifed_t* fed_t::ifed()
    {
        assert(isPointer(ifedPtr));
        return ifedPtr;
    }

    inline const ifed_t* fed_t::ifed() const
    {
        assert(isPointer(ifedPtr));
        return ifedPtr;
    }

    inline void fed_t::incRef() const
    {
        assert(isPointer(ifedPtr));
        ifedPtr->incRef();
    }

    inline void fed_t::decRef() const
    {
        assert(isPointer(ifedPtr));
        ifedPtr->decRef();
    }

    inline void fed_t::decRefImmutable() const
    {
        assert(isPointer(ifedPtr));
        ifedPtr->decRefImmutable();
    }

    inline bool fed_t::isMutable() const
    {
        return ifed()->isMutable();
    }

    inline bool fed_t::isOK() const
    {
        return ifed()->isOK();
    }

    inline void fed_t::setMutable()
    {
        if (!isMutable())
        {
            decRefImmutable();
            ifedPtr = ifed()->copy();
        }
        assert(isMutable());
    }

    inline const dbm_t& fed_t::const_dbmt() const
    {
        assert(size());
        return ifed()->const_head()->const_dbmt();
    }

    inline dbm_t& fed_t::dbmt()
    {
        assert(size() == 1 && isMutable());
        return ifed()->head()->dbmt();
    }

    // Trivial wrappers

    inline fed_t up(const fed_t& arg)                               { return fed_t(arg).up();              }
    inline fed_t down(const fed_t& arg)                             { return fed_t(arg).down();            }
    inline fed_t freeClock(const fed_t& arg, cindex_t x)             { return fed_t(arg).freeClock(x);      }
    inline fed_t freeUp(const fed_t& arg, cindex_t x)                { return fed_t(arg).freeUp(x);         }
    inline fed_t freeDown(const fed_t& arg, cindex_t x)              { return fed_t(arg).freeDown(x);       }
    inline fed_t freeAllUp(const fed_t& arg)                        { return fed_t(arg).freeAllUp();       }
    inline fed_t freeAllDown(const fed_t& arg)                      { return fed_t(arg).freeAllDown();     }
    inline fed_t relaxUp(const fed_t& arg)                          { return fed_t(arg).relaxUp();         }
    inline fed_t relaxDown(const fed_t& arg)                        { return fed_t(arg).relaxDown();       }
    inline fed_t relaxUpClock(const fed_t& arg, cindex_t k)          { return fed_t(arg).relaxUpClock(k);   }
    inline fed_t relaxDownClock(const fed_t& arg, cindex_t k)        { return fed_t(arg).relaxDownClock(k); }
    inline fed_t reduce(const fed_t& arg)                           { return fed_t(arg).reduce();          }
    inline fed_t expensiveReduce(const fed_t& arg)                  { return fed_t(arg).expensiveReduce(); }
    inline fed_t predt(const fed_t& g, const fed_t& b)              { return fed_t(g).predt(b);            }
    inline fed_t predt(const fed_t& g, const dbm_t& b)              { return fed_t(g).predt(b);            }
    inline fed_t predt(const fed_t& g, const raw_t* b, cindex_t dim) { return fed_t(g).predt(b, dim);       }
    inline fed_t predt(const dbm_t& g, const fed_t& b)              { return fed_t(g).predt(b);            }
    inline fed_t predt(const dbm_t& g, const dbm_t& b)              { return fed_t(g).predt(b);            }
    inline fed_t predt(const dbm_t& g, const raw_t* b, cindex_t dim) { return fed_t(g).predt(b, dim);       }
    inline fed_t predt(const raw_t* g, cindex_t dim, const fed_t& b) { return fed_t(g, dim).predt(b);       }
    inline fed_t predt(const raw_t* g, cindex_t dim, const dbm_t& b) { return fed_t(g, dim).predt(b);       }
    inline fed_t predt(const raw_t* g, const raw_t* b, cindex_t dim) { return fed_t(g, dim).predt(b, dim);  }
    
    /************ Operator overload -- trivial implementations ************/    
    
    inline dbm_t operator + (const dbm_t& a, const raw_t* b) { return dbm_t(a) += b; }
    inline dbm_t operator + (const fed_t& a, const raw_t* b) { return dbm_t(b, a.getDimension()) += a; }
    inline dbm_t operator + (const dbm_t& a, const dbm_t& b) { return dbm_t(a) += b; }
    inline dbm_t operator + (const dbm_t& a, const fed_t& b) { return dbm_t(a) += b; }
    inline dbm_t operator + (const fed_t& a, const dbm_t& b) { return dbm_t(b) += a; }
    inline dbm_t operator + (const fed_t& a, const fed_t& b) { return (dbm_t(a.getDimension()) += a) += b; }

    inline dbm_t operator & (const dbm_t& a, const raw_t* b) { return dbm_t(a) &= b; }
    inline fed_t operator & (const fed_t& a, const raw_t* b) { return fed_t(a) &= b; }
    inline dbm_t operator & (const dbm_t& a, const dbm_t& b) { return dbm_t(a) &= b; }
    inline fed_t operator & (const dbm_t& a, const fed_t& b) { return fed_t(b) &= a; }
    inline fed_t operator & (const fed_t& a, const dbm_t& b) { return fed_t(a) &= b; }
    inline fed_t operator & (const fed_t& a, const fed_t& b) { return fed_t(a) &= b; }

    inline dbm_t operator & (const dbm_t& a, const constraint_t& c) { return dbm_t(a) &= c; }
    inline dbm_t operator & (const constraint_t& c, const dbm_t& a) { return dbm_t(a) &= c; }
    inline fed_t operator & (const fed_t& a, const constraint_t& c) { return fed_t(a) &= c; }
    inline fed_t operator & (const constraint_t& c, const fed_t& a) { return fed_t(a) &= c; }

    inline dbm_t operator & (const dbm_t& a, const base::pointer_t<constraint_t>& c) { return dbm_t(a) &= c; }
    inline dbm_t operator & (const base::pointer_t<constraint_t>& c, const dbm_t& a) { return dbm_t(a) &= c; }
    inline fed_t operator & (const fed_t& a, const base::pointer_t<constraint_t>& c) { return fed_t(a) &= c; }
    inline fed_t operator & (const base::pointer_t<constraint_t>& c, const fed_t& a) { return fed_t(a) &= c; }

    inline dbm_t operator & (const dbm_t& a, const std::vector<constraint_t>& vec) { return dbm_t(a) &= vec; }
    inline dbm_t operator & (const std::vector<constraint_t>& vec, const dbm_t& a) { return dbm_t(a) &= vec; }
    inline fed_t operator & (const fed_t& a, const std::vector<constraint_t>& vec) { return fed_t(a) &= vec; }
    inline fed_t operator & (const std::vector<constraint_t>& vec, const fed_t& a) { return fed_t(a) &= vec; }

    inline fed_t operator | (const dbm_t& a, const raw_t* b) { return fed_t(a) |= b; }
    inline fed_t operator | (const fed_t& a, const raw_t* b) { return fed_t(a) |= b; }
    inline fed_t operator | (const dbm_t& a, const dbm_t& b) { return fed_t(a) |= b; }
    inline fed_t operator | (const fed_t& a, const dbm_t& b) { return fed_t(a) |= b; }
    inline fed_t operator | (const dbm_t& a, const fed_t& b) { return fed_t(b) |= a; }
    inline fed_t operator | (const fed_t& a, const fed_t& b) { return fed_t(a) |= b; }

    inline fed_t operator - (const fed_t& a, const raw_t* b) { return fed_t(a) -= b; }
    inline fed_t operator - (const fed_t& a, const dbm_t& b) { return fed_t(a) -= b; }
    inline fed_t operator - (const dbm_t& a, const fed_t& b) { return fed_t(a) -= b; }
    inline fed_t operator - (const fed_t& a, const fed_t& b) { return fed_t(a) -= b; }

    inline fed_t operator - (const dbm_t& arg1, const raw_t* arg2) { return fed_t::subtract(arg1, arg2); }
    inline fed_t operator - (const dbm_t& arg1, const dbm_t& arg2) { return fed_t::subtract(arg1, arg2); }

    inline fed_t operator ! (const dbm_t& arg) { return fed_t(arg.getDimension()).setInit() -= arg; }
    inline fed_t operator ! (const fed_t& arg) { return fed_t(arg.getDimension()).setInit() -= arg; }
}
