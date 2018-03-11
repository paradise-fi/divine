/* $Id$ */

/* stpcpy( char *, const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>

#ifndef REGTEST

char * stpcpy( char * _PDCLIB_restrict s1, const char * _PDCLIB_restrict s2 )
{
    while ( ( *s1 = *s2 ) ) {
        s1++;
        s2++;
    }
    return s1;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    char s[] = "xxxxx";
    TESTCASE( stpcpy( s, "" ) == s );
    TESTCASE( s[0] == '\0' );
    TESTCASE( s[1] == 'x' );
    TESTCASE( stpcpy( s, abcde ) == s + 5 );
    TESTCASE( s[0] == 'a' );
    TESTCASE( s[4] == 'e' );
    TESTCASE( s[5] == '\0' );
    return TEST_RESULTS;
}
#endif
