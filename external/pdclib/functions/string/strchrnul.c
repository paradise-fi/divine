/* $Id$ */

/* strchrnul( const char *, int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

#ifndef REGTEST

char * strchrnul( const char * s, int c )
{
    while ( ( *s != (char) c ) && ( *s != '\0' ) )
    {
        s++;
    }
    return (char *)s;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    char abccd[] = "abccd";
    TESTCASE( strchrnul( abccd, 'x' ) == &abccd[5] );
    TESTCASE( strchrnul( abccd, 'a' ) == &abccd[0] );
    TESTCASE( strchrnul( abccd, 'd' ) == &abccd[4] );
    TESTCASE( strchrnul( abccd, '\0' ) == &abccd[5] );
    TESTCASE( strchrnul( abccd, 'c' ) == &abccd[2] );
    return TEST_RESULTS;
}
#endif
