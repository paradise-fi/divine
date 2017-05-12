// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <cstdint>
#include <cstdarg>
#include <dios/core/stdlibwrap.hpp>
#include <sys/monitor.h>
#include <sys/utsname.h>

#define DIOS_DBG( ... ) __dios_trace_f( __VA_ARGS__ )

namespace __dios {

// DiOS register constants
static const uint64_t _DiOS_CF_SyscallSchedule = 0b000001 << 8;

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

struct HelpOption {
    String description;
    Vector< String > options;
};

bool useSyscallPassthrough( const SysOpts& o );


} // namespace __dios
