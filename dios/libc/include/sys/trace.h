#ifndef __SYS_TRACE_H__
#define __SYS_TRACE_H__

#include <_PDCLIB_aux.h>
#include <stdlib.h>

_PDCLIB_EXTERN_C

void __dios_trace( int indent, const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_auto( int indent, const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_t( const char *str ) _PDCLIB_nothrow;
void __dios_trace_f( const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_i( int indent_level, const char *fmt, ... ) _PDCLIB_nothrow;
void __dios_trace_out( const char *msg, size_t size)  _PDCLIB_nothrow;
int __dios_clear_file( const char *name )  _PDCLIB_nothrow;

_PDCLIB_EXTERN_END

#endif
