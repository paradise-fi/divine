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
 * Filename : malloc.h (debug)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: malloc.h,v 1.3 2005/07/22 12:55:54 adavid Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_DEBUG_MALLOC_H
#define INCLUDE_DEBUG_MALLOC_H

#include "debug/monitor.h"

#ifdef ENABLE_MONITOR

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * Replace malloc and free by calls to monitored malloc and free
 * to check for leaks. The check is automatic because we use g++
 * linker with some internal C++ objects that are automatically
 * initialized and destroyed.
 * To monitor malloc and free, just add #include "debug/malloc.h"
 * to the files where malloc and free are used.
 */

#define malloc(SIZE) debug_monitoredMalloc(SIZE, __FILE__, __LINE__, __FUNCTION__)
#define free(PTR) debug_monitoredFree(PTR, __FILE__, __LINE__, __FUNCTION__)

/** Monitored malloc.
 * @return allocated memory, as malloc would do
 * @param size: size to allocate, like malloc
 * @param filename: filename where malloc is called
 * @param line: line where malloc is called
 * @param function: function in which malloc is called
 */
void *debug_monitoredMalloc(size_t size, const char *filename, int line, const char *function);

/** Monitored free.
 * @param ptr: memory to free
 * @param filename: filename where free is called
 * @param line: line where free is called
 * @param function: function in which free is called
 */
void debug_monitoredFree(void *ptr, const char *filename, int line, const char *function);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_MONITOR */

#endif /* INCLUDE_DEBUG_MALLOC_H */
