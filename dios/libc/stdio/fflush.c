/* fflush( FILE * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST
#include "_PDCLIB/io.h"
#include <threads.h>

extern FILE * _PDCLIB_filelist;
extern mtx_t _PDCLIB_filelist_lock;

int _PDCLIB_fflush_unlocked( FILE * stream )
{
    if ( stream == NULL )
    {
        mtx_lock( &_PDCLIB_filelist_lock );
        stream = _PDCLIB_filelist;
        /* TODO: Check what happens when fflush( NULL ) encounters write errors, in other libs */
        int rc = 0;
        while ( stream != NULL )
        {
            if ( stream->status & _PDCLIB_FWRITE )
            {
                if ( _PDCLIB_flushbuffer( stream ) == EOF )
                {
                    rc = EOF;
                }
            }
            stream = stream->next;
        }
        mtx_unlock( &_PDCLIB_filelist_lock );
        return rc;
    }
    else
    {
        return _PDCLIB_flushbuffer( stream );
    }
}

int fflush( FILE * stream )
{
    if ( stream )
        _PDCLIB_flockfile( stream );
    int res = _PDCLIB_fflush_unlocked(stream);
    if ( stream )
        _PDCLIB_funlockfile( stream );
    return res;
}
                
#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    /* Testing covered by ftell.c */
    return TEST_RESULTS;
}

#endif

