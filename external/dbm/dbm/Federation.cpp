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
// Filename : Federation.cpp (dbm)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: Federation.cpp,v 1.8 2005/05/03 19:46:52 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#include "dbm/Federation.h"
#include "debug/new.h"

namespace dbm
{
    Federation::~Federation()
    {
        reset();
    }

    
    /* Wrapper to dbm_cppPrintFederation
     */
    std::ostream& Federation::prettyPrint(std::ostream& os) const
    {
        size_t size = dbmf_getSize(dbmFederation.dbmList);
        cindex_t dim = dbmFederation.dim;
        os << size << " DBM"
           << (size > 1 ? "s " : " ")
           << dim << 'x' << dim << ' ';
        return dbm_cppPrintFederation(os, dbmFederation.dbmList, dim);
    }

    /* A federation = list of DBMs. Format of
     * serialization is:
     * - number of DBMs (n)
     * - dimension of DBMs (dim)
     * - n*dim*dim constraints for every DBM,
     *   in the order: for all 0<=k<n, for all 0<=i<dim, 0<=j<dim: DBMk[i,j]
     * Write the numbers separated by space. The numbers
     * are the raw constraints.
     */
    std::ostream& Federation::serialize(std::ostream& os) const
    {
        assert(dbmFederation.dim);

        os << ' '
           << dbmf_getSize(dbmFederation.dbmList)
           << ' '
           << dbmFederation.dim
           << ' ';

        const DBMList_t *dbmList = dbmFederation.dbmList;
        size_t dimdim = dbmFederation.dim*dbmFederation.dim;

        while(dbmList)
        {
            const raw_t *dbm = dbmList->dbm;
            size_t n = dimdim;
            do { os << *dbm++ << ' '; } while(--n);
            dbmList = dbmList->next;
        }

        return os;
    }


    /* Restore DBMs from a stream (as serialized by serialize()).
     * Separators may be space, newlines, or tabulations.
     */
    void Federation::unserialize(std::istream& is) throw (std::bad_alloc)
    {
        assert(dbmAllocator);

        // free existing DBMs
        if (dbmFederation.dbmList)
        {
            dbmf_deallocateList(dbmAllocator, dbmFederation.dbmList);
            dbmFederation.dbmList = NULL;
            // the federation is now in a consistent state even
            // if an exception is thrown.
        }

        size_t size;
        is >> size >> dbmFederation.dim;

        size_t dimdim = dbmFederation.dim*dbmFederation.dim;
        DBMList_t **dbmList = &dbmFederation.dbmList;
        assert(dimdim);

        // read all DBMs
        while(size)
        {
            *dbmList = dbmf_allocate(dbmAllocator);
            if (!dbmList) throw std::bad_alloc();

            // read every DBM
            raw_t *dbm = (*dbmList)->dbm;
            size_t n = dimdim;
            do { is >> *dbm++; } while(--n);

            dbmList = &(*dbmList)->next;
            --size;
        }

        *dbmList = NULL; // terminate list
    }


    /* There are 2 ways to do it:
     * - the longer:
            if (dbmFederation.dbmList)
            {
                dbm_zero(dbmFederation.dbmList->dbm, dbmFederation.dim);
                if (dbmFederation.dbmList->next)
                {
                    dbmf_deallocateList(dbmAllocator, dbmFederation.dbmList->next);
                    dbmFederation.dbmList->next = NULL;
                }
            }
            else
            {
                dbmf_addZero(&dbmFederation, dbmAllocator);
            }
     *  - the shorter: as implemented here.
     *    Efficiency is less important than size here.
     */
    void Federation::initToZero()
    {
        if (dbmFederation.dbmList)
        {
            dbmf_deallocateList(dbmAllocator, dbmFederation.dbmList);
        }
        dbmf_addZero(&dbmFederation, dbmAllocator);
    }


    /* Similar to initToZero but calls dbmf_addInit instead.
     */
    void Federation::initUnconstrained()
    {
        if (dbmFederation.dbmList)
        {
            dbmf_deallocateList(dbmAllocator, dbmFederation.dbmList);
        }
        dbmf_addInit(&dbmFederation, dbmAllocator);
    }
}
