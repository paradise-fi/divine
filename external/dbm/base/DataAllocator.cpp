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
// Filename : DataAllocator.cpp
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: DataAllocator.cpp,v 1.16 2005/05/14 17:18:20 behrmann Exp $
//
///////////////////////////////////////////////////////////////////

#include "base/DataAllocator.h"
#include "io/FileStreamBuffer.h"
#include "debug/macros.h"
#include "debug/new.h"

// When debugging we allocate more to
// save the allocated size to check it.
// This results in an offset of the allocated
// memory.
#ifndef NDEBUG
#define DEBUG_OFFSET 1
#else
#define DEBUG_OFFSET 0
#endif

#ifdef ENABLE_64BIT
#define SIZE(I) ((I+1)/2)
#else
#define SIZE(I) (I)
#endif

#define CHECK_ALIGNED32(PTR) \
        ASSERT((((uintptr_t)(PTR)) & 3) == 0,\
               std::cerr << "Allocated memory is not 32 bits aligned.\n")


namespace base
{
    /** constructor: allocate a pool
     */
    DataAllocator::DataAllocator()
        : freeMem(145) // Good Value(tm)
    {
        memPool = new Pool_t;
        memPool->next = NULL;
        freePtr = memPool->mem;
        endFree = memPool->end;

        assert(sizeof(uintptr_t) == sizeof(uint32_t*));
#ifdef ENABLE_64BIT
        assert(sizeof(uintptr_t) == 2*sizeof(uint32_t));
#else
        assert(sizeof(uintptr_t) == sizeof(uint32_t));
#endif
        CHECK_ALIGNED32(memPool);
    }
    
    /** destructor: deallocate all pools
     */
    DataAllocator::~DataAllocator()
    {
        assert(memPool);
        
        Pool_t *pool = memPool;
        do {
            Pool_t *next = pool->next;
            delete pool;
            pool = next;
        } while(pool);
    }
    
    /** allocate memory:
     * take memory
     * - on the free list first
     * - or on the pool if there is space left
     * - or allocate a new pool
     */
    void* DataAllocator::allocate(size_t intSize)
    {
        ASSERT(intSize >=0, std::cerr << "Cannot allocate negative size!\n");
        ASSERT(intSize <= CHUNK_SIZE,
               std::cerr << "DataAllocator cannot allocate "
                         << (intSize << 2) << " bytes!\n");
        
        // nothing to do
        if (intSize == 0)
        {
            return NULL;
        }
        
        intSize = SIZE(intSize);

        // debugging: allocate more to save intSize
        intSize += DEBUG_OFFSET;
        
        // try from free memory list
        uintptr_t *data = freeMem.get(intSize);
        if (data)
        {
            freeMem[intSize] = getNext(*data); // next free block of size intSize
            DODEBUG(*data = intSize);          // store (argument) size
            return data + DEBUG_OFFSET;        // skip debug field
        }
        
        // allocate and return if within bounds
        data = freePtr;
        freePtr += intSize;

        if (freePtr <= endFree)
        {
            DODEBUG(*data = intSize);
            return data + DEBUG_OFFSET;
        }
        
        // store memory left from the pool
        uint32_t memLeft = endFree - data;
        if (memLeft)
        {
            *data = getNext(freeMem.replace(SIZE(memLeft), data));
        }
        
        // new pool
        Pool_t *newPool = new Pool_t;
        CHECK_ALIGNED32(newPool);

        newPool->next = memPool;
        memPool = newPool;
        data = newPool->mem;
        freePtr = data + intSize;
        endFree = newPool->end;
        
        DODEBUG(*data = intSize);
        return data + DEBUG_OFFSET;
    }
    
    /** Deallocate: store in the free list.
     */
    void DataAllocator::deallocate(void *ptr, size_t intSize)
    {
        if (intSize)
        {
            uintptr_t *data = ((uintptr_t*)ptr) - DEBUG_OFFSET;
            intSize = SIZE(intSize);
            intSize += DEBUG_OFFSET;

            assert(*data == intSize); // check correct size + no corruption
            assert(hasInPools(data, intSize));

            // Trivial merge: if the deallocated memory is at the
            // end, then move back freePtr instead of putting the
            // memory in the free list.

            if (data + intSize == freePtr)
            {
                freePtr = data;
            }
            else
            {
                *data = getNext(freeMem.replace(intSize, data));
            }

            assert(!hasInPools(data, intSize));
        }
    }
    
