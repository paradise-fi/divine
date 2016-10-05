// -*- C++ -*- (c) 2013 Petr Rockai <me@mornfall.net>

#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <cstdlib>
#include <_PDCLIB_aux.h>
#include <divine.h>
#include <dios/fault.hpp>
#include <string.h>
#include <dios.h>
#include <dios/main.hpp>

/*
 * Glue code that ties various bits of C and C++ runtime to the divine runtime
 * support. It's not particularly pretty. Other bits of this code are also
 * exploded over external/ which is even worse.
 */

#define __vm_mask(x) ( uintptr_t( \
                           __vm_control( _VM_CA_Get, _VM_CR_Flags,      \
                                         _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, \
                                         (x) ? _VM_CF_Mask : 0 ) ) & _VM_CF_Mask )

/* Memory allocation */
[[noinline]] void * malloc( size_t size ) noexcept
{
    using FaultFlag = __dios::Fault::FaultFlag;
    int masked = __vm_mask( 1 );
    void *r;
    int opt = _DiOS_fault_cfg[ _DiOS_SF_Malloc ] & FaultFlag::Enabled ? 2 : 1;
    if ( !__vm_choose( opt ) )
        r = __vm_obj_make( size ); // success
    else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

#define MIN( a, b )   ((a) < (b) ? (a) : (b))

[[noinline]] void *realloc( void *orig, size_t size ) noexcept
{
    using FaultFlag = __dios::Fault::FaultFlag;
    int masked = __vm_mask( 1 );
    int opt = _DiOS_fault_cfg[ _DiOS_SF_Malloc ] & FaultFlag::Enabled ? 2 : 1;
    void *r;
    if ( !size ) {
        __vm_obj_free( orig );
        r = NULL;
    }
    else if ( !__vm_choose( opt ) ) {
        void *n = __vm_obj_make( size );
        if ( orig ) {
            ::memcpy( n, orig, MIN( size, __vm_obj_size( orig ) ) );
            __vm_obj_free( orig );
        }
        r = n;
    } else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

[[noinline]] void *calloc( size_t n, size_t size ) noexcept
{
    using FaultFlag = __dios::Fault::FaultFlag;
    int masked = __vm_mask( 1 );
    void *r;
    int opt = _DiOS_fault_cfg[ _DiOS_SF_Malloc ] & FaultFlag::Enabled ? 2 : 1;
    if ( !__vm_choose( opt ) ) {
        void *mem = __vm_obj_make( n * size ); // success
        memset( mem, 0, n * size );
        r = mem;
    } else
        r = NULL; // failure
    __vm_mask( masked );
    return r;
}

void free( void * p) _PDCLIB_nothrow { if ( p ) __vm_obj_free( p ); }

/* IOStream */

void *__dso_handle; /* this is emitted by clang for calls to __cxa_exit for whatever reason */

extern "C" int __cxa_atexit( void ( *func ) ( void * ), void *arg, void *dso_handle ) {
    // TODO
    ( void ) func;
    ( void ) arg;
    ( void ) dso_handle;
    return 0;
}

extern "C" void *dlsym( void *, void * ) { __vm_fault( _VM_F_NotImplemented ); return 0; }
extern "C" void *__errno_location() { __vm_fault( _VM_F_NotImplemented ); return 0; }

extern "C" void _PDCLIB_Exit( int rv )
{
    if ( rv )
        __vm_fault( _VM_F_Control, "exit called with non-zero value" );
    __dios::runDtors();
    __dios_kill_thread( 0 );
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
