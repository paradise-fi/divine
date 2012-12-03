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
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2006, Uppsala University and Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////

#include <cstdarg>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "base/exceptions.h"

const char* UppaalException::what() const throw ()
{
    return _what;
}

SystemException::SystemException(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_what, 256, fmt, ap);
    va_end(ap);
}

InvalidOptionsException::InvalidOptionsException(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_what, 256, fmt, ap);
    va_end(ap);
}

RuntimeException::RuntimeException(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(_what, 256, fmt, ap);
    va_end(ap);
}

SuccessorException::SuccessorException(const char* s, const char* c,
                                       const char* message):
    RuntimeException(message), state(strdup(s)), channel(strdup(c))
{
}

SuccessorException::~SuccessorException() throw()
{
    free((void*)state); free((void*)channel);
}