    /** Deallocate all pools except the base.
     */
    void DataAllocator::reset()
    {
        assert(memPool);
        
        Pool_t *pool = memPool->next;
        memPool->next = NULL;
        freePtr = memPool->mem;
        endFree = memPool->end;
        
        while(pool)
        {
            Pool_t *next = pool->next;
            delete pool;
            pool = next;
        }
        
        // reset the free list too
        freeMem.reset();
    }

    /** Print statistics, C style = wrapper to C++.
     */
    void DataAllocator::printStats(FILE *out) const
    {
        io::FileStreamBuffer fsb(out);
        std::ostream os(&fsb);
        printStats(os);
    }

    /** Utility function to print stats
     */
    static
    void DataAllocator_printMem(std::ostream &out, const char *caption, uint32_t mem)
    {
        out << caption << ": " << mem << 'B';
        if (mem > 1024)
        {            
            debug_cppPrintMemory(out << "\t= ", mem);
        }
        out << "\n";
    }

    /** Print statistics.
     */
    std::ostream& DataAllocator::printStats(std::ostream &out) const
    {
        // compute pool stats
        uint32_t nbPools = 0;
        const Pool_t *pool = memPool;
        while(pool)
        {
            nbPools++;
            pool = pool->next;
        }

        // compute free list stats
        uint32_t memInFreeList = 0;
        array_t<uint32_t> freeListStats;
        uint32_t i, n = freeMem.size();
        for(i = 0; i < n; ++i)
        {
            uintptr_t *freeList = freeMem[i];
            while(freeList)
            {
                // size i at index i
                freeListStats.add(i, i);
                memInFreeList += i;
                freeList = getNext(*freeList);
            }
        }

        // Stats are wrong in 64-bit.

        DataAllocator_printMem(out << "DataAllocator stats: " << nbPools << " pools allocated\n",
                               "Total memory             ",
                               nbPools*sizeof(uint32_t[CHUNK_SIZE]));
        DataAllocator_printMem(out,
                               "Memory allocated         ",
                               nbPools*sizeof(uint32_t[CHUNK_SIZE]) -
                               (memInFreeList + endFree - freePtr)*sizeof(uint32_t));
        DataAllocator_printMem(out,
                               "Available in current pool",
                               (endFree-freePtr)*sizeof(uint32_t));
        DataAllocator_printMem(out,
                               "Deallocated available    ",
                               memInFreeList*sizeof(uint32_t));
        out << "Details of deallocated memory:";
        n = freeListStats.size();
        for(i = 0; i < n; ++i)
        {
            if (freeListStats[i])
            {
                debug_cppPrintMemory(out << " [" << i << "]=",
                                     freeListStats[i]*sizeof(uint32_t));
            }
        }
        return out << "\n";
    }


    /* Check that data belongs to a pool or a free list.
     */
    bool DataAllocator::hasInPools(const uintptr_t *data, size_t intSize) const
    {
        bool inPool = false;
        assert(memPool);

        // current pool: check against freePtr
        if (data >= memPool->mem && data < freePtr)
        {
            inPool = true;
        }
        
        // list of full pools
        for(const Pool_t *pool = memPool->next; !inPool && pool != NULL; pool = pool->next)
        {
            if (data >= pool->mem && data < pool->end)
            {
                inPool = true;
            }
        }
        
        // not allocated in the pools
        if (!inPool) return false;
        
        // may be allocated in the pools but on the free list => not in the pools
        for(uintptr_t *fdata = freeMem.get(intSize); fdata != NULL; fdata = getNext(*fdata))
        {
            if (data == fdata) return false;
        }

        return true;
    }

} // namespace base


/* Wrap the call to the allocator.
 */
int32_t* base_allocate(size_t size, void *allocator)
{
    base::DataAllocator *alloc =
        (base::DataAllocator*) allocator;

    return (int32_t*) alloc->allocate(size);
}


/* Wrap the call to the allocator.
 */
void base_deallocate(void *mem, size_t intSize, void *allocator)
{
    base::DataAllocator *alloc =
        (base::DataAllocator*) allocator;

    alloc->deallocate(mem, intSize);
}


/* Wrap the call to new.
 */
int32_t* base_new(std::size_t size, void*)
{
    return new int32_t[size];
}


/* Wrap the call to delete.
 */
void base_delete(void *mem, size_t unused1, void *unused2)
{
    delete [] (int32_t*) mem;
}


/* Instance of allocator_t based on new
 */
allocator_t base_newallocator =
{
    allocData:NULL,
    allocFunction:base_new,
    deallocFunction:base_delete
};
