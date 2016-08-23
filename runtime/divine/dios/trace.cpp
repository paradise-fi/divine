// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//                 2016 Vladimir Still <xstill@fi.muni.cz>

#include <cstdio>
#include <divine/dios/trace.h>

namespace __dios {

bool InTrace::inTrace = false;

void traceInternalV( int indent, const char *fmt, va_list ap ) noexcept __attribute__((always_inline))
{
    static int fmtIndent = 0;
    InTrace _;

    char buffer[1024];
    for ( int i = 0; i < fmtIndent; ++i )
        buffer[ i ] = ' ';

    vsnprintf( buffer + fmtIndent, 1024, fmt, ap );
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
    __vm_trace( _VM_T_Text, txt );
}

void __dios_trace_f( const char *fmt, ... ) noexcept
{
    int mask = __vm_mask(1);

    if ( __dios::InTrace::inTrace )
        goto unmask;

    va_list ap;
    va_start( ap, fmt );
    __dios::traceInternalV( 0, fmt, ap );
    va_end( ap );
unmask:
    __vm_mask(mask);
}

void __dios_trace( int indent, const char *fmt, ... ) noexcept
{
    int mask = __vm_mask(1);

    if ( __dios::InTrace::inTrace )
        goto unmask;

    va_list ap;
    va_start( ap, fmt );
    __dios::traceInternalV( indent, fmt, ap );
    va_end( ap );
unmask:
    __vm_mask(mask);
}
