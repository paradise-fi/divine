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
 * Filename : monitor.h (debug)
 *
 * This file is a part of the UPPAAL toolkit.
 * Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
 * All right reserved.
 *
 * $Id: monitor.h,v 1.2 2005/07/22 12:55:54 adavid Exp $
 *
 *********************************************************************/

#ifndef INCLUDE_DEBUG_MONITOR_H
#define INCLUDE_DEBUG_MONITOR_H

#include <stddef.h>

#ifdef ENABLE_MONITOR

#ifdef __cplusplus
extern "C" {
#endif

/** Remember a pointer and returns it.
 * @param ptr: pointer to remember
 * @param filename: in which file the call was made
 * @param line: at which line the call was made
 * @param function: in which function the call was made
 * @return ptr
 */
void* debug_rememberPointer(void *ptr, const char *filename, int line, const char *function);

/** Forget a previously remembered pointer.
 * @param ptr: pointer to forget
 * @param filename: in which file the call was made
 * @param line: at which line he call was made
 * @param function: in which function the call was made
 * @pre ptr was remembered before or an error will be printed
 */
void debug_forgetPointer(void *ptr, const char *filename, int line, const char *function);

/** Forget a previously remembered pointer
 * @param ptr: pointer to forget
 * @pre ptr was remembered before or an error will be printed
 */
void debug_forgetPtr(void *ptr);

/** Remember a position just before a call to forgetPtr.
 * The position will be gone after one call to forgetPtr.
 * @param filename: in which file the call was made
 * @param line: at which line he call was made
 * @param function: in which function the call was made
 */
void debug_prepareDelete(const char *filename, int line, const char *function);

/** Push a position for later retrieval by pop.
 * @param filename: in which file the call was made
 * @param line: at which line he call was made
 * @param function: in which function the call was made
 */
void debug_pushPosition(const char *filename, int line, const char *function);

/** Equivalent to calling prepareDelete on a previously pushed position.
 */
void debug_pop();

/* Macros to simplify calls */

#define debug_remember(TYPE, PTR) ((TYPE) debug_rememberPointer(PTR, __FILE__, __LINE__, __FUNCTION__))
#define debug_forget(PTR) debug_forgetPointer(PTR, __FILE__, __LINE__, __FUNCTION__)
#define debug_mark() debug_prepareDelete(__FILE__, __LINE__, __FUNCTION__)
#define debug_push() debug_pushPosition(__FILE__, __LINE__, __FUNCTION__)

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_MONITOR */

#endif /* INCLUDE_DEBUG_MONITOR_H */
