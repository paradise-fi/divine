// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <cstdint>
#include <cstdarg>

#include <dios/sys/stdlibwrap.hpp>
#include <util/map.hpp>
#include <dios.h>

#define DIOS_DBG( ... ) __dios_trace_f( __VA_ARGS__ )

namespace __dios
{

template < typename T >
void __attribute__((__noinline__)) traceAlias( const char *name, T *type = nullptr ) {
    __vm_trace( _VM_T_TypeAlias, type, name );
}

using SysOpts = Vector< std::pair< String, String > >;
using SC_Handler = void (*)( void *ctx, int *err, void* retval, va_list vl );

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
    AutoIncMap< __dios_task, int > hids;
    ArrayMap< __dios_task, short > trace_indent;
    ArrayMap< __dios_task, String > trace_buf;
    short kernel_indent = 0;
    void persist();
    void persist_buffers();
};

static inline bool debug_mode() noexcept
{
    return __vm_ctl_flag( 0, 0 ) & _VM_CF_DebugMode;
}

static inline bool have_debug() noexcept
{
    return debug_mode() && __vm_ctl_get( _VM_CR_User3 );
}

static inline Debug &get_debug() noexcept
{
    if ( debug_mode() && !have_debug() )
    {
        __vm_trace( _VM_T_Text, "have_debug() failed in debug mode" );
        __vm_ctl_set( _VM_CR_Flags, 0 ); /* fault & force abandonment of the debug call */
    }
    __dios_assert( have_debug() );
    void *dbg = __vm_ctl_get( _VM_CR_User3 );
    return *static_cast< Debug * >( dbg );
}

template< typename Context >
static inline Context &get_state()
{
    return *static_cast< Context * >( __vm_ctl_get( _VM_CR_State ) );
}

} // namespace __dios
