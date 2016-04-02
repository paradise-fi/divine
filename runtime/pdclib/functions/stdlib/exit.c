/* $Id$ */

/* exit( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>

#ifndef REGTEST
#include <_PDCLIB_io.h>

/* TODO - "except that a function is called after any previously registered
   functions that had already been called at the time it was registered.
*/

#ifndef __divine__

/* TODO: 32 is guaranteed. This should be dynamic but ATM gives problems
   with my malloc.
*/
#define NUMBER_OF_SLOTS 40

void (*_PDCLIB_regstack[ NUMBER_OF_SLOTS ])( void ) = { _PDCLIB_closeall };
size_t _PDCLIB_regptr = NUMBER_OF_SLOTS;

void exit( int status )
{
    while ( _PDCLIB_regptr < NUMBER_OF_SLOTS )
    {
        _PDCLIB_regstack[ _PDCLIB_regptr++ ]();
    }
    _Exit( status );
}

#else

#include <divine.h>

/* with __divine__, closeall is atexit'd by fopen (fvopen) */
void (**_PDCLIB_regstack)( void ) = NULL;
size_t _PDCLIB_regptr = 0;

void exit( int status )
{

    if ( _PDCLIB_regstack )
        for ( int i = 1; i <= _PDCLIB_regptr; ++i )
            _PDCLIB_regstack[ _PDCLIB_regptr - i ]();

    __vm_free_object( _PDCLIB_regstack );
    _Exit( status );
}

#endif

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    /* Unwinding of regstack tested in atexit(). */
    return TEST_RESULTS;
}

#endif
