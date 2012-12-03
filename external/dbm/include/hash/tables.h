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
// Filename : tables.h
//
// AbstractTable  : abstract template
//  |
//  +-TableSingle : template for single linked bucket hash table
//  |
//  +-TableDouble : template for double linked bucket hash table
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: tables.h,v 1.11 2005/04/25 16:38:27 behrmann Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_TABLES_H
#define INCLUDE_BASE_TABLES_H

#include "base/intutils.h"
#include "base/Enumerator.h"

/**
 * @file
 * Basis for hash tables. To get a working hash table one has
 * to implement the insert/delete that is dependant on the data
 * type, in particular to test for equality.
 */

namespace hash
{
    /** Single linked buckets
     * info contains the hash value and possible other data:
     * (info & mask) free for data, (info & ~mask) reserved.
     * We need a concrete type for the generic rehash functions
     * that do not depend on any customized data, and a template
     * type to avoid tons of casts.
     */
    struct SingleBucket_t : /* concrete type */
        public base::SingleLinkable<SingleBucket_t>
    {
        uint32_t info;
    };

    template<class BucketType>
    struct SingleBucket :   /* template */
        public base::SingleLinkable<BucketType>
    {
        uint32_t info;
    };


    /** Double linked buckets, as for single buckets.
     */
    struct DoubleBucket_t : /* concrete type */
        public base::DoubleLinkable<DoubleBucket_t>
    {
        uint32_t info;
    };

    template<class BucketType>
    struct DoubleBucket :   /* template */
        public base::DoubleLinkable<BucketType>
    {
        uint32_t info;
        /**< Default use of info lower bits:
         * reference counter.
         */

        /** Increment reference counter
         * with a given mask being the maximal
         * possible value.
         * @param mask: mask (also max)
         * @pre mask = ((1 << n) - 1) for some n
         * such that size of hash table <= 2^n.
         */
        void incRef(uint32_t mask)
        {
            if ((info & mask) != mask) info++;
        }

        /** Decrement reference counter.
         * May skip decrement if counter == mask.
         * @return true if counter is == 0
         * @param mask: mask (also max)
         */
        bool decRef(uint32_t mask)
        {
            if ((info & mask) == mask) return false;
            assert(info & mask); // must be referenced
            return (--info & mask) == 0;
        }
    };


    /** Rehashing for single/double linked
     * buckets. In practice, the hash value is
     * not recomputed since it is stored in the
     * 'info' field of the buckets.
     * @param tablePtr: where the table of bucket* to
     * rehash is. The table will be reallocated.
     * @param maskPtr: the mask used to access the indices
     * it will be read and written.
     * @pre the size of the table is a power of
     * 2 and size = mask + 1
     * @post *tablePtr is deallocated (delete) and
     * the new table is newly allocated (new).
     */
    void rehash(SingleBucket_t*** tablePtr, uint32_t *maskPtr);
    void rehash(DoubleBucket_t*** tablePtr, uint32_t *maskPtr);


    /** Abstract general hash table. DO NOT USE DIRECTLY.
     * @param BucketType: customized buckets (with customized
     * data) to use.
     * @param BucketParentType: SingleBucket_t or DoubleBucket_t
     * @param BucketParentTemplate: SingleBucket or DoubleBucket
     */
    template<class BucketType,
             class BucketParentType,
             class BucketParentTemplate>
    class AbstractTable
    {
    public:

        /** Control rehashing: by default
         * hash tables rehash themselves
         * automatically. You can disable this
         * feature.
         */
        void disableRehash() { mayRehash = false; }
        void enableRehash()  { mayRehash = mask < MAX_TABLE_SIZE-1; }

        /** Return the hash mask to get access
         * to the table: index = hashValue & mask.
         * @return a binary mask.
         */
        uint32_t getHashMask()  const { return mask; }


        /** @return the number of used buckets
         * so far.
         */
        size_t getNbBuckets() const { return nbBuckets; }
        

