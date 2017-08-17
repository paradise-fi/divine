/* memmove( void *, const void *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <stdint.h>
#include <sys/divm.h>

#ifndef REGTEST

__attribute__((__annotate__("lart.interrupt.skipcfl")))
__attribute__((__annotate__("divine.link.always")))
void * memmove( void * s1, const void * s2, size_t n )
{
    char *dest = ( char * ) s1;
    const char *src = ( const char * ) s2;
    if ( dest <= src ) {
        while ( ( uint64_t ) dest % 8 && ( uint64_t ) src % 8 && n ) {
            *dest++ = *src++;
            n--;
        }

        uint64_t *src_l = ( uint64_t * ) src;
        uint64_t *dest_l = ( uint64_t * ) dest;
        while ( n >= sizeof( uint64_t ) ) {
            *dest_l++ = *src_l++;
            n -= sizeof( uint64_t );
        }

        src = ( const char * ) src_l;
        dest = ( char *) dest_l;
        while ( n-- ) {
            *dest++ = *src++;
        }

    } else {
        src += n;
        dest += n;

        while ( ( uint64_t ) dest % 8 && ( uint64_t ) src % 8 && n ) {
            *--dest = *--src;
            n--;
        }

        uint64_t *src_l = ( uint64_t * ) src;
        uint64_t *dest_l = ( uint64_t * ) dest;
        while ( n >= sizeof( uint64_t ) ) {
            *--dest_l = *--src_l;
            n -= sizeof( uint64_t );
        }

        src = ( const char * ) src_l;
        dest = ( char *) dest_l;
        while ( n-- ) {
            *--dest = *--src;
        }
    }

#if 0
    /* TODO optimize */
    char * dest = (char *) s1;
    const char * src = (const char *) s2;
    if ( dest <= src )
    {
        while ( n-- )
        {
            *dest++ = *src++;
        }
    }
    else
   {
        src += n;
        dest += n;
        while ( n-- )
        {
           *--dest = *--src;
        }
    }
#endif
    return s1;
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

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
