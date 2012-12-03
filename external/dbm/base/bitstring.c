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
 * Filename : bitstring.c (base)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: bitstring.c,v 1.5 2005/04/22 15:20:08 adavid Exp $
 *
 **********************************************************************/

#include "base/bitstring.h"


/* Algorithm:
 * - go through bit table and index table in parallel
 * - write index when bit is set
 */
size_t base_bits2indexTable(const uint32_t *bits, size_t n, cindex_t *table)
{
    size_t index = 0;
    uint32_t b;
    cindex_t *t;
    assert(table && bits);

    for(; n != 0; table += 32, --n)
    {
        for(b = *bits++, t = table; b != 0; ++t, b >>= 1)
        {
            if (b & 1) *t = index++;
        }
    }

    return index;
}

