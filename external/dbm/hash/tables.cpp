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
// Filename : tables.cpp (hash)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: tables.cpp,v 1.7 2005/07/21 22:29:25 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef NDEBUG
#include <iostream>
#include "base/array_t.h"
#endif

#include "debug/macros.h"
#include "hash/tables.h"
#include "debug/new.h"

using namespace base;

namespace hash
{

// Local debugging: use -DSHOWS_STATS
// #define SHOW_STATS

#if defined(SHOW_STATS) && !defined(NDEBUG)
#define HASHDEBUG(x) x

    /** Count "true" collisions where 2 buckets in
     * the same collision list have the same info.
     * @param root: 1st bucket of the collision list.
     * @return # of cases where 2 buckets have the
     * same info.
     */
    static inline
    size_t hash_countCollisions(const SingleBucket_t *root0)
    {
        size_t count = 0;
        for(const SingleBucket_t *root = root0; root != NULL; root = root->getNext())
        {
            // Check not already counted
            bool ok = true;
            for(const SingleBucket_t *check = root0; check != root; check = check->getNext())
            {
                if (check->info == root->info)
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
            {
                // Count collisions
                const SingleBucket_t *next = root->getNext();
                while(next)
                {
                    if (root->info == next->info) count++;
                    next = next->getNext();
                }
            }
        }
        return count;
    }
    
    /** Same, for doubly linked buckets. Code
     * is the same syntactically but different
     * semantically since the struct are different.
     */
    static inline
    size_t hash_countCollisions(const DoubleBucket_t *root0)
    {
        size_t count = 0;
        for(const DoubleBucket_t *root = root0; root != NULL; root = root->getNext())
        {
            // Check not already counted
            bool ok = true;
            for(const DoubleBucket_t *check = root0; check != root; check = check->getNext())
            {
                if (check->info == root->info)
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
            {
                // Count collisions
                const DoubleBucket_t *next = root->getNext();
                while(next)
                {
                    if (root->info == next->info) count++;
                    next = next->getNext();
                }
            }
        }
        return count;
    }

#else
#define HASHDEBUG(x)
#endif

    /** Rehashing for singled linked buckets.
     * Algorithm: the buckets are typed as follows
     * { bucket_t *next; uint info; custom data.. }
     * where info & ~mask is reserved for higher bits
     * of hash values.
     * Rehashing consists in going through all the buckets
     * and moving them to right places, depending on one
     * bit that corresponds to the previous size (= power of 2).
     */
    void rehash(SingleBucket_t ***oldTablePtr, uint32_t *maskPtr)
    {
        assert(oldTablePtr && maskPtr);

        HASHDEBUG((std::cerr << CYAN(THIN) "Rehashing(single)..." NORMAL).flush());
        HASHDEBUG(array_t<size_t> lenStat);  // stats on lengths
        HASHDEBUG(size_t oldCollisions = 0); // collision stats
        HASHDEBUG(size_t newCollisions = 0); // collision stats
        HASHDEBUG(size_t oldTrueCollis = 0); // true collision stats
        HASHDEBUG(size_t nbBuckets = 0);

        size_t oldSize = *maskPtr + 1; // used as bit mask
        size_t newSize = oldSize << 1; // double size
        *maskPtr = newSize - 1;          // new mask (newSize is a power of 2)

        SingleBucket_t **oldBuckets = *oldTablePtr;
        SingleBucket_t **newBuckets = new SingleBucket_t*[newSize];
        *oldTablePtr = newBuckets; // set new table

        // reset new buckets only, those 0..oldSize-1 will be copied
        base_resetLarge(newBuckets + oldSize, oldSize*(sizeof(void*)/sizeof(int)));

        for(size_t i = 0 ; i < oldSize ; ++i)
        {
            HASHDEBUG(size_t thisLen = 0);
            SingleBucket_t *bucketi = oldBuckets[i];
            newBuckets[i] = NULL;
            HASHDEBUG(oldTrueCollis += hash_countCollisions(bucketi));
            while(bucketi) // rellocate collision list
            {
                // (bucketi->info & oldSize) == 0 or oldSize
                // depending on the hashValue.
                SingleBucket_t **root = newBuckets + i + (bucketi->info & oldSize);
                SingleBucket_t *next = bucketi->getNext();
                HASHDEBUG(if (*root) newCollisions++);
                bucketi->link(root);
                bucketi = next;
                HASHDEBUG(if (bucketi) oldCollisions++);
                HASHDEBUG(thisLen++);
                HASHDEBUG(nbBuckets++);
            }
            HASHDEBUG(lenStat.add(thisLen, 1));
        }
        // free old table
        delete [] oldBuckets;

        // stats
#if defined(SHOW_STATS) && !defined(NDEBUG)
        std::cerr << nbBuckets << '/' << oldSize
                  << " true collisions=" << oldTrueCollis
                  << "\nOld collisions(" << oldCollisions << ')';
        size_t size = lenStat.size();
        for(size_t j = 0 ; j < size ; ++j)
        {
            if (lenStat[j]) std::cerr << " [" << j << "]=" << lenStat[j];
        }
        size_t nbEntries = nbBuckets - oldCollisions;
        double average = nbEntries ? ((100*nbBuckets)/nbEntries)/100.0 : 0.0;
        std::cerr << " average=" << average;
        nbEntries = nbBuckets - newCollisions;
        average = nbEntries ? ((100*nbBuckets)/nbEntries)/100.0 : 0.0;
        std::cerr << "\nNew collision("
                  << newCollisions << ") average="
                  << average << std::endl;
#endif
    }
    
    
    /** Rehashing for double linked buckets. Same as
     * the previous rehash with the difference on the
     * type of the buckets:
     * { bucket_t *next; bucket_t **previous; uint info; custom data }
     */
    void rehash(DoubleBucket_t*** oldTablePtr, uint32_t *maskPtr)
    {
        assert(oldTablePtr && maskPtr);

        HASHDEBUG((std::cerr << CYAN(THIN) "Rehashing(double)..." NORMAL).flush());
        HASHDEBUG(array_t<size_t> lenStat);  // stats on lengths
        HASHDEBUG(size_t oldCollisions = 0); // collision stats
        HASHDEBUG(size_t newCollisions = 0); // collision stats
        HASHDEBUG(size_t oldTrueCollis = 0); // true collision stats
        HASHDEBUG(size_t nbBuckets = 0);

        size_t oldSize = *maskPtr + 1; // used as bit mask
        size_t newSize = oldSize << 1; // double size
        *maskPtr = newSize - 1;          // new mask (newSize is a power of 2)

        DoubleBucket_t **oldBuckets = *oldTablePtr;
        DoubleBucket_t **newBuckets = new DoubleBucket_t*[newSize];
        *oldTablePtr = newBuckets;

        // reset new buckets only, those 0..oldSize-1 will be copied
        base_resetLarge(newBuckets + oldSize, oldSize*(sizeof(void*)/sizeof(int)));

        for(size_t i = 0 ; i < oldSize ; ++i)
        {
            HASHDEBUG(size_t thisLen = 0);
            DoubleBucket_t *bucketi = oldBuckets[i];
            newBuckets[i] = NULL;
            HASHDEBUG(oldTrueCollis += hash_countCollisions(bucketi));
            while(bucketi) // rellocate collision list
            {
                // (bucketi->info & oldSize) == 0 or oldSize
                // depending on the hashValue.
                DoubleBucket_t **root = newBuckets + i + (bucketi->info & oldSize);
                DoubleBucket_t *next = bucketi->getNext();
                HASHDEBUG(if (*root) newCollisions++);
                bucketi->link(root);
                bucketi = next;
                HASHDEBUG(if (bucketi) oldCollisions++);
                HASHDEBUG(thisLen++);
                HASHDEBUG(nbBuckets++);
            }
            HASHDEBUG(lenStat.add(thisLen, 1));
        }
        // free old table
        delete [] oldBuckets;

        // stats
#if defined(SHOW_STATS) && !defined(NDEBUG)
        std::cerr << nbBuckets << '/' << oldSize
                  << " true collisions=" << oldTrueCollis
                  << "\nOld collisions(" << oldCollisions << ')';
        size_t size = lenStat.size();
        for(size_t j = 0 ; j < size ; ++j)
        {
            if (lenStat[j]) std::cerr << " [" << j << "]=" << lenStat[j];
        }
        size_t nbEntries = nbBuckets - oldCollisions;
        double average = nbEntries ? ((100*nbBuckets)/nbEntries)/100.0 : 0.0;
        std::cerr << " average=" << average;
        nbEntries = nbBuckets - newCollisions;
        average = nbEntries ? ((100*nbBuckets)/nbEntries)/100.0 : 0.0;
        std::cerr << "\nNew collisions("
                  << newCollisions << ") average="
                  << average << std::endl;
#endif
    }

}

