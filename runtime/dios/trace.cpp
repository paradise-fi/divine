// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#include <cstdio>
#include <dios/trace.hpp>
#include <dios.h>

namespace __dios {

bool InTrace::inTrace = false;

void __attribute__((always_inline)) traceInternalV( int indent, const char *fmt, va_list ap ) noexcept
{
    static int fmtIndent = 0;
    InTrace _;

    char buffer[1024];
    for ( int i = 0; i < fmtIndent; ++i )
        buffer[ i ] = ' ';

    int n = 0;
    auto tid = __dios_get_thread_id();
    bool kernel = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags ) ) & _VM_CF_KernelMode;
    if ( !kernel ) {
        auto utid = reinterpret_cast< uint64_t >( tid ) >> 32;
        n = snprintf( buffer + fmtIndent, 1024 - fmtIndent, "t%u: ",
            static_cast< uint32_t >( utid ) );
    }
    __dios_assert( n >= 0 );

    vsnprintf( buffer + fmtIndent + n, 1024 - fmtIndent - n, fmt, ap );
    __vm_trace( _VM_T_Text, buffer );

    fmtIndent += indent * 4;
}

void traceInternal( int indent, const char *fmt, ... ) noexcept
{
    va_list ap;
    va_start( ap, fmt );
    traceInternalV( indent, fmt, ap );
    va_end( ap );
}

} // namespace __dios

void __dios_trace_t( const char *txt ) noexcept
{
    /* __vm_trace( _VM_T_Text, txt ); */
    __dios_trace( 0, txt );
}

void __dios_trace_f( const char *fmt, ... ) noexcept
{
    uintptr_t flags = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) );

    if ( __dios::InTrace::inTrace )
        goto unmask;

    va_list ap;
    va_start( ap, fmt );
    __dios::traceInternalV( 0, fmt, ap );
    va_end( ap );
unmask:
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, flags ); /*  restore */
}

void __dios_trace( int indent, const char *fmt, ... ) noexcept
{
    uintptr_t flags = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) );

    if ( __dios::InTrace::inTrace )
        goto unmask;

    va_list ap;
    va_start( ap, fmt );
    __dios::traceInternalV( indent, fmt, ap );
    va_end( ap );
unmask:
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, flags ); /*  restore */
}
