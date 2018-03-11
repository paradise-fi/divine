/* $Id$ */

/* strcasecmp( const char *s1, const char *s2 )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <ctype.h>

#ifndef REGTEST

int strcasecmp( const char *s1, const char *s2 )
{
    const unsigned char *p1 = ( const unsigned char * ) s1;
    const unsigned char *p2 = ( const unsigned char * ) s2;
    int result;

    if ( p1 == p2 )
      return 0;

    if ( !p1 )
        return -1;

    if ( !p2 )
        return 1;

    while ( ( result = tolower( *p1 ) - tolower( *p2++ ) ) == 0 )
      if ( *p1++ == '\0' )
        break;

    return result;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    char aBcdE[] = "aBcdE";
    char cmpabcde[] = "abcde";
    char cmpabcd_[] = "abcd\xfc";
    char empty[] = "";
    TESTCASE( strcasecmp( abcde, cmpabcde ) == 0 );
    TESTCASE( strcasecmp( aBcdE, cmpabcde ) == 0 );

    TESTCASE( strcasecmp( abcde, abcdx ) < 0 );
    TESTCASE( strcasecmp( aBcdE, abcdx ) < 0 );

    TESTCASE( strcasecmp( abcdx, abcde ) > 0 );
    TESTCASE( strcasecmp( abcdx, aBcdE ) > 0 );

    TESTCASE( strcasecmp( empty, abcde ) < 0 );
    TESTCASE( strcasecmp( empty, aBcdE ) < 0 );

    TESTCASE( strcasecmp( abcde, empty ) > 0 );
    TESTCASE( strcasecmp( aBcdE, empty ) > 0 );

    TESTCASE( strcasecmp( abcde, cmpabcd_ ) < 0 );
    TESTCASE( strcasecmp( aBcdE, cmpabcd_ ) < 0 );

    return TEST_RESULTS;
}
#endif
