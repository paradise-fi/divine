#ifndef __SYS_TRACE_H__
#define __SYS_TRACE_H__

#include <_PDCLIB/cdefs.h>
#include <stdlib.h>
#include <stdarg.h>

_PDCLIB_EXTERN_C

void __dios_trace_adjust( int ) _PDCLIB_nothrow;
void __dios_trace( int indent, const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_internal( int indent, const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_auto( int indent, const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_t( const char *str ) _PDCLIB_nothrow;
void __dios_trace_v( const char *str, va_list ap ) _PDCLIB_nothrow;
void __dios_trace_f( const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_i( int indent_level, const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_out( const char *msg, size_t size)  _PDCLIB_nothrow;
int __dios_clear_file( const char *name )  _PDCLIB_nothrow;

_PDCLIB_EXTERN_END

#ifdef __cplusplus

struct __dios_trace_restore
{
    __inline ~__dios_trace_restore() { __dios_trace_adjust( -1 ); }
};

__inline static inline __dios_trace_restore __dios_trace_inhibit()
{
    __dios_trace_adjust( 1 );
    return __dios_trace_restore(); /* RVO */
}

#endif

#endif
