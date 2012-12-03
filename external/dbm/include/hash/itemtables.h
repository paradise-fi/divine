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
// Filename : itemtables.h (hash)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: itemtables.h,v 1.3 2005/04/21 13:39:04 behrmann Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_HASH_ITEMTABLES_H
#define INCLUDE_HASH_ITEMTABLES_H

#include "hash/tables.h"

/**
 * @file
 * Hash tables for items of known sizes, right now only ItemTableSingle.
 */
    
namespace hash
{
    /** Bucket for a specific item type. Singly linked.
     */
    template<class ItemType>
    struct ItemBucketSingle :
        public TableSingle<ItemBucketSingle<ItemType> >::Bucket_t
    {
        ItemType item;
    };


    /** Hash table for singly linked buckets with items
     * of fixed size. info is used to store the hash value.
     */
    template<class ItemType>
    class ItemTableSingle :
        public TableSingle<ItemBucketSingle<ItemType> >
    {
    public:

        /** @see TableSingle
         */
        ItemTableSingle(uint32_t sizePower2, bool aggressive)
            : TableSingle<ItemBucketSingle<ItemType> >
              (sizePower2, aggressive)
            {}


        /** @return true if the hash table has a given
         * item. ItemType needs the == operator.
         * @param item: item to test
         * @param hashValue: hash value for the item.
         */
        bool hasItem(const ItemType& item, uint32_t hashValue)
        {
            ItemBucketSingle<ItemType> **entry = TableSingle<ItemBucketSingle<ItemType> >::getAtBucket(hashValue);
            ItemBucketSingle<ItemType> *bucket = *entry;
            while(bucket)
            {
                if (bucket->info == hashValue &&
                    bucket->item == item)
                {
                    return true;
                }
                bucket = bucket->getNext();
            }
            return false;
        }
        

        /** Add an item to the hash table.
         * @return added bucket or existing bucket that
         * contains this item. ItemType needs the ==
         * and the = operators.
         * @param item: item to add to the hash table.
         * @param hashValue: hash associated to the item.
         * @param alloc: an allocator having a allocate(uint)
         * to allocate in int units.
         */
        template<class Alloc>
        ItemBucketSingle<ItemType>* addItem(const ItemType& item, uint32_t hashValue,
                                            Alloc *alloc)
        {
            ItemBucketSingle<ItemType> **entry = TableSingle<ItemBucketSingle<ItemType> >::getAtBucket(hashValue);
            ItemBucketSingle<ItemType> *bucket = *entry;
            while(bucket)
            {
                if (bucket->info == hashValue &&
                    bucket->item == item)
                {
                    return bucket;
                }
                bucket = bucket->getNext();
            }
            bucket = (ItemBucketSingle<ItemType>*)
                alloc->allocate(intsizeof(ItemBucketSingle<ItemType>));
            bucket->link(entry);
            bucket->info = hashValue;
            bucket->item = item;
            TableSingle<ItemBucketSingle<ItemType> >::incBuckets();
            return bucket;
        }


        /** Add an item to the hash table.
         * @return added bucket or existing bucket that
         * contains this item. ItemType needs the ==
         * and the = operators.
         * @param item: item to add to the hash table.
         * @param hashValue: hash associated to the item.
         * NOTE: bucket is allocated with new so you need
         * to call resetDelete() to deallocate all properly.
         */
        ItemBucketSingle<ItemType>* addNewItem(const ItemType& item, uint32_t hashValue)
        {
            ItemBucketSingle<ItemType> **entry = TableSingle<ItemBucketSingle<ItemType> >::getAtBucket(hashValue);
            ItemBucketSingle<ItemType> *bucket = *entry;
            while(bucket)
            {
                if (bucket->info == hashValue &&
                    bucket->item == item)
                {
                    return bucket;
                }
                bucket = bucket->getNext();
            }
            bucket = new ItemBucketSingle<ItemType>;
            bucket->link(entry);
            bucket->info = hashValue;
            bucket->item = item;
            TableSingle<ItemBucketSingle<ItemType> >::incBuckets();
            return bucket;
        }

    };

} // namespace hash

#endif // INCLUDE_HASH_ITEMTABLES_H
