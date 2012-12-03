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

// -*- Mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2006, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DBM_PFED_H
#define DBM_PFED_H

#include <stdexcept>
#include "dbm/Valuation.h"
#include "dbm/priced.h"
#include "base/slist.h"

namespace dbm
{
    /**
     * Small C++ wrapper for the PDBM priced DBM type.
     *
     * The wrapper handles all reference counting, thus freeing you
     * from calling pdbm_incRef and pdbm_decRef yourself.
     *
     * It provides a type-conversion operator to PDBM, thus the
     * preferred way of working with priced DBMs is to use the C
     * functions directly on the wrapper type (the compiler will
     * handle the conversion for you).
     *
     * For compatibility with dbm_t (the C++ wrapper for normal DBMs),
     * a few selected additional methods have been implemented. This
     * also happens to be the only reason why the dimension of the
     * priced DBM is stored in the wrapper. However, the bulk of the
     * pdbm_X() functions are not made available as methods.
     */
    class pdbm_t
    {
    protected:
	PDBM pdbm;
	cindex_t dim;
    public:
	/**
	 * The default constructor constructs an empty priced DBM of
	 * dimension zero.  This must not be assigned to another
	 * pdbm_t.
	 */
	pdbm_t();

	/**
	 * Allocates a new empty priced DBM of the given
	 * dimension. This must not be assigned to another pdbm_t.
	 *
	 * @see pdbm_allocate()
	 */
	explicit pdbm_t(cindex_t);

	/**
	 * Makes a reference to the given priced DBM.
	 */
	pdbm_t(PDBM pdbm, cindex_t dim);

	/**
	 * Copy constructor. This will increment the reference count
	 * to the priced DBM given.
	 */
	pdbm_t(const pdbm_t &pdbm);

	/**
	 * Destructor. Reduces the reference count to the priced
	 * DBM. This in turn may result in its deallocation.
	 */
	~pdbm_t();

	/** 
	 * Assignment operator.
	 */
	pdbm_t &operator= (const pdbm_t &);

	/**
	 * Type conversion to PDBM. Notice that a reference to the
	 * PDBM is provided, thus the pdbm_t object can even be used
	 * with pdbm_X() functions modifying the priced DBM.
	 */
	operator PDBM&();

	/**
	 * Type conversion to constant PDBM.
	 */
	operator const PDBM() const;

        /**
	 * Returns a hash value for the priced DBM.
	 */
        uint32_t hash(uint32_t seed = 0) const;

        /**
         * Returns the bound on x(i) - x(j).
         */
        raw_t operator () (cindex_t i, cindex_t j) const;

	/** @see dbm_t::setDimension() */
        void setDimension(cindex_t dim);

        /** Returns the dimension of this priced DBM. */	 
        cindex_t getDimension() const;
	
	/** @see dbm_t::const_dbm() */
        const raw_t* const_dbm() const;

	/** @see dbm_t::getDBM() */
	raw_t* getDBM();

        /** @see dbm_t::analyseForMinDBM() */
        size_t analyzeForMinDBM(uint32_t *bitMatrix) const;

        /** @see dbm_t::writeToMinDBMWithOffset() */
        int32_t *writeToMinDBMWithOffset(bool minimizeGraph,
                                         bool tryConstraints16,
                                         allocator_t c_alloc,
                                         uint32_t offset) const;

        /** @see pdbm_relationWithMinDBM() */
        relation_t relation(const int32_t *, raw_t *) const;        
	
	/** @see dbm_t::freeClock() */
	pdbm_t& freeClock(cindex_t clock);

	/** @see dbm_t::getSizeOfMinDBM() */
	static size_t getSizeOfMinDBM(cindex_t dimension, const int32_t *);

	/** @see dbm_t::readFromMinDBM() */
        static pdbm_t readFromMinDBM(cindex_t dimension, const int32_t *);
    };

    inline pdbm_t::pdbm_t() : pdbm(NULL), dim(0)
    {

    }

    inline pdbm_t::pdbm_t(cindex_t dim) : pdbm(NULL), dim(dim)
    {

    }

    inline pdbm_t::pdbm_t(PDBM pdbm, cindex_t dim) : pdbm(pdbm), dim(dim)
    {
	pdbm_incRef(pdbm);
    }

    inline pdbm_t::pdbm_t(const pdbm_t &other) : pdbm(other.pdbm), dim(other.dim)
    {
	pdbm_incRef(pdbm);
    }

    inline pdbm_t::~pdbm_t()
    {
	pdbm_decRef(pdbm);
    }

