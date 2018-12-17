/* atexit( void (*)( void ) )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#include <stdlib.h>
#include <stdint.h>

#ifndef REGTEST

#ifndef __divine__

extern void (*_PDCLIB_exitstack[])( void );
extern size_t _PDCLIB_exitptr;

int atexit( void (*func)( void ) )
{
    if ( _PDCLIB_exitptr == 0 )
    {
        return -1;
    }
    else
    {
        _PDCLIB_exitstack[ --_PDCLIB_exitptr ] = func;
        return 0;
    }
}

#else

#include <sys/divm.h>

struct AtexitEntry {
    void ( *func )( void * );
    void *arg;
    void *dso_handle;
};

static struct AtexitEntry *atexit_entries;

static void call(void *p) {
    ((void (*)(void))(uintptr_t)p)();
}

int __cxa_atexit( void ( *func ) ( void * ), void *arg, void *dso_handle ) {
    typedef struct AtexitEntry Ent;
    if ( !atexit_entries )
        atexit_entries = ( Ent* ) __vm_obj_make( sizeof( Ent ) );
    else
        __vm_obj_resize( atexit_entries, __vm_obj_size( atexit_entries ) + sizeof( Ent ) );

    size_t idx = __vm_obj_size( atexit_entries ) / sizeof( Ent ) - 1;
    Ent tmp = { func, arg, dso_handle };
    atexit_entries[ idx ] = tmp;
    return 0;
}

int __cxa_finalize( void *dso_handle ) {
    typedef struct AtexitEntry Ent;
    size_t i = atexit_entries ? __vm_obj_size( atexit_entries ) / sizeof( Ent ) : 0;
    for ( ; i != 0; i-- ) {
        Ent *entry = &atexit_entries[ i - 1 ];
        if ( entry->func && ( !dso_handle || entry->dso_handle == dso_handle ) ) {
            ( *entry->func )( entry->arg );
            entry->func = NULL;
        }
    }
    __vm_obj_free( atexit_entries );
    // ToDo: Remove invalid entries?
    return 0;
}

int atexit( void ( *func )( void ) ) {
    return __cxa_atexit( call, ( void * )( uintptr_t )func, 0);
}

#endif

#endif

#ifdef TEST
#include "_PDCLIB_test.h"
#include <assert.h>

static int flags[ 32 ];

static void counthandler( void )
{
    static int count = 0;
    flags[ count ] = count;
    ++count;
}

static void checkhandler( void )
{
    for ( int i = 0; i < 31; ++i )
    {
        assert( flags[ i ] == i );
    }
}

int main( void )
{
    TESTCASE( atexit( &checkhandler ) == 0 );
    for ( int i = 0; i < 31; ++i )
    {
        TESTCASE( atexit( &counthandler ) == 0 );
    }
    return TEST_RESULTS;
}

#endif
