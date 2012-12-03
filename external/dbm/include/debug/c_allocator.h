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
 * Filename : c_allocator.h (debug)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: c_allocator.h,v 1.2 2004/06/14 07:36:54 adavid Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_DEBUG_C_ALLOCATOR_H
#define INCLUDE_DEBUG_C_ALLOCATOR_H

#include "base/c_allocator.h"
#include "base/intutils.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @file
 * Implementation of a particular allocator_t
 * to check for memory leaks when testing.
 */

/** Type to keep track of allocations.
 */
typedef struct debug_allocation_s
{
    struct debug_allocation_s *next;
    void *ptr;
    size_t size;
} debug_allocation_t;


/** Debug allocator function based on malloc.
 * @param intSize: size to allocate in ints
 * @param debugAllocations: a debug_allocation_t**
 * @return int32_t[intSize] allocated by malloc
 */
int32_t* debug_malloc(size_t intSize, void *debugAllocations);


/** Debug free function, to free memory allocated by debug_malloc.
 * @param ptr: memory to deallocate.
 * @param intSize: size to deallocate
 * @param debugAllocations: all the allocations.
 */
void debug_free(void *ptr, size_t intSize, void *debugAllocations);


/** Size of the allocation table.
 * This MUST be a power of 2.
 */
enum { debug_ALLOCATION_TABLE = 1 << 20 };


/** Initialize a debug_allocator_t.
 * @param alloc: allocator to initialize.
 */
static inline
void debug_initAllocator(allocator_t *alloc)
{
    alloc->allocData = malloc(debug_ALLOCATION_TABLE*sizeof(debug_allocation_t*));
    alloc->allocFunction = debug_malloc;
    alloc->deallocFunction = debug_free;
    memset(alloc->allocData, 0, debug_ALLOCATION_TABLE*sizeof(debug_allocation_t*));
}


/** Destroy a debug allocator and check it for leaks.
 * @param debugAllocator: an allocator as initialized by debug_initAllocator
 */
void debug_destroyAllocator(allocator_t *debugAllocator);


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_DEBUG_C_ALLOCATOR_H */