    inline pdbm_t &pdbm_t::operator= (const pdbm_t &other)
    {	
	pdbm_incRef(other.pdbm);
	pdbm_decRef(pdbm);
	dim = other.dim;
	pdbm = other.pdbm;
	return *this;
    }
    
    inline pdbm_t::operator PDBM&()
    {
	return pdbm;
    }

    inline pdbm_t::operator const PDBM() const
    {
	return pdbm;
    }

    inline void pdbm_t::setDimension(cindex_t dim)
    {
	pdbm_decRef(pdbm);
	pdbm = NULL;
	this->dim = dim;
    }

    inline cindex_t pdbm_t::getDimension() const
    {
	return dim;
    }

    inline const raw_t* pdbm_t::const_dbm() const
    {
	return pdbm_getMatrix(pdbm, dim);
    }

    inline raw_t* pdbm_t::getDBM()
    {
	return pdbm_getMutableMatrix(pdbm, dim);
    }

    inline uint32_t pdbm_t::hash(uint32_t seed) const
    {
	return pdbm_hash(pdbm, dim, seed);
    }

    inline raw_t pdbm_t::operator () (cindex_t i, cindex_t j) const
    {
        return pdbm_getMatrix(pdbm, dim)[i * dim + j];
    }

    inline size_t pdbm_t::analyzeForMinDBM(uint32_t *bitMatrix) const
    {
        return pdbm_analyzeForMinDBM(pdbm, dim, bitMatrix);
    }

    inline int32_t *pdbm_t::writeToMinDBMWithOffset(bool minimizeGraph,
						    bool tryConstraints16,
						    allocator_t c_alloc,
						    uint32_t offset) const
    {        
        return pdbm_writeToMinDBMWithOffset(pdbm, dim,
                                            minimizeGraph, tryConstraints16, 
                                            c_alloc, offset);        
    }
    
    inline relation_t pdbm_t::relation(const int32_t *graph, raw_t *buffer) const
    {
        return pdbm_relationWithMinDBM(pdbm, dim, graph, buffer);
    }

    inline size_t pdbm_t::getSizeOfMinDBM(cindex_t dim, const int32_t *graph)
    {
        return dbm_getSizeOfMinDBM(graph + dim + 2) + dim + 2;
    }

    inline pdbm_t pdbm_t::readFromMinDBM(cindex_t dim, const int32_t *graph)
    {
        pdbm_t pdbm(dim);
        pdbm_readFromMinDBM(pdbm, dim, graph);
        return pdbm;
    }
        
    inline pdbm_t& pdbm_t::freeClock(cindex_t clock)
    {
	pdbm_freeClock(pdbm, dim, clock);
	return *this;
    }

    /**
     * Implementation of priced federations.
     *
     * A priced federation is essentially a list of priced DBMs.
     * Semantically, the priced federation represents the union of the
     * priced DBMs.
     * 
     * Priced federations implement copy-on-write using reference
     * counting.
     */
    class pfed_t
    {
    public:
        typedef std::slist<pdbm_t>::const_iterator const_iterator;

        typedef std::slist<pdbm_t>::iterator iterator;

    protected:

        struct pfed_s
        {
            std::slist<pdbm_t> zones;
            uint32_t count;
            cindex_t dim;
        };

        static std::allocator<pfed_s> alloc;

        /** Pointer to record holding the federation. */
        struct pfed_s *ptr;

        /** Increment reference count. */
        void incRef();

        /** Decrement reference count. */
        void decRef();

        /** Prepare federation for modification. */
        void prepare();

	/** Copy-on-write: Creates an unshared copy of the federation. */
	void cow();

        /**
         * Adds \a pdbm to the federation. The reference count on pdbm
         * is incremented by one.
         */
        void add(const PDBM pdbm, cindex_t dim);    
    public:

        /** Allocate empty priced federation of dimension 0. */
        pfed_t();

        /** Allocate empty priced federation of dimension \a dim. */
        explicit pfed_t(cindex_t dim);

        /**
         * Allocate a priced federation of dimension \a dim initialised to
         * \a pdbm.
         */
        pfed_t(const PDBM pdbm, cindex_t dim);

        /** The copy constructor implements copy on write. */
        pfed_t(const pfed_t &);

        /**
         * The destructor decrements the reference count and deallocates
         * the priced DBM when the count reaches zero.
         */
        ~pfed_t();

        /**
         * Constrain x(i) to value. 
         *
         * @return Returns true if non-empty after operation.
         */
        bool constrain(cindex_t i, uint32_t value);

        /**
         * Constrain federation to valuations satisfying x(i) - x(j) ~
         * constraint.
         *
         * @return Returns true if non-empty after operation.
         */
        bool constrain(cindex_t i, cindex_t j, raw_t constraint);

