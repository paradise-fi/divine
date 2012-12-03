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
 * Filename : mingraph_cache.c
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: mingraph_cache.cpp,v 1.4 2005/07/22 12:55:54 adavid Exp $
 *
 *********************************************************************/

#ifdef ENABLE_MINGRAPH_CACHE

#include "base/stats.h"
#include "base/intutils.h"
#include "debug/macros.h"
#include "mingraph_cache.h"
#include <iostream>

#ifndef NDEBUG
#include "base/bitstring.h"
#endif

/* Default size for the cache, can redefined to anything > 0 */
/* Choose your favorite prime among http://primes.utm.edu/lists/small/1000.txt. */
#ifndef MINGRAPH_CACHE_SIZE
#define MINGRAPH_CACHE_SIZE 683
#endif

/* Simple cache entry */
typedef struct
{
    uint32_t hashValue;
    cindex_t dim;
    size_t cnt;
    uint32_t data[]; /* dbm[dim*dim] followed by bitMatrix */
} cache_t;

/* Simple cache implementation */
class Cache
{
public:
    Cache()
    {
        base_resetSmall(entries, MINGRAPH_CACHE_SIZE*(sizeof(void*)/sizeof(int)));
    }

    ~Cache() {
        // One static object, no need for deallocation.
#ifdef ENABLE_MONITOR
        for(uint32_t i = 0; i < MINGRAPH_CACHE_SIZE; ++i) {
            delete [] (uint32_t*) entries[i];
        }
#endif
    }

    cache_t* get(uint32_t hashValue) {
        return entries[hashValue % MINGRAPH_CACHE_SIZE];
    }

    void set(cache_t *entry) {
        entries[entry->hashValue % MINGRAPH_CACHE_SIZE] = entry;
    }

private:
    cache_t *entries[MINGRAPH_CACHE_SIZE];
};

static Cache cache;

size_t mingraph_getCachedResult(const raw_t *dbm, cindex_t dim, uint32_t *bitMatrix, uint32_t hashValue)
{
    cache_t* entry = cache.get(hashValue);
    uint32_t dim2;
    if (entry &&
        entry->hashValue == hashValue &&
        entry->dim == dim &&
        base_areEqual(entry->data, dbm, dim2 = dim*dim)) /* hit! */
    {
        RECORD_NSTAT(MAGENTA(BOLD)"DBM: Mingraph cache","hit");
        base_copySmall(bitMatrix, &entry->data[dim2], bits2intsize(dim2)); /* write result */
        assert(base_countBitsN(bitMatrix, bits2intsize(dim2)) == entry->cnt);
        return entry->cnt;
    }
    else /* miss! */
    {
        RECORD_NSTAT(MAGENTA(BOLD)"DBM: Mingraph cache","miss");
        return 0xffffffff;
    }
}

void mingraph_putCachedResult(const raw_t *dbm, cindex_t dim, const uint32_t *bitMatrix, uint32_t hashValue, size_t cnt)
{
    uint32_t dim2 = dim*dim;
    cache_t* entry = cache.get(hashValue);
    if (!entry || entry->dim != dim)
    {
        delete [] (uint32_t*) entry;
        entry = (cache_t*) new uint32_t[intsizeof(cache_t)+dim2+bits2intsize(dim2)];
    }
    entry->hashValue = hashValue;
    entry->dim = dim;
    entry->cnt = cnt;
    base_copyBest(entry->data, dbm, dim2);
    base_copySmall(&entry->data[dim2], bitMatrix, bits2intsize(dim2));
    cache.set(entry);

    assert(base_countBitsN(&entry->data[dim2], bits2intsize(dim2)) == entry->cnt);
}

#endif
