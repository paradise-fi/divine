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
 * Filename : inttypes.h (base)
 * C header.
 *
 * Definition of integer types. Basically a wrapper for different
 * includes + basic size computation in int units.
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * v 1.2 reviewed.
 * $Id: inttypes.h,v 1.11 2005/04/22 15:20:10 adavid Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_BASE_INTTYPES_H
#define INCLUDE_BASE_INTTYPES_H

/** Note on availability of headers:
 * - Linux has inttypes.h and stdint.h
 * - Solaris has inttypes.h only
 * - Mingwing has stdint.h
 * - Microsoft has none of them
 *   temporary solution: define here.
 *
 * config.h knows about availability:
 */
#include "config.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#else
#error "No inttypes.h or stdint.h"
#endif

/** Size computation in "int" units to avoid any alignment problem.
 * intsizeof    : as sizeof but result in "int" units
 * bits2intsize : convert # of bits  to # of int (size+31)/32
 * bytes2intsize: convert # of bytes to # of int (size+3)/4
 */
#define bits2intsize(X)  (((X) + 31) >> 5)
#define bytes2intsize(X) (((X) +  3) >> 2)
#define intsizeof(X)     bytes2intsize(sizeof(X))


/** Hack to get around icc failure to recognize property
 * POD types (plain old data): if TYPE is a class, then
 * TYPE* is not considered as POD.
 * Here we allocate on the stack an array of (icc-non POD)
 * TYPE* via a cast.
 */
#ifdef __INTEL_COMPILER
#define POINTERS_OF(_TYPE, _NAME, _SIZE) \
 uintptr_t _alloc_##_NAME[_SIZE];         \
 _TYPE **_NAME = (_TYPE**) _alloc_##_NAME
#else
#define POINTERS_OF(_TYPE, _NAME, _SIZE) \
 _TYPE *_NAME[_SIZE]
#endif

#ifndef _WIN32

/* Define ARCH_APPLE_DARWIN on Mac OS X.
 */
#if defined(__APPLE__) && defined(__MACH__)
#define ARCH_APPLE_DARWIN
#endif

/** Define BOOL for C compilers.
 * NOTE: we could define bool for C compilers only but
 * the type may not be compatible with the C++ compiler
 * (as defined in stdbool.h).
 */
#ifdef ARCH_APPLE_DARWIN
 #ifndef FALSE
  #define FALSE 0
 #endif
 #ifndef TRUE
  #define TRUE 1
 #endif
 typedef int BOOL;
#else
 typedef enum { FALSE=0, TRUE=1 } BOOL;
#endif

#else
#include <windows.h>
/* big trouble */
#if !defined(__cplusplus) && !defined(__GNUC__)
#define inline __inline
#endif
#endif

/** Type for indices (variables and clocks).
 */
typedef uint32_t cindex_t;

#endif /* INCLUDE_BASE_INTTYPES_H */
