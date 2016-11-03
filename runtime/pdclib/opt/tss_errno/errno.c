/* $Id$ */

/* _PDCLIB_errno

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef REGTEST

#ifdef __divine__
#include <dios.h>

int * _PDCLIB_errno_func() { return __dios_get_errno(); }

#else
#include <pthread.h>

static pthread_key_t key;
static pthread_once_t once = PTHREAD_ONCE_INIT;

void  _PDCLIB_delete_errno( void *errno ) {
    free( errno );
}

void  _PDCLIB_init_errno ( void ) {
    pthread_key_create( &key, _PDCLIB_delete_errno );
}

int * _PDCLIB_errno_func() {
    pthread_once( &once, _PDCLIB_init_errno );

    if ( !pthread_getspecific( key )) {
        int *errno = malloc( sizeof( int ) );
        *errno = 0;
        pthread_setspecific( key, errno );
    }

    return (int *)pthread_getspecific( key );
}
#endif

#endif

#ifdef TEST
#include <_PDCLIB_test.h>

#include <errno.h>

int main( void )
{
    errno = 0;
    TESTCASE( errno == 0 );
    errno = EDOM;
    TESTCASE( errno == EDOM );
    errno = ERANGE;
    TESTCASE( errno == ERANGE );
    return TEST_RESULTS;
}

#endif

