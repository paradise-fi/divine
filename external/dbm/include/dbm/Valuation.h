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
// Filename : Valuation.h
//
// IntValuation & DoubleValuation
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: Valuation.h,v 1.2 2005/04/25 16:38:27 behrmann Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DBM_VALUATION_H
#define INCLUDE_DBM_VALUATION_H

#include <sstream>
#include "base/pointer.h"
#include "dbm/ClockAccessor.h"
#include "debug/utils.h"

namespace dbm
{
    /** Valuation: like a fixed vector of scalars S with
     * some basic operations on it.
     * We don't use array_t because we don't want to
     * resize but we need basic memory alloc/dealloc.
     */
    template<class S>
    class Valuation : public base::pointer_t<S>
    {
    public:

        /** Constructor
         * @param size: size of the S valuation vector,
         * or dimension.
         */
        Valuation(size_t size)
            : base::pointer_t<S>(new S[size], size)
        { base::pointer_t<S>::reset(); }

        /** Copy constructor
         * @param original: vector to copy
         */
        Valuation(const Valuation<S>& original)
            : base::pointer_t<S>(new S[original.size()], original.size())
        {
            copyFrom(original);
        }

        ~Valuation() { delete [] base::pointer_t<S>::data; }

        /**
         * Copy the elements of the original vector.
         * It is dangerous not to have this operator.
         * @pre original.size() == size().
         */
        Valuation<S>& operator = (const Valuation<S>& original) {
            base::pointer_t<S>::copyFrom(original);
            return *this;
        }

        /** Add value to all the elements of this vector
         * EXCEPT element 0 (reference clock).
         * @param value: element to add.
         */
        Valuation<S>& operator += (S value)
        {
            if (value) 
            { 
                for(S *i = base::pointer_t<S>::begin()+1; i < base::pointer_t<S>::end(); ++i) 
                {
                    *i += value;
                }
            }
            return *this;
        }

        S last() const
        {
            assert(base::pointer_t<S>::size() > 0);
            return (*this)[base::pointer_t<S>::size()-1];
        }
        

        /** Subtract value to all the elements of this vector.
         * @param value: element to subtract.
         */
        Valuation<S>& operator -= (S value)
        {
            return *this += -value;
        }

        /** @return the delay between this point and the
         * argument point. This has any sense iff
         * argument point = this point + some delay.
         */
        S getDelayTo(const Valuation<S>& arg) const
        {
            if (base::pointer_t<S>::size() <= 1)
            {
                return 0;                // Only ref clock.
            }
            S delay = arg[1] - (*this)[1]; // Get delay.
#ifndef NDEBUG                           // Check consistency.
            size_t n = base::pointer_t<S>::size();
            for(size_t i = 1; i < n; ++i)
            {
                assert(arg[i] - (*this)[i] == delay);
            }
#endif
            return delay;
        }

        /** String representation of the valuation in a more
         * user friendly manner than just a vector of numbers.
         */
        std::string toString(const ClockAccessor& a) const
        {
            std::ostringstream os;
            cindex_t id = 1;
            os.precision(32);
            os <<  "[ ";
            for(const S *i = base::pointer_t<S>::begin()+1; i < base::pointer_t<S>::end(); ++id, ++i) 
            {
                os << a.getClockName(id) << "=" << *i << " ";
            }
            os << "]";
            return os.str();
        }
    };


    /** IntValuation = Valuation<int32_t>
     */
    typedef Valuation<int32_t> IntValuation;


    /** DoubleValuation = Valuation<double>
     */
    typedef Valuation<double> DoubleValuation;


    /** Overload += for automatic cast.
     */
    static inline
    DoubleValuation& operator += (DoubleValuation& d, int32_t v)
    { return d += (double) v; }
}

#endif // INCLUDE_DBM_VALUATION_H
