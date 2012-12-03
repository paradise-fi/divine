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
 * Filename : platform.h (base)
 * C header.
 *
 * Platform dependant code abstraction layer. This API gives access
 * to miscellanous platform dependent code, such as:
 * - getting time
 * - ... TODO
 *
 * NOTE: C interface and implementation
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.3 reviewed.
 * $Id: platform.h,v 1.4 2005/01/25 22:05:19 behrmann Exp $
 *
 **********************************************************************/

#ifndef INCLUDE_BASE_PLATFORM_H
#define INCLUDE_BASE_PLATFORM_H

#include <time.h>
#include "base/inttypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** strptime is not available under Windows
 * so we have an implementation of it,
 * otherwise include <time.h> to get the
 * standard (posix) version under Linux.
 */
//#ifdef __MINGW32__
char* strptime2(const char *buf, const char *fmt, struct tm *tm, time_t *now);
//#endif


/** Time access function
 * @return some time reference in second.
 * The meaning of the time reference does not matter but computing
 * the difference value of consecutive calls will give the real
 * elapsed time.
 */
double base_getTime();


/** Finer grain time measure function.
 * @return number of CPU cycles (TSC asm call).
 * This is officially implemented only starting with Intel Pentium II platforms.
 * Unreliable due to power saving (hibernation, freq scaling), multi-cpu/core.
 */
static inline
uint64_t gettsc(void)
{
#if defined(__GNUG__) && defined(i386)
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return (uint64_t) hi << 32 | lo;
#else
    return 0;
#endif
} 


/** 
 * Returns an OS-specific error description for given error code.
 * (this is a replacement for strerror which is inconsistent 
 * with errno.h on MinGW32).
 */
const char* oserror(int error_code);


/**
 * Reading memory information of the host system in kB:
 * We offer a generic function that supports a
 * common set of information under Windows & Linux
 * in a way that is easily extendable.
 */
enum {
    PHYS_TOTAL = 0, /* total amount of physical RAM */
    PHYS_AVAIL = 1, /* free physical RAM */
    PHYS_CACHE = 2, /* RAM used by OS (for I/O cache and buffers etc) */
    SWAP_TOTAL = 3, /* total swap/pagefile size */
    SWAP_AVAIL = 4, /* free swap */
    VIRT_TOTAL = 5, /* total virtual memory */
    VIRT_AVAIL = 6  /* free virtual memory */
};

typedef uint64_t meminfo_t[7];

void base_getMemInfo(meminfo_t);

/**
 * Reading information about this process in kB and millisec.
 */
enum {
    PROC_MEM_VIRT = 0, /* allocated virtual memory */
    PROC_MEM_WORK = 1, /* working set in physical RAM */
    PROC_MEM_SWAP = 2, /* swapped out */
    PROC_TIME_USR = 3, /* user time */
    PROC_TIME_SYS = 4, /* kernel/system time */
    PROC_TIME_TSP = 5  /* current timestamp of snapshot */
};

typedef uint64_t procinfo_t[6];

void base_initProcInfo();
void base_getProcInfo(procinfo_t);


/* MAC addresses. */

typedef uint8_t mac_adr_t[8];

typedef struct
{
    size_t size;
    mac_adr_t mac[];
} maclist_t;

/**
 * Get the available MAC addresses. The
 * structure is malloc allocated and should
 * be freed. Returns NULL in case of error.
 */
maclist_t* base_getMAC();


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_BASE_PLATFORM_H */
