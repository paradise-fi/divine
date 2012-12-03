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
// Filename : PriorityQueue.h (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: PriorityQueue.h,v 1.4 2005/04/22 15:20:10 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_PRIORITYQUEUE_H
#define INCLUDE_BASE_PRIORITYQUEUE_H

#include <assert.h>
#include "base/intutils.h"

namespace base
{
    /** This class is here only to protect a variable */
    class BasePriorityQueue
    {
    protected:
        /** Update the maximum.
         * @param max: maximum.
         */
        static void updateQueueMax(uint32_t max)
        {
            if (queueMaxSize < max) queueMaxSize = max;
        }

        /** @return the maximum.
         */
        static uint32_t getQueueMax() { return queueMaxSize; }

    private:
        /** This is used to dynamically adjust
         * the initial allocated table of elements.
         */
        static uint32_t queueMaxSize;
    };


    /** Fast, leak free, and controlled priority queue.
     * std::priority_queue is 20-30% slower.
     * The difference with std::priority_queue is the ordering:
     * here it is from lowest to highest, while in std::priority_queue
     * it is from highest to lowest.
     *
     * The element must be sortable and
     * have a < comparison operator.
     * Elements are copied and never destroyed.
     * The algorithm is based on heaps.
     * The the Introduction to Algorithms
     * 2nd edition p130 for max-heapify and
     * p140 for insertion. We use min instead
     * of max, so invert the comparisons.
     */
    template<class T>
    class PriorityQueue : protected BasePriorityQueue
    {
    public:

        /** If we want to have one allocation to fit
         * on a page (4k), it is advisable to allocate
         * slightly less than 4k for headers.
         */
        enum { DEC_ALLOC = 16 };

        /** Constructor: initialize the heap (size 0).
         */
        PriorityQueue()
            : capacity(getQueueMax()-DEC_ALLOC),
              qsize(0),
              table(alloc(capacity))
        {
            assert(getQueueMax() > DEC_ALLOC);
        }

        /** Destructor: update initial size & deallocate
         * memory without calling any destructor.
         */
        ~PriorityQueue()
        {
            updateQueueMax(capacity+DEC_ALLOC);
            delete [] (char*) table;
        }
        
        /** Push an item to the queue.
         * Implements min-heap insert (p140).
         * The original algorithm uses a swap, but
         * it is to propagate the new item.
         * @param item: item to push.
         * @pre item has a transitive < operator
         */
        void push(const T& item)
        {
            ensureCapacity();

            uint32_t k, i = qsize++;
            T* t = table;
            while(i && item < t[k = ((i-1) >> 1)])
            {
                t[i] = t[k];
                i = k;
            }
            t[i] = item;
        }

        /** Read the top element of the queue.
         * It is the minimal one with respect to <
         */
        const T& top() const
        {
            return table[0];
        }

        /** Remove the top element from the queue.
         * Implement min-heapify (see max-heapify
         * p130: Left(i) == l and Right(i) == l+1).
         * @pre !empty()
         */
        void pop()
        {
            assert(qsize);
            if (--qsize)
            {
                uint32_t i = 0;
                T* t = table;
                T* last = &t[qsize];
                if (qsize > 1)
                {
                    uint32_t l;
                    while(&t[l = (i << 1) + 1] < last)
                    {
                        uint32_t k =
                            &t[l+1] < last && t[l+1] < t[l] ?
                            l+1 : l;
                        if (t[k] < *last)
                        {
                            t[i] = t[k];
                            i = k;
                        }
                        else break;
                    }
                }
                t[i] = *last;
            }
        }

        /** @return true if the queue is empty,
         * false otherwise.
         */
        bool empty() const { return qsize == 0; }

        /** @return the size of the queue.
         */
        uint32_t size() const { return qsize; }

    private:

        /** @return allocated memory for T[n]
         * without calling any constructor.
         * @param n: number of elements
         * @pre T is int padded
         */
        T* alloc(uint32_t n)
        {
            assert(sizeof(T)%4 == 0); // int padded
            return (T*) new char[sizeof(T[n])];
        }

        /** Ensure capacity for a new element to
         * be added on the heap.
         */
        void ensureCapacity()
        {
            assert(qsize <= capacity);
            if (qsize == capacity)
            {
                capacity = (capacity << 1) + DEC_ALLOC; // == (capa+dec_alloc)<<1 - dec_alloc
                T *larger = alloc(capacity);
                base_copyLarge(larger, table, intsizeof(T[qsize]));
                delete [] table;
                table = larger;
            }
        }

        uint32_t capacity; /**< capacity of the heap                 */
        uint32_t qsize;    /**< size of the queue                    */
        T *table;          /**< table of items with a heap structure */
    };

    /** Wrapper for large types: wrap a pointer.
     * The large type needs a < comparison operator.
     */
    template<class T>
    class Sortable
    {
    public:
        Sortable(T *p) : ptr(p) {}

        bool operator < (const Sortable<T>& x) const
        {
            assert(ptr && x.ptr);
            return *ptr < *x.ptr;
        }

        T *ptr;
    };

} // namespace base

#endif // INCLUDE_BASE_PRIORITYQUEUE_H
