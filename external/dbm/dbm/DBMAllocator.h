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
// Filename : DBMAllocator.h
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: DBMAllocator.h,v 1.4 2005/07/22 12:55:54 adavid Exp $$
//
///////////////////////////////////////////////////////////////////

#ifndef DBM_DBMALLOCATOR_H
#define DBM_DBMALLOCATOR_H

#include "dbm/fed.h"
#include "base/array_t.h"

/** @file
 * Allocation of classes related to fed_t, dbm_t, etc.
 * Declaration of the DBM allocator and DBM table.
 */
#ifndef ENABLE_DBM_NEW
namespace dbm
{
    ///< Allocator for DBMs, only if we don't use new for them.
    class DBMAllocator
    {
    public:
        ///< Default constructor
        DBMAllocator()
            : freeList(16), dbm1x1(1) {
            raw_t lezero = dbm_LE_ZERO;
            dbm1x1.newCopy(&lezero, 1);
            dbm1x1.intern();
        }

        ///< free memory of the deallocated idbm_t
        void cleanUp();

        /** Allocate memory to instantiate an idbm_t
         * @param dim: dimension of the DBM
         */
        void* alloc(cindex_t dim) {
            idbm_t *dbm = freeList.get(dim);
            if (dbm)
            {
                freeList[dim] = dbm->getNext();
                return dbm;
            }
#ifdef ENABLE_STORE_MINGRAPH
            return new int32_t[intsizeof(idbm_t)+dim*dim+bits2intsize(dim*dim)];
#else
            return new int32_t[intsizeof(idbm_t)+dim*dim];
#endif
        }

        /** Deallocate an idbm_t
         * @param dbm: dbm to deallocate
         */
        void dealloc(idbm_t *dbm)
        {
            assert(dbm);
            cindex_t dim = dbm->getDimension();
            dbm->next = freeList.get(dim);
            freeList.set(dim, dbm);
        }

#ifdef ENABLE_MONITOR
        // only one global instance hidden here:
        // deallocation is not needed because it is done
        // only at program exit.
        ~DBMAllocator() {
            dbm1x1.nil();
            cleanUp();
        }
#endif

        ///< @return a constant DBM 1x1 (copies will make it non-mutable).
        dbm_t& dbm1() { assert(!dbm1x1.isEmpty()); return dbm1x1; }

    private:
        base::array_t<idbm_t*> freeList;
        dbm_t dbm1x1;
    };

    ///< Allocator instance
    extern DBMAllocator dbm_allocator;
}
#endif // ENABLE_DBM_NEW

#endif // DBM_FED_ALLOC_H