        /**
         * Constrain federation to valuations satisfying x(i) - x(j) ~
         * (b, isStrict).
         *
         * @return Returns true if non-empty after operation.
         */
        bool constrain(cindex_t i, cindex_t j, int32_t b, bool isStrict);

        /**
         * Constrain federation to valuations satisfying the
         * constraints.
         *
         * @param constraints Array of length \a n of constraints.
         * @param n           length of \a constraints.
         * @pre               n > 0
         * @return Returns true if non-empty after operation.
         */
        bool constrain(const constraint_t *constraints, size_t n);

        /**
         * Constrain federation to valuations satisfying \a c.
         *
         * @return Returns true if non-empty after operation.
         */
        bool constrain(const constraint_t& c);

        /** Returns the infimum of the federation. */
        int32_t getInfimum() const;

        /**
         * Check if the federation satisfies a given constraint.        
         *
         * @return Returns true if the federation satisfies \a c.
         */
        bool satisfies(const constraint_t& c) const;

        /**
         * Check if the federation satisfies a given constraint.        
         *
         * @return Returns true if the federation satisfies x(i) -
         * x(j) ~ constraint.
         */
        bool satisfies(cindex_t i, cindex_t j, raw_t constraint) const;

        /** Returns true iff the federation is empty. */
        bool isEmpty() const;

        
        bool isUnbounded() const;

        /** Contains a hash of the federation. */
        uint32_t hash(uint32_t seed) const;

        /** Returns true iff the federation contains \v. */
        bool contains(const DoubleValuation &v) const;

        /** Returns true iff the federation contains \v. */
        bool contains(const IntValuation &v) const;

        /** 
	 * Returns true iff the federation contains \v, ignoring
	 * strictness of constraints.
	 */
        bool containsWeakly(const IntValuation &v) const;

        /** Delay with the current delay rate. */
        void up();

        /** Delay with rate \a rate. */
        void up(int32_t rate);

        /** Set x(clock) to \a value. */
        void updateValue(cindex_t clock, uint32_t value);

        /** 
         * Set x(clock) to \a value. x(zero) must be on a zero-cycle with
         * x(clock).
         */
        void updateValueZero(cindex_t clock, int32_t value, cindex_t zero);

        void extrapolateMaxBounds(int32_t *max);
        void diagonalExtrapolateMaxBounds(int32_t *max);
        void diagonalExtrapolateLUBounds(int32_t *lower, int32_t *upper);

        void incrementCost(int32_t value);
    
        int32_t getCostOfValuation(const IntValuation &valuation) const;
        void relax();

        void freeClock(cindex_t clock);

        /**
         * Resets the federation to the federation containing only the
         * origin, with a cost of 0.
         */
        void setZero();

        /**
         * Resets the federation to the federation containing all
         * valuations, with a cost of 0.
         */
        void setInit();

        /**
         * Resets the federation to the empty federation.
         */
        void setEmpty();

        /**
         * Returns an iterator to the first zone of the federation.
         */
        const_iterator begin() const;

        /**
         * Returns an iterator to the position after the last zone of
         * the federation.
         */
        const_iterator end() const;

        /** 
	 * Returns a mutable iterator to the beginning of the federation. 
	 */
        iterator beginMutable();

        /**
	 * Returns a mutable iterator to the end of the federation. 
	 */
        iterator endMutable();

        /**
         * Erases a DBM from the federation and returns an iterator to
         * the successor element.
         */
        iterator erase(iterator);

        bool operator == (const pfed_t &) const;

        /** Assignment operator. */
        pfed_t &operator = (const pfed_t &);

        /** Assignment operator. */
        pfed_t& operator = (const PDBM);

        /** Union operator. */
        pfed_t &operator |= (const pfed_t &);

        /** Union operator. */
        pfed_t &operator |= (const PDBM);

        /** Not implemented. */
        pfed_t &operator -= (const pfed_t &);

        /** Not implemented. */
        pfed_t &operator += (const pfed_t &);

        /** Not implemented. */
        pfed_t &operator &= (const pfed_t &);

        /** Not implemented. */
        bool intersects(const pfed_t &) const;

        /** Not implemented. */
        pfed_t operator & (const pfed_t& b) const;

        /** Not implemented. */
        pfed_t operator - (const pfed_t& b) const;

        /** Not implemented. */
        void down();

        /** Not implemented. */
        int32_t getUpperMinimumCost(cindex_t) const;

        /** Not implemented. */
        void relaxUp();

        /** Not implemented. */
        void getValuation(DoubleValuation& cval, bool *freeC = NULL) const;

        /** Not implemented. */
        void swapClocks(cindex_t, cindex_t);