        /** @return the size of the table. As the size
         * is a power of 2, mask = size - 1. We keep
         * mask because it is more useful.
         */
        size_t getTableSize() const { return mask + 1; }

        
        /** Reset the table without deleting the buckets
         */
        void reset()
        {
            nbBuckets = 0;
            base_resetLarge(buckets, getTableSize()*(sizeof(void*)/sizeof(int)));
        }


        /** Reset the table and delete the buckets.
         * Note: as the bucket type is custom, the delete call
         * may call destructors of the sub-types.
         */
        void resetDelete()
        {
            size_t n = getTableSize(); // mask + 1 > 0
            BucketType **table = getBuckets();
            do {
                if (*table) {
                    BucketType *bucket = *table;
                    do {
                        BucketType *next = bucket->getNext();
                        delete bucket;
                        bucket = next;
                    } while(bucket);
                    *table = NULL;
                }
                ++table;
            } while(--n);
            nbBuckets = 0;  
        }


        /** Simple adapter to tie together the hash table and
         * bucket types.
         */
        struct Bucket_t : public BucketParentTemplate
        {};

        
        /** Enumerator
         */
        base::Enumerator<BucketType> getEnumerator() const
        {
            return base::Enumerator<BucketType>(getTableSize(), getBuckets());
        }


        /** Increase the counter of the buckets. This
         * automatically calls rehash if needed. Call
         * this whenever you add a new bucket in the
         * hash table, BUT ALWAYS AFTER adding a bucket
         * (bucket->link(somewhere) because of the rehashing.
         */
        void incBuckets()
        {
            ++nbBuckets;
            if (needsRehash() && mayRehash)
            {
                rehash(reinterpret_cast<BucketParentType***>(&buckets), &mask);
                mayRehash = mask < MAX_TABLE_SIZE-1;
            }
        }
        
        
        /** Decrease the counter of the buckets.
         * Call this whenever you remove a bucket from
         * the hash table.
         */
        void decBuckets()
        {
            assert(nbBuckets);
            --nbBuckets;
        }
        
        
        /** Access to a particular table
         * entry with a hash value.
         * @param hashValue: the hash to compute the index.
         */
        BucketType** getAtBucket(uint32_t hashValue) const
        {
            return &buckets[hashValue & mask];
        }
        
        
        /** Access to the first bucket in the
         * table with a given hash value.
         * @param hashValue: the hash to compute the index.
         */
        BucketType* getBucket(uint32_t hashValue) const
        {
            return buckets[hashValue & mask];
        }


        /** Swap this table with another.
         */
        void swap(AbstractTable<BucketType,
                  BucketParentType,BucketParentTemplate>& arg)
        {
            size_t n = arg.nbBuckets;
            arg.nbBuckets = nbBuckets;
            nbBuckets = n;
            uint32_t m = arg.mask;
            arg.mask = mask;
            mask = m;
            uint32_t s = arg.shiftThreshold;
            arg.shiftThreshold = shiftThreshold;
            shiftThreshold = s;
            bool r = arg.mayRehash;
            arg.mayRehash = mayRehash;
            mayRehash = r;
            BucketType **b = arg.buckets;
            arg.buckets = buckets;
            buckets = b;
        }

    protected:

        /** Access to the bucket table.
         */
        BucketType** getBuckets() const
        {
            return buckets;
        }


        /** Protected constructor: this is an abstract
         * class only.
         * @param sizePower2: size in power of 2, ie, real
         * size will be 2**sizePower2.
         * @param aggressive: aggressive rehashing or not,
         * which decides the threshold for rehashing.
         */
        AbstractTable(uint32_t sizePower2, bool aggressive)
            : nbBuckets(0),
              mask((1 << sizePower2) - 1),
              shiftThreshold(aggressive ? 1 : 0),
              mayRehash(true)
        {
            buckets = new BucketType*[getTableSize()];
            base_resetLarge(buckets, getTableSize()*(sizeof(void*)/sizeof(int)));

#ifndef NDEBUG
            for (size_t i = 0; i < getTableSize(); ++i) {
                assert(buckets[i] == 0);
            }
#endif /* not NDEBUG */
        }

        
        /** Destructor: not virtual. There is no polymorphism
         * involved here.
         */
        ~AbstractTable() { delete [] buckets; }


