// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_H__
#define __DIOS_H__

#include <sys/divm.h>
#include <sys/types.h>
#include <sys/trace.h>
#include <sys/fault.h>
#include <sys/interrupt.h>
#include <sys/stack.h>
#include <sys/monitor.h>

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define CPP_END }
#define CAST( T, X ) reinterpret_cast< T >( X )
#if __cplusplus >= 201103L
#define NOTHROW noexcept
#else
#define NOTHROW throw()
#endif
#else
#define EXTERN_C
#define CPP_END
#define NOTHROW __attribute__((__nothrow__))
#define CAST( T, X ) ((T)(X))
#endif

EXTERN_C

#include <stddef.h>

struct _DiOS_TLS {
    int _errno;
    char data[ 0 ];
};

typedef struct _DiOS_TLS * _DiOS_TaskHandle;
typedef void ( *_DiOS_TaskRoutine )( void * );

static inline int __dios_pointer_get_type( void *ptr ) NOTHROW
{
    unsigned long p = (unsigned long) ptr;
    return ( p & _VM_PM_Type ) >> _VM_PB_Off;
}

static inline void *__dios_pointer_set_type( void *ptr, int type ) NOTHROW
{
    unsigned long p = (unsigned long) ptr;
    p &= ~_VM_PM_Type;
    unsigned long newt = ( type << _VM_PB_Off ) & _VM_PM_Type;
    return (void *)( p | newt );
}

/*
 * Start a new task and obtain its identifier. Task starts executing routine
 * with arg.
 * - tls_size is the total size of TLS, _DiOS_TLS_Reserved must be included in this,
 *   if tls_size is less then _DiOS_TLS_Reserved at least _DiOS_TLS_Reserved is allocated
 * - the resulting _DiOS_TaskHandle points to the beginning of TLS. Userspace is
 *   allowed to use it from offset _DiOS_TLS_Reserved
 */
_DiOS_TaskHandle __dios_start_task( void ( *routine )( void * ), void *arg, int tls_size ) NOTHROW;

/*
 * Get caller task id
 *
 * - the resulting _DiOS_TaskHandle points to the beginning of TLS. Userspace is
 *   allowed to use it from offset _DiOS_TLS_Reserved
 */
static inline _DiOS_TaskHandle __dios_get_task_handle() NOTHROW
{
    return CAST( _DiOS_TaskHandle, __vm_control( _VM_CA_Get, _VM_CR_User2 ) );
}

/*
 * get pointer to errno, which is in dios-managed task-local data (accessible
 * to userspace, but independent of ptasking library)
 */
static inline int *__dios_get_errno() NOTHROW
{
    return &( __dios_get_task_handle()->_errno );
}


/*
 * Kill task with given id.
 */
void __dios_kill_task( _DiOS_TaskHandle id ) NOTHROW;

_DiOS_TaskHandle *__dios_get_process_tasks() NOTHROW;

/*
 * Return number of claimed hardware concurrency units, specified in DiOS boot
 * parameters.
 */
int __dios_hardware_concurrency() NOTHROW;

void __run_atfork_handlers( unsigned short index ) NOTHROW;

void __dios_exit_process( int code ) NOTHROW;

void __dios_yield() NOTHROW;

#define __dios_assert_v( x, msg ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d: %s", \
                __FILE__, __LINE__, msg ); \
            __dios_fault( (_VM_Fault) _DiOS_F_Assert, "DiOS assert failed" ); \
        } \
    } while (0)

#define __dios_assert( x ) do { \
        if ( !(x) ) { \
            __dios_trace( 0, "DiOS assert failed at %s:%d", \
                __FILE__, __LINE__ ); \
            __dios_fault( (_VM_Fault) _DiOS_F_Assert, "DiOS assert failed" ); \
        } \
    } while (0)

CPP_END


#endif // __DIOS_H__

#undef EXTERN_C
#undef CPP_END
#undef NOTHROW
#undef CAST
