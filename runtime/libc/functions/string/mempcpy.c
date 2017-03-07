/* $Id$ */

/* mempcpy( void *, const void *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

#ifndef REGTEST

void * mempcpy( void * _PDCLIB_restrict dest, const void * _PDCLIB_restrict src, size_t n )
{
    memcpy( dest, src, n ); // Call DiVinE builtin for faster execution.
    return dest + n;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    char s[] = "xxxxxxxxxxx";
    TESTCASE( mempcpy( s, abcde, 6 ) == s + 6 );
    TESTCASE( s[4] == 'e' );
    TESTCASE( s[5] == '\0' );
    TESTCASE( mempcpy( s + 5, abcde, 5 ) == s + 5 + 5 );
    TESTCASE( s[9] == 'e' );
    TESTCASE( s[10] == 'x' );
    return TEST_RESULTS;
}
#endif
