// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* A bag of DiOS-related functionality. Deprecated, do not use. */

#ifndef __DIOS_H__
#define __DIOS_H__

#include <sys/divm.h>
#include <sys/types.h>
#include <sys/trace.h>
#include <sys/fault.h>
#include <sys/interrupt.h>
#include <sys/stack.h>
#include <sys/monitor.h>
#include <sys/task.h>

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#if __cplusplus >= 201103L
#define NOTHROW noexcept
#else
#define NOTHROW throw()
#endif
#else
#define EXTERN_C
#define CPP_END
#define NOTHROW __attribute__((__nothrow__))
#endif

EXTERN_C

#include <stddef.h>

static inline int __dios_pointer_get_type( void *ptr ) NOTHROW
{
    unsigned long p = (unsigned long) ptr;
    uint32_t obj = p >> _VM_PB_Off;
    return __vm_pointer_type( obj );
}

void __run_atfork_handlers( unsigned short index ) NOTHROW;

void __dios_exit_process( int code ) NOTHROW;

void __dios_yield() NOTHROW;

#define __dios_assert_v( x, msg ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d: %s", \
                __FILE__, __LINE__, msg ); \
            __dios_fault( (enum _VM_Fault) _DiOS_F_Assert, "DiOS assert failed" ); \
        } \
    } while (0)

#define __dios_assert( x ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d", \
                __FILE__, __LINE__ ); \
            __dios_fault( (enum _VM_Fault) _DiOS_F_Assert, "DiOS assert failed" ); \
        } \
    } while (0)

CPP_END


#endif // __DIOS_H__

#undef EXTERN_C
#undef CPP_END
#undef NOTHROW
