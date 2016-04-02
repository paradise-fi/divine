// -*- C++ -*- (c) 2013 Petr Rockai <me@mornfall.net>

#include <limits.h>
#include <unistd.h>
#include <cstdlib>
#include <_PDCLIB_aux.h>
#include <divine.h>
#include <string.h>
#include "../filesystem/fs-manager.h"

namespace divine { namespace fs { VFS vfs; } }

/*
 * Glue code that ties various bits of C and C++ runtime to the divine runtime
 * support. It's not particularly pretty. Other bits of this code are also
 * exploded over external/ which is even worse.
 */

/* Memory allocation */
[[noinline]] void * malloc( size_t size ) noexcept
{
    int masked = __vm_mask( 1 );
    void *r;
    if ( __vm_choose( 2 ) )
        r = __vm_make_object( size ); // success
    else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

#define MIN( a, b )   ((a) < (b) ? (a) : (b))

[[noinline]] void *realloc( void *orig, size_t size ) noexcept
{
    int masked = __vm_mask( 1 );
    void *r;
    if ( !size ) {
        __vm_free_object( orig );
        r = NULL;
    }
    else if ( __vm_choose( 2 ) ) {
        void *n = __vm_make_object( size );
        if ( orig ) {
            ::memcpy( n, orig, MIN( size, __vm_query_object_size( orig ) ) );
            __vm_free_object( orig );
        }
        r = n;
    } else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

[[noinline]] void *calloc( size_t n, size_t size ) noexcept
{
    int masked = __vm_mask( 1 );
    void *r;
    if ( __vm_choose( 2 ) ) {
        void *mem = __vm_make_object( n * size ); // success
        memset( mem, 0, n * size );
        r = mem;
    } else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

void free( void * p) _PDCLIB_nothrow { return __vm_free_object( p ); }

/* IOStream */

void *__dso_handle; /* this is emitted by clang for calls to __cxa_exit for whatever reason */

extern "C" int __cxa_atexit( void ( *func ) ( void * ), void *arg, void *dso_handle ) {
    // TODO
    ( void ) func;
    ( void ) arg;
    ( void ) dso_handle;
    return 0;
}

extern "C" void *dlsym( void *, void * ) { __vm_fault( vm::Fault::NotImplemented ); return 0; }
extern "C" void *__errno_location() { __vm_fault( vm::Fault::NotImplemented ); return 0; }

extern "C" void _PDCLIB_Exit( int rv )
{
    if ( rv )
        __vm_fault( vm::Fault::Control, "exit called with non-zero value" );
    __vm_fault( vm::Fault::NotImplemented ); // TODO unwind
}

extern "C" int nanosleep(const struct timespec *req, struct timespec *rem) {
    // I believe we will do nothing wrong if we verify nanosleep as NOOP,
    // it does not guearantee anything anyway
    return 0;
}

extern "C" unsigned int sleep( unsigned int seconds ) {
    // same as nanosleep
    return 0;
}

extern "C" int usleep( useconds_t usec ) { return 0; }

extern "C" void _exit( int rv )
{
    _PDCLIB_Exit( rv );
}
