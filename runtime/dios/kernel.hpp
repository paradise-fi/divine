// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <cstdint>
#include <dios/core/stdlibwrap.hpp>
#include <sys/monitor.h>

namespace __dios {

namespace fs {
    struct VFS;
}

template < class T, class... Args >
T *new_object( Args... args ) {
    T* obj = static_cast< T * >( __vm_obj_make( sizeof( T ) ?: 1 ) );
    new (obj) T( args... );
    return obj;
}

template < class T >
void delete_object( T *obj ) {
    obj->~T();
    __vm_obj_free( obj );
}

using SysOpts = Vector< std::pair< String, String > >;

struct Scheduler;
struct Syscall;
struct Fault;
using VFS = fs::VFS;

struct MachineParams {
    int hardwareConcurrency;

    void initialize( const SysOpts& opts );
    void traceParams( int indent );
};

typedef void ( *sighandler_t )( int );

struct Context {
    Scheduler *scheduler;
    Fault *fault;
    VFS *vfs;
    void *globals;
    Monitor *monitors;
    MachineParams machineParams;
    sighandler_t *sighandlers;

    Context();
    void finalize();
};


} // namespace __dios
