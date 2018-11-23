/* $Id$ */

/* strerror_r( int, char * , size_t )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <string.h>
#include <errno.h>

#ifndef REGTEST

#include <_PDCLIB/locale.h>

#define MIN( a, b ) ( (a) < (b) ? (a) : (b) )

/* The GNU-specific strerror_r() returns a pointer to a string
   containing the error message. This may be either a pointer to
   a string that the function stores in buf, or a pointer to some
   (immutable) static string (in which case buf is unused).
   If the function stores a string in buf, then at most buflen bytes
   are stored (the string may be truncated if buflen is too small
   and errnum is unknown). The string always includes a terminating
   null byte.
*/
#if _HOST_is_glibc
        int __xpg_strerror_r( int errnum, char *buf, size_t buflen )
#else
        int strerror_r( int errnum, char *buf, size_t buflen )
#endif
    {
        const char *err;
        if ( errnum == 0 || errnum >= _PDCLIB_ERRNO_MAX
             || _PDCLIB_threadlocale()->_ErrnoStr[errnum] == NULL )
            err = "Unknown error";
        else
            err = _PDCLIB_threadlocale()->_ErrnoStr[errnum];

        size_t errlen = strlen( err );
        int len = MIN( buflen - 1, errlen );
        memcpy( buf, err, len );
        buf[ len ] = '\0';

        if ( errnum == 0 || errnum >= _PDCLIB_ERRNO_MAX )
            return EINVAL;
        return buflen <= errlen? ERANGE : 0;
    }

#ifdef _HOST_is_glibc // _GNU_SOURCE
    char * strerror_r( int errnum, char *buf, size_t buflen )
    {
        if ( errnum == 0 || errnum >= _PDCLIB_ERRNO_MAX )
            return (char *)"Unknown error";
        else
        {
            const char *src = _PDCLIB_threadlocale()->_ErrnoStr[errnum];
            int len = MIN( buflen - 1, strlen( src ) );
            memcpy( buf, src, len );
            buf[ len ] = '\0';
            return buf;
        }
    }
#endif

#endif // REGTEST

#ifdef TEST
#include <_PDCLIB_test.h>

#include <stdio.h>
#include <errno.h>

int main( void )
{
    char buf1[128], buf2[128];
    TESTCASE( strcmp( strerror_r( ERANGE, buf1, 128 ), strerror_r( EDOM, buf2, 128 ) ) );
    TESTCASE( strcmp( strerror_r( ERANGE, buf1 + 125, 3 ), strerror_r( EDOM, buf2 + 125, 3 ) ) );
    return TEST_RESULTS;
}
#endif
