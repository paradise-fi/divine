// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <cstdint>
#include <cstdarg>

#include <dios/core/stdlibwrap.hpp>
#include <dios/lib/map.hpp>
#include <dios.h>

#define DIOS_DBG( ... ) __dios_trace_f( __VA_ARGS__ )

namespace __dios {

// DiOS register constants
static const uint64_t _DiOS_CF_SyscallSchedule = 0b000001 << 8;

template < typename T >
void __attribute__((__noinline__)) traceAlias( const char *name, T *type = nullptr ) {
    __vm_trace( _VM_T_TypeAlias, type, name );
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

struct Debug
{
    AutoIncMap< _DiOS_TaskHandle, int > hids;
    ArrayMap< _DiOS_TaskHandle, short > trace_indent;
    short kernel_indent = 0;
};

static inline Debug &get_debug() noexcept
{
    void *dbg = __vm_control( _VM_CA_Get, _VM_CR_User1 );
    return *static_cast< Debug * >( dbg );
}

} // namespace __dios
