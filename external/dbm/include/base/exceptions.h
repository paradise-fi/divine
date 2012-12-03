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

#ifndef INCLUDE_BASE_EXCEPTIONS_H
#define INCLUDE_BASE_EXCEPTIONS_H

#include <exception>

// Common base class.

class UppaalException : public std::exception
{
protected:
    char _what[256];
public:
    virtual const char *what() const throw ();
};

// Specialized exceptions. Unfortunately
// it is difficult to factorize the code
// in the constructor due to the varying
// number of arguments.

class SystemException : public UppaalException
{
public:
    SystemException(const char *fmt, ...);
};

class InvalidOptionsException : public UppaalException
{
public:
    InvalidOptionsException(const char *fmt, ...);
};

class RuntimeException : public UppaalException
{
public:
    RuntimeException(const char *fmt, ...);
};

class SuccessorException : public RuntimeException
{
public:
    const char* state;
    const char* channel;
    SuccessorException(const char* state, const char* channel,
                       const char* message);
    virtual ~SuccessorException() throw();
};

#endif // INCLUDE_BASE_EXCEPTIONS_H
