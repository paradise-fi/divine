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
// Filename : DBMAllocator.cpp
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: DBMAllocator.cpp,v 1.3 2005/05/24 19:13:24 adavid Exp $$
//
///////////////////////////////////////////////////////////////////

#include "DBMAllocator.h"
#include "debug/new.h"

#ifndef ENABLE_DBM_NEW

namespace dbm
{
    /* Go through the list of deallocated DBMs
     * and delete them all (ie really free memory).
     */
    void DBMAllocator::cleanUp()
    {
        idbm_t **begin;
        for(begin = freeList.begin(); begin < freeList.end(); ++begin)
        {
            idbm_t *current, *next;
            for(current = *begin, *begin = NULL;
                current != NULL; current = next)
            {
                next = current->getNext();
                delete [] reinterpret_cast<int32_t*>(current);
            }
        }
    }

    ///< Wrapper function
    void* dbm_new(cindex_t dim)
    {
        return dbm_allocator.alloc(dim);
    }

    ///< Wrapper function
    void dbm_delete(idbm_t *dbm)
    {
        assert(dbm);
        dbm_allocator.dealloc(dbm);
    }

    ///< Wrapper function
    void cleanUp()
    {
        dbm_allocator.cleanUp();
    }

#endif // ENABLE_DBM_NEW

    // Instances.
    DBMTable dbm_table;
    DBMAllocator dbm_allocator;
    base::ItemAllocator<alloc_ifed_t> ifed_allocator(200);
    base::ItemAllocator<alloc_fdbm_t> fdbm_allocator(600);
}