        /** Not implemented. */        
        void splitExtrapolate(const constraint_t *, const constraint_t *,
                              const int32_t *);

        /** Not implemented (REVISIT). */        
	void setDimension(cindex_t);

        /** Not implemented. */        
        raw_t* getDBM();

        /** Not implemented. */        
        const raw_t* const_dbm() const;	

        /** Not implemented. */        
        pfed_t& convexHull();

        /**
         * Relation between two priced federations: SUBSET is returned
         * if all zones of this federation are contained in some zone
         * of fed. SUPERSET is returned if all zones of \a fed are
         * contained in some zone of this federation. EQUAL is
         * returned if both conditions are true. DIFFERENT is returned
         * if none of them are true.
         *
         * Notice that this implementation of set inclusion is not
         * complete in the sense that DIFFERENT could be returned even
         * though the two federations are comparable.
         */
        relation_t relation(const pfed_t &fed) const;

	/**
	 * Exact version of relation(). Only correct if both
	 * federations have size one.
	 */
        relation_t exactRelation(const pfed_t &fed) const;

        /**
         * Returns the infimum of the federation given a partial
         * valuation.
         */
        int32_t getInfimumValuation(IntValuation &valuation, const bool *free = NULL) const;

        /** Returns the number of zones in the federation. */
        size_t size() const;

        /** Returns the dimension of the federation. */
        cindex_t getDimension() const;

        /**
         * Implementation of the free up operation for priced
         * federations.
         *
         * @see pdbm_freeUp
         */
        void freeUp(cindex_t);

        /**
         * Implementation of the free down operation for priced
         * federations.
         *
         * @see pdbm_freeDown
         */
        void freeDown(cindex_t);

        template <typename Predicate>
        void erase_if(Predicate p);

        template <typename Predicate>
        void erase_if_not(Predicate p);
        
        const pdbm_t &const_dbmt() const;
        pdbm_t &dbmt();
    };

    template <typename Predicate>
    inline void pfed_t::erase_if(Predicate p)
    {
        iterator first = beginMutable();
        iterator last = endMutable();
        while (first != last)
        {
            if (p(*first))
            {
                first = erase(first);
            }
            else
            {
                ++first;
            }
        }
    }

    template <typename Predicate>
    void pfed_t::erase_if_not(Predicate p) 
    {
        iterator first = beginMutable();
        iterator last = endMutable();
        while (first != last)
        {
            if (p(*first))
            {
                ++first;
            }
            else
            {
                first = erase(first);
            }
        }
    }


    inline pfed_t::pfed_t(const pfed_t &pfed) : ptr(pfed.ptr)
    {
        incRef();
    }

    inline pfed_t::~pfed_t()
    {
        assert(ptr->count > 0);
        decRef();
    }

    inline void pfed_t::prepare()
    {
        if (ptr->count > 1)
        {
	    cow();
	}
    }

    inline pfed_t::iterator pfed_t::beginMutable()
    {
        prepare();
        return ptr->zones.begin();
    }

    inline pfed_t::iterator pfed_t::endMutable()
    {
        prepare();
        return ptr->zones.end();
    }

    inline pfed_t::const_iterator pfed_t::begin() const
    {
        return ptr->zones.begin();
    }
 
    inline pfed_t::const_iterator pfed_t::end() const
    {
        return ptr->zones.end();
    }

    inline cindex_t pfed_t::getDimension() const
    {
        return ptr->dim;
    }

    inline size_t pfed_t::size() const
    {
        return ptr->zones.size();
    }

    inline void pfed_t::incRef()
    {
        ptr->count++;
    }


    inline bool pfed_t::isEmpty() const
    {
        return ptr->zones.empty();
    }

    inline const pdbm_t &pfed_t::const_dbmt() const 
    {
        assert(size() == 1);
        return ptr->zones.front();
    }

    inline pdbm_t &pfed_t::dbmt() 
    {
        assert(size() == 1);
        return ptr->zones.front();
    }

    inline pfed_t& pfed_t::operator = (const PDBM pdbm)
    {
        *this = pfed_t(pdbm, ptr->dim);
        return *this;
    }

    inline bool pfed_t::constrain(cindex_t i, cindex_t j, int32_t b, bool isStrict)
    {
        return constrain(i, j, dbm_boundbool2raw(b, isStrict));
    }

    inline bool pfed_t::constrain(const constraint_t& c)
    {
        return constrain(c.i, c.j, c.value);
    }

    inline bool pfed_t::satisfies(const constraint_t& c) const 
    {
        return satisfies(c.i, c.j, c.value);
    }

    std::ostream &operator << (std::ostream &, const pfed_t &);
}

#endif
