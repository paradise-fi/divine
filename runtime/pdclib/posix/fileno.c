/* $Id$ */

/* fdopen( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST
#include <_PDCLIB_io.h>
#include <_PDCLIB_glue.h>

int fileno( FILE *f )
{
    return f->handle.sval;
}

#endif
