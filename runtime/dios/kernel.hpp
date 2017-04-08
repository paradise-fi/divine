// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <cstdint>
#include <cstdarg>
#include <dios/core/stdlibwrap.hpp>
#include <sys/monitor.h>
#include <sys/utsname.h>

#define DIOS_DBG( ... ) __dios_trace_f( __VA_ARGS__ )

namespace __dios {

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

enum SchedCommand { RESCHEDULE, CONTINUE };

using SysOpts = Vector< std::pair< String, String > >;
using SC_Handler = SchedCommand (*)( void *ctx, int *err, void* retval, va_list vl );

struct sighandler_t
{
    void ( *f )( int );
    int sa_flags;
};

struct TraceDebugConfig {
    bool threads:1;
    bool help:1;
    bool raw:1;
    bool machineParams:1;
    bool mainArgs:1;
    bool faultCfg:1;

    TraceDebugConfig() :
        threads( false ), help( false ), raw( false ), machineParams( false ),
        mainArgs( false ), faultCfg( false ) {}

    bool anyDebugInfo() {
        return help || raw || machineParams || mainArgs || faultCfg;
    }
};

bool useSyscallPassthrough( const SysOpts& o );
TraceDebugConfig getTraceDebugConfig( const SysOpts& o );


} // namespace __dios
