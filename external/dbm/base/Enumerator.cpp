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
// Filename : Enumerator.cpp (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: Enumerator.cpp,v 1.1 2004/01/30 20:54:35 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#include "base/Enumerator.h"

namespace base
{
    // return NULL when no bucket left
    SingleLinkable_t* LinkableEnumerator::getNextLinkable()
    {
        SingleLinkable_t *item = current;
        if (item) // got a bucket, prepare for next
        {
            current = current->next;
        }
        else if (size)
        {
            // find 1st non null
            do { table++; } while(--size && !*table);

            // if valid
            if (size)
            {
                // while stop condition: nEntries == 0 || *table != NULL
                // if nEntries == 0, we are not here, item stays NULL
                // else *table not NULL and valid.
                assert(*table);

                item = *table;
                current = item->next;
            }
        }
        return item;
    }
}

