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
// Filename : stats.cpp
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: $
//
///////////////////////////////////////////////////////////////////

#ifdef SHOW_STATS

#include <stdlib.h>
#include <cstring>
#include <iostream>

#include "hash/compute.h"
#include "base/stats.h"
#include "debug/macros.h"

using namespace std;

namespace base
{
    // Entry sorter function.
    static int cmpentry(const void *a, const void *b)
    {
        Stats::entry_t **first = (Stats::entry_t**) a;
        Stats::entry_t **second = (Stats::entry_t**) b;
        assert(a && b);
        return strcmp((*first)->stat.name, (*second)->stat.name);
    }
    
    Stats stats; // The global instance.

    Stats::~Stats()
    {
        // Convert table -> array.
        size_t n = table.getNbBuckets();
        entry_t **all = new entry_t*[n];
        entry_t **end = &all[n], **a = all;
        base::Enumerator<entry_t> tenum = table.getEnumerator();
        for(entry_t *e = tenum.getNext(); e != NULL; ++a, e = tenum.getNext())
        {
            assert(a < end);
            *a = e;
        }
        assert(a == end);
        // Sort entries.
        qsort(all, n, sizeof(entry_t*), cmpentry);

        // Show stats.
	cout.flush();
	cerr.flush();
        cerr << "\n******* Stats *******\n";
        for(a = all; a < end; ++a)
        {
            entry_t *s = *a;
            cerr << BLUE(THIN) << s->stat.name << NORMAL ": " BLUE(BOLD)
                 << s->stat.counter << NORMAL;
            for(stat_t *sub = s->stat.next; sub != NULL; sub = sub->next)
            {
                cerr << "; " BLUE(THIN) << sub->name << NORMAL ": " BLUE(BOLD)
                     << sub->counter << NORMAL;
                if (s->stat.counter)
                {
                    long pc = (100*sub->counter)/s->stat.counter;
                    cerr << " (" << (pc > 75 ? GREEN(BOLD) : (pc < 25 ? RED(BOLD) : CYAN(BOLD)))
                         << pc << "%" NORMAL ")";
                }
            }
            cerr << endl;
        }

        // Deallocate.
        delete [] all;
        table.resetDelete();
    }

    void Stats::count(const char *statName, const char *subStat)
    {
        uint32_t hashValue = hash_computeStr(statName, 0);
        entry_t **entry = table.getAtBucket(hashValue);
        entry_t *bucket = *entry;

        while(bucket)
        {
            if (bucket->info == hashValue &&
                strcmp(bucket->stat.name, statName) == 0)
            {
                if (!subStat) // Main stat.
                {
                    bucket->stat.counter++;
                }
                else // Sub-stat.
                {
                    stat_t *sub;
                    for(sub = bucket->stat.next; sub != NULL; sub = sub->next)
                    {
                        if (strcmp(sub->name, subStat) == 0)
                        {
                            sub->counter++;
                            return;
                        }
                    }
                    // New sub stat!
                    bucket->stat.next = new stat_t(subStat, bucket->stat.next);
                }
                return;
            }
            bucket = bucket->getNext();
        }
        // New stat!
        bucket = new entry_t(statName);
        bucket->link(entry);
        bucket->info = hashValue;
        table.incBuckets();
        if (subStat)
        {
            bucket->stat.next = new stat_t(statName, NULL);
            std::cerr << "Missing stat '" RED(BOLD) << statName << NORMAL "' added.\n";
        }
    }
}

#endif