        /** We give a limit to the size of the tables
         * to stop rehashing after a certain point.
         */
        enum { MAX_TABLE_SIZE = (1 << 26) };

        size_t nbBuckets;   /**< number of buckets */

    private:

        /** @return true if the threshold for rehashing is reached
         */
        bool needsRehash() const
        {
            return nbBuckets > (mask >> shiftThreshold);
        }
        
        
        uint32_t mask;           /**< mask to apply to hash value
                                    to get indices = size - 1 since
                                    the size of the table is a power of 2     */
        uint32_t shiftThreshold; /**< mask >> shiftThreshold is the threshold */
        bool mayRehash;          /**< used to disable rehashing               */
        BucketType **buckets;    /**< the table of buckets                    */
    };


    /**************************************************************
     * Adapters to implement easily hash tables using single or
     * double linked buckets.
     *
     * - How to use:
     *
     * struct MyBucket_t
     *       : public TableSingle<MyBucket_t>::Bucket_t { ... };
     * class MyHashTable
     *       : public TableSingle<MyBucket_t> { ... };
     *
     * - Better way to make it more readable:
     *
     * struct MyBucket_t;
     * typedef TableSingle<MyBucket_t> ParentTable;
     *
     * struct MyBucket_t : public ParentTable::Bucket_t { ... };
     * class MyHashtable : public ParentTable { ... };
     *
     ***************************************************************/

    template<class BucketType>
    class TableSingle :
        public AbstractTable<BucketType,
                             SingleBucket_t,
                             SingleBucket<BucketType> >
    {
    public:
        TableSingle(uint32_t sizePower2 = 8, bool aggressive = false)
            : AbstractTable<BucketType,
                            SingleBucket_t,
                            SingleBucket<BucketType> >
                           (sizePower2, aggressive)
            {}


        /** Remove a bucket from the hash table. This
         * is useful for singly linked buckets only
         * where the info field stores the hash value
         * ON ALL ITS BITS.
         * @param bucket: bucket to remove from the hash table.
         * @pre
         * - bucket->info = hash value
         * - bucket is stored in this hash table (otherwise segfault)
         * @post bucket is removed but not deallocated
         */
        void remove(BucketType *bucket)
        {
            remove(bucket, bucket->info);
        }

        /** Real implementation of remove, using explicitely the
         * hash value that was used to entering this bucket in the
         * table. THIS IS VITAL.
         * @param bucket: bucket belonging to this table to remove
         * @param hashValue: hash that was used to enter this table
         */
        void remove(BucketType *bucket, uint32_t hashValue)
        {
            BucketType **entry = 
                AbstractTable<BucketType,
                SingleBucket_t,
                SingleBucket<BucketType> >::getAtBucket(hashValue);
            while(*entry != bucket)
            {
                assert(*entry); // MUST find it
                entry = (*entry)->getAtNext();
            }
            *entry = bucket->getNext(); // unlink
            AbstractTable<BucketType,
                             SingleBucket_t,
                             SingleBucket<BucketType> >::decBuckets();
        }

    };

    template<class BucketType>
    class TableDouble :
        public AbstractTable<BucketType,
                             DoubleBucket_t,
                             DoubleBucket<BucketType> >
    {
    public:
        TableDouble(uint32_t sizePower2 = 8, bool aggressive = false)
            : AbstractTable<BucketType,
                            DoubleBucket_t,
                            DoubleBucket<BucketType> >
                           (sizePower2, aggressive)
            {}

        /** Remove a bucket from this hash table.
         * @param bucket: bucket in this hash table to remove
         */
        void remove(BucketType *bucket)
        {
            assert(bucket);
            bucket->unlink();
            AbstractTable<BucketType,
                DoubleBucket_t,
                DoubleBucket<BucketType> >::decBuckets();
        }
    };

} // namespace hash

#endif // INCLUDE_BASE_TABLES_H

