/* $Id$ */

/* isascii( int )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <ctype.h>

#ifndef REGTEST

int isascii( int c )
{
    return (((c) & ~0x7f) == 0);
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

int main( void )
{
    TESTCASE( isascii( 0 ) );
    TESTCASE( isascii( 'a' ) );
    TESTCASE( isascii( 127 ) );
    TESTCASE( ! isascii( 128 ) );
    TESTCASE( ! isascii( 200 ) );
    TESTCASE( ! isascii( 12345 ) );
    return TEST_RESULTS;
}

#endif
