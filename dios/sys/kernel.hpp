// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#pragma once

#include <cstdint>
#include <cstdarg>

#include <util/map.hpp>
#include <dios.h>
#include <dios/sys/kobject.hpp>
#include <dios/sys/debug.hpp>

#define DIOS_DBG( ... ) __dios_trace_f( __VA_ARGS__ )

namespace __dios
{

template < typename T >
void __attribute__((__noinline__)) traceAlias( const char *name, T *type = nullptr ) {
    __vm_trace( _VM_T_TypeAlias, type, name );
}

using SysOpts = Array< std::pair< std::string_view, std::string_view > >;
using SC_Handler = void (*)( void *ctx, int *err, void* retval, va_list vl );

struct sighandler_t
{
    void ( *f )( int );
    int sa_flags;
};

struct HelpOption
{
    std::string_view description;
    Array< std::string_view > options;
};

bool useSyscallPassthrough( const SysOpts& o );

template< typename Context >
static inline Context &get_state()
{
    return *static_cast< Context * >( __vm_ctl_get( _VM_CR_State ) );
}

} // namespace __dios
