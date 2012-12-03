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
// Filename : Timer.cpp
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// v 1.2 reviewed.
// $Id: Timer.cpp,v 1.4 2004/08/17 14:53:31 behrmann Exp $
//
///////////////////////////////////////////////////////////////////

#include <iomanip>
#include <iostream>

#include "base/Timer.h"
#include "base/platform.h"

namespace base
{

    Timer::Timer()
        : startTime(base_getTime())
    {}

    double Timer::getElapsed()
    {
        double now = base_getTime();
        double elapsed = now - startTime;
        startTime = now;
        return elapsed;
    }

}    

using namespace std;

ostream& operator << (ostream& out, base::Timer& t)
{
    // save old & set new crappy C++ iostuff
    ios_base::fmtflags oldFlags = out.flags();
    streamsize oldPrecision = out.precision(3);
    out.setf(ios_base::fixed, ios_base::floatfield);
 
    // print!
    out << t.getElapsed() << 's';
   
    // restore crappy C++ iostuff
    out.flags(oldFlags);
    out.precision(oldPrecision);

    return out;
}

