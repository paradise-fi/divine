/* $Id$ */

/* rawmemchr( const void *s, int c )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

#ifndef REGTEST

void *rawmemchr( const void *s, int c )
{
    const unsigned char * p = (const unsigned char *) s;
    while ( 1 )
    {
        if ( *p == (unsigned char) c )
        {
            return (void *) p;
        }
        ++p;
    }
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    TESTCASE( rawmemchr( abcde, 'c' ) == &abcde[2] );
    TESTCASE( rawmemchr( abcde, 'a' ) == &abcde[0] );
    TESTCASE( rawmemchr( abcde, '\0' ) == &abcde[5] );
    return TEST_RESULTS;
}

#endif
