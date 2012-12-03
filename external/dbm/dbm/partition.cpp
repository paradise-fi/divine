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
// Filename : partition.cpp
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: partition.cpp,v 1.13 2005/10/17 17:11:13 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#include "dbm/partition.h"
#include "base/intutils.h"
#ifndef NDEBUG
#include "dbm/print.h"
#endif

#define FTABLE partition_t::fedtable_t

#ifndef REDUCE
//#define REDUCE convexReduce
//#define REDUCE partitionReduce
//#define REDUCE convexReduce().expensiveReduce
//#define REDUCE expensiveConvexReduce
#define REDUCE mergeReduce
//#define REDUCE reduce
//#define REDUCE expensiveReduce
//#define REDUCE reduce().mergeReduce
//#define REDUCE noReduce
#endif
#ifndef BIGREDUCE
#define BIGREDUCE expensiveConvexReduce
#endif

//#define REDUCE_SKIP(S) REDUCE()
#define REDUCE_SKIP(S) mergeReduce(S)
//#define REDUCE_SKIP(S) mergeReduce(0,2)

namespace dbm
{
    void partition_t::intern()
    {
        assert(fedTable);

        if (isPtr())
        {
            entry_t** start = fedTable->getTable();
            entry_t** end = start + fedTable->getSize();
            for(; start < end; ++start)
            {
                if (*start) (*start)->fed.intern();
            }
        }
    }

    void partition_t::add(uintptr_t id, fed_t& fed)
    {
        assert(fedTable);

        if (!isPtr())
        {
            fedTable = fedtable_t::create(edim(), eflag());
            // Now it is mutable.
        }
        else
        {
            checkMutable();
        }
        if (fedTable->add(id, fed))
        {
            fedTable = fedTable->larger();
        }
    }
        
    FTABLE* FTABLE::create(cindex_t dim, bool noP)
    {
        // call placement constructor
        return new (new char[sizeof(FTABLE)+INIT_SIZE*sizeof(entry_t*)]) FTABLE(dim, noP);
    }

    void FTABLE::remove()
    {
        assert(refCounter == 0);

#ifndef NDEBUG
        if (!noPartition)
        {
            const entry_t* const* start = getBeginTable();
            const entry_t* const* end = getEndTable();
            for(const entry_t* const* i = start; i < end; ++i) if (*i)
            {
                for(const entry_t* const* j = start; j < end; ++j) if (*j && i != j)
                {
                    assert((*i)->fed.getDimension() <= 1 || (((*i)->fed & (*j)->fed).isEmpty()));
                }
            }
        }
#endif

        uint32_t n = mask+1;
        for(uint32_t i = 0; i < n; ++i)
        {
            delete table[i]; // delete NULL is fine
        }

        all.nil();
        // allocate as char*, deallocate as char*
        delete [] ((char*)this);
    }

    fed_t FTABLE::get(uint32_t id) const
    {
        uint32_t index = id & mask;
        while(table[index] && table[index]->id != id)
        {
            index = (index + 1) & mask;
        }
        return table[index] ? table[index]->fed : fed_t(getDimension());
    }

    bool FTABLE::add(uint32_t id, fed_t& fedi)
    {
        // Partition invariant if !noPartition, otherwise keep partition only for id == 0.
        fed_t newi = fedi;

        if (!(noPartition && id != 0))
        {
            newi -= all;
        }
        /*
        else
        {
            uint32_t id0 = 0;
            while(table[id0] && table[id0]->id != 0)
            {
                id0 = (id0 + 1) & mask;
            }
            if (table[id0]) newi -= table[id0]->fed;
        }
        */

        if (newi.isEmpty())
        {
            fedi.setEmpty();
            return false;
        }
        
        // Add new federation to all (cheapest way).
        if (newi.REDUCE().size() < fedi.size())
        {
            size_t s = all.size();
            if (s > 0)
            {
                fed_t copy = newi;
                all.appendEnd(copy).REDUCE_SKIP(s);
            }
            else
            {
                all = newi; // newi already reduce.
            }
            fedi.setEmpty();
        }
        else
        {
            size_t s = all.size();
            all.appendEnd(fedi).REDUCE_SKIP(s);
        }

        // Find right entry for id.
        uint32_t index = id & mask;
        while(table[index] && table[index]->id != id)
        {
            index = (index + 1) & mask;
        }

        if (!table[index]) // New entry;
        {
            table[index] = new entry_t(id, newi);
            nbEntries++;
            newi.nil();
        }
        else // Add to existing entry.
        {
            size_t s = table[index]->fed.size();
            table[index]->fed.appendEnd(newi).REDUCE_SKIP(s);
        }
        table[index]->fed.intern();

        // Return = needs rehash?
        return 4*nbEntries > 3*(mask+1);
    }

    // Move the entries to a larger table.
    FTABLE* FTABLE::larger()
    {
        assert(isMutable());

        uint32_t oldN = mask+1;
        uint32_t newMask = (oldN << 1) - 1;
        FTABLE* ftab = new // placement constructor
            (new char[sizeof(FTABLE)+(newMask+1)*sizeof(entry_t*)])
            FTABLE(getDimension(), nbEntries, newMask, noPartition);
        base_resetSmall(ftab->table, (newMask+1)*(sizeof(void*)/sizeof(int)));

        ftab->all = all;
        for(uint32_t i = 0; i < oldN; ++i) if (table[i])
        {
            cindex_t j = table[i]->id & newMask;
            while(ftab->table[j])
            {
                j = (j + 1) & newMask;
            }
            ftab->table[j] = table[i];
        }

        all.nil();
        delete [] ((char*)this);
        return ftab;
    }

    size_t FTABLE::getNumberOfDBMs() const
    {
        size_t nb = 0;
        const entry_t* const* start = table;
        const entry_t* const* end = start + getSize();
        for(; start < end; ++start)
        {
            if (*start) nb += (*start)->fed.size();
        }
        return nb;
    }

    // Deep copy of fedtable_t
    FTABLE* FTABLE::copy()
    {
        uint32_t n = mask+1;
        FTABLE* ftab = new // placement constructor
            (new char[sizeof(FTABLE)+n*sizeof(entry_t*)])
            FTABLE(getDimension(), nbEntries, mask, noPartition);

        ftab->all = all;
        for(uint32_t i = 0; i < n; ++i)
        {
            ftab->table[i] =
                table[i] != NULL
                ? new entry_t(table[i]->id, table[i]->fed)
                : NULL;
        }

        assert(refCounter > 1);
        refCounter--;
        
        return ftab;
    }
}
