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

// -*- Mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 2006, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////////////////

#include "dbm/pfed.h"

#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <memory>
#include <boost/bind.hpp>

using boost::bind;
using std::for_each;
using std::accumulate;
using std::find_if;

namespace dbm
{
    std::allocator<pfed_t::pfed_s> pfed_t::alloc;

    void pfed_t::decRef()
    {
        if (!--(ptr->count))
        {
            alloc.destroy(ptr);
            alloc.deallocate(ptr, 1);
        }
    }

    void pfed_t::cow()
    {
        assert(ptr->count > 1);

        ptr->count--;
        pfed_s *old = ptr;
        ptr = alloc.allocate(1);
        alloc.construct(ptr, pfed_s());
        ptr->zones = old->zones;
        ptr->dim = old->dim;
        ptr->count = 1;
    }

    pfed_t::pfed_t() 
    {
        ptr = alloc.allocate(1);
        alloc.construct(ptr, pfed_s());
        ptr->dim = 0;
        ptr->count = 1;
    }

    pfed_t::pfed_t(cindex_t dim)
    {
        ptr = alloc.allocate(1);
        alloc.construct(ptr, pfed_s());
        ptr->dim = dim;
        ptr->count = 1;
    }

    pfed_t::pfed_t(const PDBM pdbm, cindex_t dim)
    {
        ptr = alloc.allocate(1);
        alloc.construct(ptr, pfed_s());
        ptr->dim = dim;
        ptr->count = 1;
        add(pdbm, dim);
    }

    pfed_t::iterator pfed_t::erase(iterator i)
    {
        assert(ptr->count <= 1);
        return ptr->zones.erase(i);
    }

#define for_each__(Z) for (iterator Z = beginMutable(), Z_ = endMutable(); Z != Z_; zone++)
#define for_each_(Z) for (iterator Z = beginMutable(), Z_ = endMutable(); Z != Z_; )
#define for_each_const(Z) for (const_iterator Z = begin(), Z_ = end(); Z != Z_; zone++)

    bool pfed_t::constrain(cindex_t i, uint32_t value)
    {
        constraint_t c[2] = { 
            constraint_t(i, 0, dbm_bound2raw(value, dbm_WEAK)),
            constraint_t(0, i, dbm_bound2raw(-value, dbm_WEAK))
        };
        return constrain(c, 2);    
    }

    bool pfed_t::constrain(cindex_t i, cindex_t j, raw_t constraint)
    {    
        // lambda x . pdbm_constrain1(x, ptr->dim, i, j, constraint)

        erase_if_not(bind(pdbm_constrain1, _1, ptr->dim, i, j, constraint));
        return !isEmpty();
    }

    bool pfed_t::constrain(const constraint_t *constraints, size_t n)
    {
        erase_if_not(bind(pdbm_constrainN, _1, ptr->dim, constraints, n));
        return !isEmpty();
    }

    static int32_t min(int32_t a, int32_t b)
    {
        return a < b ? a : b;
    }

    int32_t pfed_t::getInfimum() const
    {
        /* The binary operator used in the following accumulation is
         * lambda inf zone.min(inf, pdbm_getInfimum(zone, ptr->dim));
         */
        return accumulate(begin(), end(), INT_MAX, 
                          bind(min, _1, bind(pdbm_getInfimum, _2, ptr->dim)));
    }

    int32_t pfed_t::getInfimumValuation(IntValuation &valuation, const bool *free) const
    {
        uint32_t dim = ptr->dim;
        int32_t infimum = INT_MAX;
        int32_t copy[dim];

        for_each_const(zone) 
        {
            std::copy(valuation.begin(), valuation.end(), copy);
            int32_t inf = pdbm_getInfimumValuation(*zone, dim, copy, free);
            if (inf < infimum)
            {
                infimum = inf;
                std::copy(copy, copy + dim, valuation.begin());
            }
        }
        return infimum;
    }

    bool pfed_t::satisfies(cindex_t i, cindex_t j, raw_t constraint) const
    {
        return find_if(
            begin(), end(), 
            bind(pdbm_satisfies, _1, ptr->dim, i, j, constraint)) != end();
    }

    bool pfed_t::isUnbounded() const
    {
        return find_if( 
            begin(), end(), bind(pdbm_isUnbounded, _1, ptr->dim)) != end();
    }

    uint32_t pfed_t::hash(uint32_t seed) const
    {
        /* The binary operator used in the following accumulation is
         * lambda seed zone . pdbm_hash(zone, ptr->dim, seed)
         */
        return accumulate(begin(), end(), seed,
                          bind(pdbm_hash, _2, ptr->dim, _1));
    }

