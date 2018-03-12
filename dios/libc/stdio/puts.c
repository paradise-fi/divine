/* puts( const char * )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST
#include "_PDCLIB/io.h"

extern char * _PDCLIB_eol;

int puts( const char * s )
{
    int ret = fputs( s, stdout );
    putchar('\n');
    return ret;
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    FILE * fh;
    char const * message = "SUCCESS testing puts()";
    char buffer[23];
    buffer[22] = 'x';
    TESTCASE( ( fh = freopen( testfile, "wb+", stdout ) ) != NULL );
    TESTCASE( puts( message ) >= 0 );
    rewind( fh );
    TESTCASE( fread( buffer, 1, 22, fh ) == 22 );
    TESTCASE( memcmp( buffer, message, 22 ) == 0 );
    TESTCASE( buffer[22] == 'x' );
    TESTCASE( fclose( fh ) == 0 );
    TESTCASE( remove( testfile ) == 0 );
    return TEST_RESULTS;
}

#endif

