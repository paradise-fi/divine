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
 * Filename : c_allocator.c (base)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: c_allocator.c,v 1.2 2004/06/14 07:36:52 adavid Exp $
 *
 *********************************************************************/

#include <stdlib.h>
#include "base/c_allocator.h"
#include "debug/malloc.h"

/* straight-forward (m)allocation of int[intSize]
 */
int32_t* base_malloc(size_t intSize, void *data)
{
    return (int32_t*) malloc(intSize << 2);
}

/* straight-forward deallocation
 */
void base_free(void *mem, size_t unused1, void *unused2)
{
    free(mem);
}


/* default allocator instance
 */
allocator_t base_mallocator =
{
    allocData:NULL,
    allocFunction:base_malloc,
    deallocFunction:base_free
};

