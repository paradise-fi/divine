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
// Filename : DataAllocator.h (base)
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: DataAllocator.h,v 1.15 2005/04/22 15:20:10 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_DATAALLOCATOR_H
#define INCLUDE_BASE_DATAALLOCATOR_H

#include <stdio.h>
#include <iostream>
#include "base/array_t.h"
#include "base/c_allocator.h"
#include "base/Object.h"


/** C wrapper function for DataAllocator.
 * @see dbm/mingraph.h
 * @param size: size in int to allocate.
 * @param allocator: a pointer to DataAllocator
 */
int32_t* base_allocate(size_t size, void *allocator);


/** C wrapper function for DataAllocator.
 * @see base/c_allocator.h
 * @param mem: memory to deallocate
 * @param intSize: size in int to deallocate
 * @param allocator: DataAllocator object
 * @pre memory was allocated with base_allocate and intSize corresponds
 * to the allocated size.
 */
void base_deallocate(void *mem, size_t intSize, void *allocator);


/** C wrapper function for new.
 * @see dbm/mingraph.h, base/c_allocator.h
 * @param size: size in int to allocate.
 * @param unused: unused.
 */
int32_t* base_new(size_t size, void *unused);


/** C wrapper function for delete.
 * @see base/c_allocator.h
 * @param mem: memory to deallocate.
 * @param unused1, unused2: unused parameters.
 */
void base_delete(void *mem, size_t unused1, void *unused2);


/** C wrapper allocator instance for new.
 */
extern allocator_t base_newallocator;


namespace base
{
    /** Fast chunk allocator.
     * Has the ability to deallocate all data at once.
     */
    class DataAllocator : public base::Object
    {
    public:
        DataAllocator();
        virtual ~DataAllocator();
        
        /** Allocate memory.
         * @param intSize: size in int units
         * @return a int32[intSize] allocated
         * memory area.
         * If ALIGN_WORD64 is defined then the result is aligned
         * on 64 bits. It is the responsability of the developper
         * to use the flag. It is possible to skip the flag on
         * Intel architecture that can cope with non aligned
         * addresses.
         * @pre intSize <= CHUNK_SIZE
         */
        void*  allocate(size_t intSize);
        
        
        /** Deallocate memory.
         * @pre 
         * - memory was allocated with allocate
         * - size allocated was intSize
         * @param data: memory to deallocate
         * @param intSize: size in int units of the
         * allocated memory.
         */
        void deallocate(void *data, size_t intSize);
        
        
        /** Reset the allocator: deallocate all
         * memory allocated by this allocator!
         */
        void reset();

        
        /** Print statistics, C style.
         * @param out: where to print.
         */
        void printStats(FILE *out) const;


        /** Print statistics, C++ style.
         * @param out: where to print.
         * @return the ostream.
         */
        std::ostream& printStats(std::ostream &out) const;


        /** C wrapper allocator
         * @return C wrapper allocator_t
         */
        allocator_t getCAllocator()
        {
            allocator_t c_alloc =
            {
                allocData:this,
                allocFunction:base_allocate,
                deallocFunction:base_deallocate
            };
            return c_alloc;
        }

    private:
        /** Memory is allocated internally by
         * chunks. This controls the size of
         * these chunks.
         */
        enum { CHUNK_SIZE = (1 << 22) };
        
        /** Table of free memory lists.
         * freeMem[i] starts a list of free
         * memory blocks of i INT size
         */
        array_t<uintptr_t*> freeMem;

        /** An entry in the freeMem table
         * at i gives a free memory block
         * of size i ints. If there are other
         * such blocks, then this memory block
         * contains the pointer to the next
         * free block.
         * This function is a convenience cast
         * for this case.
         */
        static inline
        uintptr_t* getNext(uintptr_t data)
        {
            return (uintptr_t*) data;
        }
        static inline
        uintptr_t getNext(uintptr_t *data)
        {
            return (uintptr_t) data;
        }
        

        /** Check that a given address belongs to a pool or the free list.
         * Used for debugging, this is not a performance critical method.
         * @param data: memory to check,
         * @param intSize: size of the memory to check.
         * @return true if the memory belongs to a pool.
         */
        bool hasInPools(const uintptr_t *data, size_t intSize) const;


        /** Memory pool.
         */
        struct Pool_t
        {
            Pool_t *next;
            uintptr_t mem[CHUNK_SIZE];
            uintptr_t end[]; /**< only to mark the end, no data */
        };
        
        Pool_t *memPool;    /**< current pool in use       */
        uintptr_t *freePtr; /**< current free mem position */
        uintptr_t *endFree; /**< end of current chunk      */
    };

} // namespace base


static inline
std::ostream& operator << (std::ostream& os, const base::DataAllocator& alloc)
{
    return alloc.printStats(os);
}

#endif // INCLUDE_ALLOCATOR_DATAALLOCATOR_H