    bool pfed_t::contains(const IntValuation &valuation) const
    {
        return find_if(begin(), end(),
                       bind(pdbm_containsInt, _1, ptr->dim, valuation())) != end();
    }

    bool pfed_t::contains(const DoubleValuation &valuation) const
    {
        return find_if(begin(), end(),
                       bind(pdbm_containsDouble, _1, ptr->dim, valuation())) != end();
    }

    bool pfed_t::containsWeakly(const IntValuation &valuation) const
    {
        return find_if(begin(), end(),
                       bind(pdbm_containsIntWeakly, _1, ptr->dim, valuation())) != end();
    }

    relation_t pfed_t::relation(const pfed_t &b) const
    {
        /* For symmetry, we call (*this) for a. 
         */
        const pfed_t &a = *this;

        assert(a.getDimension() == b.getDimension());

        /* a_s[i] becomes true if zone i of a is contained in some
         * zone of b.

         * b_s[j] becomes true if zone j of b is contained in some
         * zone of a.
         */
        cindex_t dim = ptr->dim;
        size_t size_a = a.size();
        size_t size_b = b.size();
        bool a_s[size_a];
        bool b_s[size_b];
        bool *p_a, *p_b;
        const_iterator i, j;
        
        // Stack allocated obj are aligned so the override of
        // resetSmall is harmless.
        base_resetSmall(a_s, intsizeof(bool[size_a]));
        base_resetSmall(b_s, intsizeof(bool[size_b]));

        for (i = a.begin(), p_a = a_s; i != a.end(); ++i, ++p_a)
        {
            for (j = b.begin(), p_b = b_s; j != b.end(); ++j, ++p_b)
            {
                if (!*p_a || !*p_b)
                {
                    switch (pdbm_relation(*i, *j, dim))
                    {
                    case base_DIFFERENT:
                        break;
                    case base_SUPERSET:
                        *p_b = true;
                        break;
                    case base_SUBSET:
                        *p_a = true;
                        break;
                    case base_EQUAL:
                        *p_b = true;
                        *p_a = true;
                        break;
                    }
                }
            }
        }

        /* a is a subset of b if all zones of a are contained in some
         * zone of b, and vice versa.
         */
        for (p_a = a_s; p_a < a_s + size_a && *p_a; ++p_a)
        {
        }

        for (p_b = b_s; p_b < b_s + size_b && *p_b; ++p_b)
        {
        }

        return (relation_t)
            ((p_a == a_s + size_a ? base_SUBSET : 0) |
             (p_b == b_s + size_b ? base_SUPERSET : 0));
    }

    relation_t pfed_t::exactRelation(const pfed_t &b) const
    {
        assert(size() == 1 && b.size() == 1);
        return relation(b);
    }

    void pfed_t::up()
    {
        for_each(beginMutable(), endMutable(), bind(pdbm_up, _1, ptr->dim));
    }

    void pfed_t::up(int32_t rate)
    {
        assert(rate >= 0);

        uint32_t dim = ptr->dim;

        prepare();

        for_each__ (zone)
        {
            cindex_t x;
            int32_t oldrate = pdbm_getSlopeOfDelayTrajectory(*zone, dim);

            if (rate == oldrate)
            {
                /* This is a simple case which does not require splits.
                 */
                pdbm_up(*zone, dim);
            }
            else if (pdbm_findZeroCycle(*zone, dim, 0, &x))
            {
                /* This is a simple case which does not require splits.
                 */
                pdbm_upZero(*zone, dim, rate, x);
            }
            else if (rate < oldrate)
            {
                /* New rate is smaller than old rate, so we delay from the
                 * lower facets
                 */
                uint32_t facets[dim];
                uint32_t count = pdbm_getLowerFacets(*zone, dim, facets);
        
                for (uint32_t j = 0; j < count - 1; j++) 
                {
                    ptr->zones.push_front(*zone);
                    pdbm_constrainToFacet(ptr->zones.front(), dim, 0, facets[j]);
                    pdbm_upZero(ptr->zones.front(), dim, rate, facets[j]);
                }
                pdbm_constrainToFacet(*zone, dim, 0, facets[count - 1]);
                pdbm_upZero(*zone, dim, rate, facets[count - 1]);
            } 
            else 
            {
                assert(rate > oldrate);

                /* New rate is bigger than old rate, so we delay from the
                 * upper facets. We also need the original zone.
                 */
                uint32_t facets[dim];
                uint32_t count = pdbm_getUpperFacets(*zone, dim, facets);

                for (uint32_t j = 0; j < count; j++) 
                {
                    ptr->zones.push_front(*zone);
                    pdbm_constrainToFacet(ptr->zones.front(), dim, facets[j], 0);
                    pdbm_upZero(ptr->zones.front(), dim, rate, facets[j]);
                }
                pdbm_constrainToFacet(*zone, dim, facets[count - 1], 0);
                pdbm_upZero(*zone, dim, rate, facets[count - 1]);
            }
        }
    }

