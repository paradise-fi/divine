/* $Id$ */

/* memmove( void *, const void *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <divine.h>

#ifndef REGTEST

void * memmove( void * s1, const void * s2, size_t n )
{
    return __divine_memcpy( s1, ( void * )s2, n );
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    char s[] = "xxxxabcde";
    TESTCASE( memmove( s, s + 4, 5 ) == s );
    TESTCASE( s[0] == 'a' );
    TESTCASE( s[4] == 'e' );
    TESTCASE( s[5] == 'b' );
    TESTCASE( memmove( s + 4, s, 5 ) == s + 4 );
    TESTCASE( s[4] == 'a' );
    return TEST_RESULTS;
}
#endif
