// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_SYSCALL_H__
#define __DIOS_SYSCALL_H__

#include <stdarg.h>

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#define NOTHROW noexcept
#else
#define EXTERN_C
#define CPP_END
#define NOTHROW __attribute__((__nothrow__))
#endif

EXTERN_C

// Syscodes
enum _DiOS_SC {
    _SC_INACTIVE = 0,
    _SC_START_THREAD,
    _SC_GET_THREAD_ID,
    _SC_KILL_THREAD,
    _SC_DUMMY,

    _SC_LAST
};

// Mapping of syscodes to implementations
extern void ( *_DiOS_SysCalls[_SC_LAST] ) ( void* retval, va_list vl );

void __sc_start_thread( void *retval, va_list vl );
void __sc_kill_thread( void *retval, va_list vl );
void __sc_get_thread_id( void *retval, va_list vl );
void __sc_dummy( void* retval, va_list vl );


CPP_END

#endif // __DIOS_SYSCALL_H__