    void pfed_t::updateValueZero(cindex_t clock, int32_t value, cindex_t zero)
    {
        for_each(beginMutable(), endMutable(),
                 bind(pdbm_updateValueZero, _1, ptr->dim, clock, value, zero));
    }

    void pfed_t::updateValue(cindex_t clock, uint32_t value)
    {
        uint32_t dim = ptr->dim;

        prepare();

        for_each__ (zone)
        {
            cindex_t k;
            int32_t rate = pdbm_getRate(*zone, dim, clock);
      
            if (rate == 0)
            {
                pdbm_updateValue(*zone, dim, clock, value);
            }
            else if (pdbm_findZeroCycle(*zone, dim, clock, &k))
            {
                pdbm_updateValueZero(*zone, dim, clock, value, k);
            }
            else if (rate > 0)
            {        
                /* Find and reset lower facets when rate is positive.
                 */
                uint32_t facets[dim];
                int32_t count = pdbm_getLowerRelativeFacets(*zone, dim, clock, facets);
          
                assert(count >= 1);
                for (int32_t j = 0; j < count - 1; j++) 
                {
                    ptr->zones.push_front(*zone);
                    pdbm_constrainToFacet(ptr->zones.front(), dim, facets[j], clock);
                    pdbm_updateValueZero(ptr->zones.front(), dim, clock, value, facets[j]);
                }
                pdbm_constrainToFacet(*zone, dim, facets[count - 1], clock);
                pdbm_updateValueZero(*zone, dim, clock, value, facets[count - 1]);
            }
            else
            {
                /* Find and reset upper facets when rate is negative.
                 */
                uint32_t facets[dim];
                int32_t count = pdbm_getUpperRelativeFacets(*zone, dim, clock, facets);
        
                assert(count >= 1);
                for (int32_t j = 0; j < count - 1; j++) 
                {
                    ptr->zones.push_front(*zone);
                    pdbm_constrainToFacet(ptr->zones.front(), dim, clock, facets[j]);
                    pdbm_updateValueZero(ptr->zones.front(), dim, clock, value, facets[j]);
                }
                pdbm_constrainToFacet(*zone, dim, clock, facets[count - 1]);
                pdbm_updateValueZero(*zone, dim, clock, value, facets[count - 1]);
            }
        }
    }

    void pfed_t::extrapolateMaxBounds(int32_t *max)
    {
        for_each(beginMutable(), endMutable(), 
                 bind(pdbm_extrapolateMaxBounds, _1, ptr->dim, max));
    }

    void pfed_t::diagonalExtrapolateMaxBounds(int32_t *max)
    {
        for_each(beginMutable(), endMutable(), 
                 bind(pdbm_diagonalExtrapolateMaxBounds, _1, ptr->dim, max));
    }

    void pfed_t::diagonalExtrapolateLUBounds(int32_t *lower, int32_t *upper)
    {
        for_each(
            beginMutable(), endMutable(), 
            bind(pdbm_diagonalExtrapolateLUBounds, _1, ptr->dim, lower, upper));
    }

    void pfed_t::incrementCost(int32_t value)
    {
        for_each(beginMutable(), endMutable(), 
                 bind(pdbm_incrementCost, _1, ptr->dim, value));
    }
    
    int32_t pfed_t::getCostOfValuation(const IntValuation &valuation) const
    {
        /* The binary operator used in the following accumulation is
         * lambda x y . min(x, pdbm_getCostOfValuation(y, dim, val))
         */
        return accumulate(
            begin(), end(), INT_MAX,
            bind(min, _1, bind(pdbm_getCostOfValuation, _2, ptr->dim, valuation())));
    }

    void pfed_t::relax()
    {
        for_each(beginMutable(), endMutable(), 
                 bind(pdbm_relax, _1, ptr->dim));
    }

    void pfed_t::freeClock(cindex_t clock)
    {
        for_each(beginMutable(), endMutable(),
                 bind(pdbm_freeClock, _1, ptr->dim, clock));
    }

