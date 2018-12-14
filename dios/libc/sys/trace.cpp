// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#include <cstdio>
#include <dios/sys/kernel.hpp> // get_debug
#include <rst/common.h> // for weaken
#include <fcntl.h>
#include <dios.h>
#include <cstring>
#include <cstdarg>

namespace __dios {

void Debug::persist()
{
    __vm_trace( _VM_T_DebugPersist, this );
    __vm_trace( _VM_T_DebugPersist, hids.begin() );
    __vm_trace( _VM_T_DebugPersist, trace_indent.begin() );
    __vm_trace( _VM_T_DebugPersist, trace_buf.begin() );
}

void Debug::persist_buffers()
{
    for ( auto &b : trace_buf )
        __vm_trace( _VM_T_DebugPersist, b.second.begin() );
}

__inline static void traceInternalV( int shift, const char *fmt, va_list ap ) noexcept
{
    bool kernel = __vm_ctl_flag( 0, 0 ) & _VM_CF_KernelMode;

    int nice_id = -1;
    short *indent = nullptr;
    auto tid = __dios_this_task();

    if ( have_debug() )
    {
        auto key = abstract::weaken( tid );
        auto &hids = get_debug().hids;
        indent = tid ? &get_debug().trace_indent[ key ] : &get_debug().kernel_indent;
        auto nice_id_it = tid ? hids.find( key ): hids.end();
        nice_id = nice_id_it == hids.end() ? -2 : nice_id_it->second;

        if ( indent && shift < 0 && *indent > 0 )
        {
            *indent += shift * 2;
            get_debug().persist();
        }
    }

    if ( *fmt )
    {
        char buffer[1024];
        int n = 0;

        if ( kernel && tid )
            n = snprintf( buffer, 1024, "[%d] ", nice_id );
        else if ( tid )
            n = snprintf( buffer, 1024, "(%d) ", nice_id );
        else
            n = snprintf( buffer, 1024, "[  ] " );

        if ( indent )
            for ( int i = 0; i < *indent && i < 32 && *indent >= 0; ++i )
                buffer[ n++ ] = ' ';

        vsnprintf( buffer + n, 1024 - n, fmt, ap );
        __vm_trace( _VM_T_Text, buffer );
    }

    if ( have_debug() && indent && shift > 0 && *indent < 32 )
    {
        *indent += shift * 2;
        get_debug().persist();
    }
}

void traceInternal( int indent, const char *fmt, ... ) noexcept
{
    __dios_assert( have_debug() );
    va_list ap;
    va_start( ap, fmt );
    traceInternalV( indent, fmt, ap );
    va_end( ap );
}

} // namespace __dios

__debugfn void __dios_trace_t( const char *txt ) noexcept
{
    __dios::traceInternal( 0, "%s", txt );
}

__debugfn void __dios_trace_f( const char *fmt, ... ) noexcept
{
    va_list ap;
    va_start( ap, fmt );
    __dios::traceInternalV( 0, fmt, ap );
    va_end( ap );
}

__debugfn void __dios_trace_v( const char *fmt, va_list ap ) noexcept
{
    __dios::traceInternalV( 0, fmt, ap );
}

void __dios_trace_i( int indent_level, const char* fmt, ... ) noexcept
{
    va_list ap;
    va_start( ap, fmt );

    char buffer[1024];
    int indent = indent_level * 2;
    for ( int i = 0; i < indent; ++i )
        buffer[ i ] = ' ';
    vsnprintf( buffer + indent, 1024 - indent, fmt, ap );
    __vm_trace( _VM_T_Info, buffer );
    va_end( ap );
}

__debugfn void __dios_trace( int indent, const char *fmt, ... ) noexcept
{
    va_list ap;
    va_start( ap, fmt );
    __dios::traceInternalV( indent, fmt, ap );
    va_end( ap );
}

__debugfn void __dios_trace_auto( int indent, const char *fmt, ... ) noexcept
{
    uintptr_t flags = __vm_ctl_flag( 0, 0 );

    if ( flags & _VM_CF_KernelMode )
        return;

    va_list ap;
    va_start( ap, fmt );
    __dios::traceInternalV( indent, fmt, ap );
    va_end( ap );
}

