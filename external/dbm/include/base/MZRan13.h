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
// Filename : MZRan13.h
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: MZRan13.h,v 1.2 2004/09/08 09:02:57 johanb Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_MZRAN13
#define INCLUDE_BASE_MZRAN13

#include "base/inttypes.h"

namespace base 
{

/* 32-bit (pseudo) random number generator.
 * Random number generator due to Marsaglia and Zaman.
 * Generates 32-bit numbers with a period of about 2^125.
 *
 * @section Reference
 * Marsaglia, G. and A. Zaman. 1994. Some portable very-long period random
 * number generators. Computers in Physics.  8(1): 117-121. 
 */
    class MZRan13 
    {
    public:
        /* Constructor
         * @param seed: The initial random seed.
         */
        MZRan13(uint32_t seed);

        /* Generate random number
         * @return the next pseudorandom number.
         */
        uint32_t operator()();
    private:
        uint32_t x, y, z, n, c;
    };

    inline MZRan13::MZRan13(uint32_t s)
        : x(s ^ 521288629), y(s ^ 362436069)
        , z(s ^ 16163801), n(s ^ 1131199209)
    {
        c = (y > z ? 1 : 0);
    }

    inline uint32_t MZRan13::operator()() 
    {
        uint32_t s;
        if (y > x + c) { 
            s = y - (x + c); 
            c = 0;
        } else {
            s = (y - (x + c) - 18) & 0xffffffff; 
            c = 1;
        }
        x = y;
        y = z;   

        return ((z = s) + (n = 69069 * n + 1013904243)) & 0xffffffff;
    }
}

#endif /* INCLUDE_BASE_MZRAN13 */
