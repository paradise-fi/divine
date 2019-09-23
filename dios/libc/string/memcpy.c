/* memcpy( void *, const void *, size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <assert.h>
#include <stdint.h>

#ifndef REGTEST

__link_always __local_skipcfl
void * memcpy( void * _PDCLIB_restrict s1, const void * _PDCLIB_restrict s2, size_t n )
{
    assert( s1 < s2 ? s1 + n <= s2 : s2 + n <= s1 );

    char *dst = ( char * ) s1;
    const char *src = ( const char * ) s2;

    switch ( n )
    {
        case 1: *dst = *src; return s1;
        case 2: *( uint16_t * ) dst = *( uint16_t * ) src; return s1;
        case 4: *( uint32_t * ) dst = *( uint32_t * ) src; return s1;
        case 8: *( uint64_t * ) dst = *( uint64_t * ) src; return s1;
        default:
        {
            while ( ( uint64_t ) dst % 8 && ( uint64_t ) src % 8 && n )
                *dst++ = *src++, n --;

            uint64_t *src_l = ( uint64_t * ) src;
            uint64_t *dst_l = ( uint64_t * ) dst;

            while ( n >= 8 )
            {
                *dst_l++ = *src_l++;
                n -= 8;
            }

            src = ( const char * ) src_l;
            dst = ( char *) dst_l;
            while ( n-- )
                *dst++ = *src++;
        }
        return s1;
    }
}

#endif

#ifdef TEST
#include "_PDCLIB_test.h"

int main( void )
{
    char s[] = "xxxxxxxxxxx";
    TESTCASE( memcpy( s, abcde, 6 ) == s );
    TESTCASE( s[4] == 'e' );
    TESTCASE( s[5] == '\0' );
    TESTCASE( memcpy( s + 5, abcde, 5 ) == s + 5 );
    TESTCASE( s[9] == 'e' );
    TESTCASE( s[10] == 'x' );
    return TEST_RESULTS;
}
#endif
