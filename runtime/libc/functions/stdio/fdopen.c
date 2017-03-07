/* $Id$ */

/* fdopen( const char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>
#include <stdlib.h>

#ifndef REGTEST
#include <_PDCLIB_io.h>
#include <_PDCLIB_glue.h>
#include <string.h>
#include <errno.h>

extern _PDCLIB_fileops_t _PDCLIB_fileops;

FILE * fdopen( int fd,
               const char * _PDCLIB_restrict mode )
{
    _PDCLIB_fd_t handle;
    handle.sval = fd;
    int imode = _PDCLIB_filemode( mode );

    if( imode == 0 )
        return NULL;

    return _PDCLIB_fvopen( handle, &_PDCLIB_fileops, imode, NULL );
}

#endif