    void pfed_t::add(const PDBM pdbm, cindex_t dim)
    {
        assert(dim == ptr->dim);
        prepare();
        ptr->zones.push_front(pdbm_t(pdbm, dim));
    } 

    void pfed_t::setZero()
    {
        PDBM pdbm = NULL;
        pdbm_zero(pdbm, ptr->dim);
        *this = pfed_t(pdbm, ptr->dim);
    }

    void pfed_t::setInit()
    {
        PDBM pdbm = NULL;
        pdbm_init(pdbm, ptr->dim);
        *this = pfed_t(pdbm, ptr->dim);
    }

    void pfed_t::setEmpty()
    {
        *this = pfed_t(ptr->dim);
    }
    
    void pfed_t::freeUp(cindex_t i) 
    {
        for_each(beginMutable(), endMutable(), 
                 bind(pdbm_freeUp, _1, ptr->dim, i));
    }

    void pfed_t::freeDown(cindex_t i)
    {
        for_each(beginMutable(), endMutable(), 
                 bind(pdbm_freeDown, _1, ptr->dim, i));
    }

    pfed_t &pfed_t::operator = (const pfed_t &fed)
    {
        if (ptr != fed.ptr)
        {
            decRef();
            ptr = fed.ptr;
            incRef();
        }
        return *this;
    }

    pfed_t &pfed_t::operator |= (const pfed_t &fed)
    {
        // REVISIT: Eliminate included zones.
        assert(fed.ptr->dim == ptr->dim);
        prepare();
        ptr->zones.insert(ptr->zones.begin(), fed.begin(), fed.end());
        return *this;
    }

    pfed_t &pfed_t::operator |= (const PDBM pdbm)
    {
        // REVISIT: Eliminate included zones.
        add(pdbm, ptr->dim);
        return *this;
    }
    
    bool pfed_t::intersects(const pfed_t &) const
    {
        throw std::logic_error("pfed_t::intersects not implemented");
    }

    pfed_t &pfed_t::operator &= (const pfed_t &) 
    {
        throw std::logic_error("pfed_t::operator &= not implemented");
    }

    pfed_t pfed_t::operator & (const pfed_t &) const
    {
        throw std::logic_error("pfed_t::operator & not implemented");
    }

    pfed_t pfed_t::operator - (const pfed_t &) const
    {
        throw std::logic_error("pfed_t::operator - not implemented");
    }

    pfed_t &pfed_t::operator -= (const pfed_t &)
    {
        throw std::logic_error("pfed_t::operator -= not implemented");
    }

    pfed_t &pfed_t::operator += (const pfed_t &)
    {
        throw std::logic_error("pfed_t::operator += not implemented");
    }

    void pfed_t::down() 
    {
        throw std::logic_error("pfed_t::down not implemented");
    }

    int32_t pfed_t::getUpperMinimumCost(cindex_t) const
    {
        throw std::logic_error("pfed_t::getUpperMinimumCost not implemented");
    }

    void pfed_t::relaxUp()
    {
        throw std::logic_error("pfed_t::relaxUp not implemented");
    }

    void pfed_t::getValuation(DoubleValuation& cval, bool *freeC) const 
    {
        throw std::logic_error("pfed_t::getValuation not implemented");
    }

    void pfed_t::swapClocks(cindex_t, cindex_t)
    {
        throw std::logic_error("pfed_t::swapClocks not implemented");
    }    
    
    void pfed_t::splitExtrapolate(
        const constraint_t *begin, const constraint_t *end,
        const int32_t *max)
    {
        throw std::logic_error("pfed_t::splitExtrapolate not implemented");
    }

    void pfed_t::setDimension(cindex_t)
    {
        throw std::logic_error("pfed_t::setDimension not implemented");
    }
    
    raw_t* pfed_t::getDBM()
    {
        throw std::logic_error("pfed_t::getDBM not implemented");
    }
    
    const raw_t* pfed_t::const_dbm() const
    {
        throw std::logic_error("pfed_t::const_dbm not implemented");
    }

    pfed_t& pfed_t::convexHull()
    {
        throw std::logic_error("pfed_t::convexHull not implemented");
    }

    std::ostream &operator << (std::ostream &o, const pfed_t &f)
    {
        pfed_t::const_iterator first = f.begin();
        pfed_t::const_iterator last = f.end();
        while (first != last)
        {
            pdbm_print(o, *first, f.getDimension());
            first++;
        }
        return o;
    }
}
