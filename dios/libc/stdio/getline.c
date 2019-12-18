/* $Id$ */

/* getline( char **lineptr, size_t *n, FILE *stream )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdio.h>

#ifndef REGTEST
#include <_PDCLIB/io.h>
#include <stdint.h>
#include <stdlib.h>

#define INIT_SIZE       10

ssize_t _PDCLIB_getline_unlocked( char **lineptr, size_t *n, FILE *stream )
{
    if ( !lineptr )
        return -1;

    if ( !(*lineptr) || *n <= 1 )
    {
        *lineptr = realloc( *lineptr, INIT_SIZE );
        if ( !(*lineptr) )
            return -1;
        *n = INIT_SIZE;
    }

    if ( _PDCLIB_prepread( stream ) == EOF )
    {
        return -1;
    }

    char *dest = *lineptr;
    int read, tread = 0, nfree = *n;

    do {
        read = _PDCLIB_getchars( dest, nfree - 1, '\n', stream );
        tread += read;
        if ( ( read == nfree - 1 ) && ( dest[read-1] != '\n' ) ) {
            *lineptr = realloc( *lineptr, (*n) * 2 );
            if ( !(*lineptr) )
                return -1;
            nfree = *n + 1;
            *n = (*n) * 2;
            dest = *lineptr + tread;
        } else {
            dest[read] = '\0';
            break;
        }
    } while ( 1 );

    return tread > 0 ? tread : -1 ;
}

ssize_t getline( char **lineptr, size_t *n, FILE *stream )
{
    _PDCLIB_flockfile( stream );
    int read = _PDCLIB_getline_unlocked( lineptr, n, stream );
    _PDCLIB_funlockfile( stream );
    return read;
}

#endif

#ifdef TEST
#include <_PDCLIB_test.h>
#include <string.h>

int main( void )
{
    FILE * fh;
    char *buffer = malloc( 3 );
    size_t n = 3;
    char const * getline_test = "foo\nbar\0baz\nweenie";
    TESTCASE( ( fh = fopen( testfile, "wb" ) ) != NULL );
    TESTCASE( fwrite( getline_test, 1, 18, fh ) == 18 );
    TESTCASE( fclose( fh ) == 0 );
    TESTCASE( ( fh = fopen( testfile, "r" ) ) != NULL );
    TESTCASE( getline( &buffer, &n, fh  ) == 4 );
    TESTCASE( strcmp( buffer, "foo\n" ) == 0 );
    TESTCASE( getline( &buffer, &n, fh ) == 8 );
    TESTCASE( memcmp( buffer, "bar\0baz\n", 8 ) == 0 );
    TESTCASE( getline( &buffer, &n, fh ) == 6 );
    TESTCASE( strcmp( buffer, "weenie" ) == 0 );
    TESTCASE( getline( &buffer, &n, fh ) == -1 );
    TESTCASE( feof( fh ) );
    TESTCASE( fseek( fh, -7, SEEK_END ) == 0 );
    TESTCASE( getline( &buffer, &n, fh ) == 1 );
    TESTCASE( strcmp( buffer, "\n" ) == 0 );
    TESTCASE( fseek( fh, 0, SEEK_END ) == 0 );
    TESTCASE( getline( &buffer, &n, fh ) == -1 );
    TESTCASE( fclose( fh ) == 0 );
    TESTCASE( remove( testfile ) == 0 );
    return TEST_RESULTS;
}

#endif
