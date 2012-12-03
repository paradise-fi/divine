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
// Filename : FileStreamBuffer.h (base)
//
// Wrap (C) FILE* to std::streambuf (very simple wrap).
//
// This file is a part of the UPPAAL toolkit.
// Copyright (c) 1995 - 2003, Uppsala University and Aalborg University.
// All right reserved.
//
// $Id: FileStreamBuffer.h,v 1.1 2004/06/14 07:36:54 adavid Exp $
//
///////////////////////////////////////////////////////////////////

#ifndef INCLUDE_IO_FILESTREAMBUFFER_H
#define INCLUDE_IO_FILESTREAMBUFFER_H

#include <stdio.h>
#include <iostream>

namespace io
{
    /** Simple wrapper of FILE* to streambuf to use C-style API.
     * This class does not open or close the file, it only uses it.
     */
    class FileStreamBuffer : public std::streambuf
    {
    public:

        /** Constructor:
         * @param theFile: file stream (in C) to wrapp.
         * @pre theFile != NULL
         */
        FileStreamBuffer(FILE *theFile)
            : wrappedFile(theFile) {}
        
        /** Destructor: flush.
         */
        virtual ~FileStreamBuffer()
        {
            fflush(wrappedFile);
        }
        
        /** overrides default overflow.
         */
        virtual int overflow(int c)
        {
            return fputc(c, wrappedFile);
        }

    private:
        FILE *wrappedFile;
    };
}


/** A macro to wrap print calls based on FILE* to
 * calls using ostream.
 * @param F: file* to wrap
 * @param OBJ: object to print
 */
#define PRINT_OSTREAM(F,OBJ)  \
FileStreamBuffer local_fsb(F);\
std::ostream(&local_fsb) << OBJ


#endif // INCLUDE_IO_FILESTREAMBUFFER_H
