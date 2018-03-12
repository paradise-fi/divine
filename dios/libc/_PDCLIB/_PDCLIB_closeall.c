/* _PDCLIB_closeall( void )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST
#include "_PDCLIB/io.h"
#include <threads.h>
extern _PDCLIB_file_t * _PDCLIB_filelist;
extern mtx_t _PDCLIB_filelist_lock;

void _PDCLIB_closeall( void )
{
    mtx_lock( &_PDCLIB_filelist_lock );
    _PDCLIB_file_t * stream = _PDCLIB_filelist;
    _PDCLIB_file_t * next;
    while ( stream != NULL )
    {
        next = stream->next;
        fclose( stream );
        stream = next;
    }
    mtx_unlock( &_PDCLIB_filelist_lock );
}
#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    /* No testdriver */
    return TEST_RESULTS;
}

#endif

