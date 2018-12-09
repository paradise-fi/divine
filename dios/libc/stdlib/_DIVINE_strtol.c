/* strtol( const char *, char * *, int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef REGTEST

#include <stdint.h>
#include <_PDCLIB/int.h>

// See _PDCLIB_io.h
long int _DIVINE_strtol( const char *s, int size, char **endptr )
{
    const char *p = s;
    long int res = 0;
    const char *x;

    while ( p - s < size && ( x = memchr( _PDCLIB_digits, *p, 10 ) ) != NULL )
    {
        res = res * 10 + ( x - _PDCLIB_digits );
        ++p;
    }

    if ( endptr != NULL )
        *endptr = (char *) p;

    return res;
}

#endif
