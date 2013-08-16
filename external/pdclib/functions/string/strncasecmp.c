/* $Id$ */

/* strncasecmp( const char *s1, const char *s2, size_t n )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <ctype.h>

#ifndef REGTEST

int strncasecmp( const char *s1, const char *s2, size_t n )
{
    const unsigned char *p1 = ( const unsigned char * ) s1;
    const unsigned char *p2 = ( const unsigned char * ) s2;
    int result = 0;

    if ( p1 == p2 )
        return 0;

    if ( !p1 )
        return -1;

    if ( !p2 )
        return 1;

    while ( n && ( result = tolower( *p1 ) - tolower( *p2++ ) ) == 0 ) {
        n--;
        if ( *p1++ == '\0' )
            break;
    }

    return result;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    char aBcdE[] = "aBcdE";
    char cmpabcde[] = "abcde\0f";
    char cmpabcd_[] = "abcde\xfc";
    char empty[] = "";
    char x[] = "x";
    TESTCASE( strncasecmp( abcde, cmpabcde, 5 ) == 0 );
    TESTCASE( strncasecmp( aBcdE, cmpabcde, 5 ) == 0 );

    TESTCASE( strncasecmp( abcde, cmpabcde, 10 ) == 0 );
    TESTCASE( strncasecmp( aBcdE, cmpabcde, 10 ) == 0 );

    TESTCASE( strncasecmp( abcde, abcdx, 5 ) < 0 );
    TESTCASE( strncasecmp( aBcdE, abcdx, 5 ) < 0 );

    TESTCASE( strncasecmp( abcdx, abcde, 5 ) > 0 );
    TESTCASE( strncasecmp( abcdx, aBcdE, 5 ) > 0 );

    TESTCASE( strncasecmp( empty, abcde, 5 ) < 0 );
    TESTCASE( strncasecmp( empty, aBcdE, 5 ) < 0 );

    TESTCASE( strncasecmp( abcde, empty, 5 ) > 0 );
    TESTCASE( strncasecmp( aBcdE, empty, 5 ) > 0 );

    TESTCASE( strncasecmp( abcde, abcdx, 4 ) == 0 );
    TESTCASE( strncasecmp( aBcdE, abcdx, 4 ) == 0 );

    TESTCASE( strncasecmp( abcde, x, 0 ) == 0 );
    TESTCASE( strncasecmp( aBcdE, x, 0 ) == 0 );

    TESTCASE( strncasecmp( abcde, x, 1 ) < 0 );
    TESTCASE( strncasecmp( aBcdE, x, 1 ) < 0 );

    TESTCASE( strncasecmp( abcde, cmpabcd_, 10 ) < 0 );
    TESTCASE( strncasecmp( aBcdE, cmpabcd_, 10 ) < 0 );
    return TEST_RESULTS;
}
#endif
