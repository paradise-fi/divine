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
// Filename : new.h
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: new.h,v 1.5 2005/07/22 12:55:54 adavid Exp $
//
///////////////////////////////////////////////////////////////////

/**
 * @file
 * Monitor all new/delete operator calls. ALL or NONE of them must be
 * monitored.
 * Limitations for more information in new:
 * 1) no placement constructor
 * 2) the new with no exception will fail too.
 *
 * Limitation for the verbose delete:
 * -  delete MUST be enclosed within brackets, statements like
 *    if (test) delete me; WILL FAIL
 * => if (test) { delete me; } is correct
 * Check this with a grep command.
 *
 * Define:
 * - ENABLE_MONITOR to enable the monitor
 * - NNEW_INFO to skip incompatible new replacements -- local to .h or .cpp
 * - NDELETE_INFO to skip incompatible delete replacements -- local to .h or .cpp
 *
 * NOTE: defining NDEBUG will *not* skip this, which allows for
 * optimized compilation with new monitor.
 */

#ifndef INCLUDE_DEBUG_NEW_H
#define INCLUDE_DEBUG_NEW_H

#include "debug/monitor.h"

#if !defined(_WIN32) && defined(ENABLE_MONITOR)

#include <new>

#ifndef NNEW_INFO

void *operator new(size_t size,const char*,int,const char*) throw (std::bad_alloc);
void *operator new[](size_t size,const char*,int,const char*) throw (std::bad_alloc);

#define new new(__FILE__,__LINE__,__FUNCTION__)

// Don't need to overload these: they fail anyway!
// void *operator new(size_t size, const std::nothrow_t&) throw();
// void *operator new[](size_t size, const std::nothrow_t&) throw();

#endif // NNEW_INFO

#ifndef NDELETE_INFO
#define delete debug_prepareDelete(__FILE__,__LINE__,__FUNCTION__); delete
#endif

#endif // ndef(_WIN32) && def(ENABLE_MONITOR)

#endif // INCLUDE_DEBUG_NEW_H
