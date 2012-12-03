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
// Filename : Timer.h (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// v 1.3 reviewed.
// $Id: Timer.h,v 1.6 2004/04/02 22:50:43 behrmann Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_TIMER_H
#define INCLUDE_BASE_TIMER_H

#include <iostream>

namespace base
{
    /** A simple timer to measure CPU time.
     */
    class Timer
    {
    public:
        Timer();
        
        /** 
         * Access to CPU time. Returns the CPU time consumed since the
         * last call of this method or since initialization of the 
         * object if the method has not been called before.
         *
         * @returns CPU time in seconds.
         */
        double getElapsed();
        
    private:
        double startTime;
    };
}

/** 
 * Output operator for Timer class. Writes the consumed CPU time to \a
 * out. Calls \c getElapsed() internally, so the timer is reset.  Up
 * to 3 decimals are printed.
 */
std::ostream& operator << (std::ostream& out, base::Timer& t);

#endif // INCLUDE_BASE_TIMER_H
