#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <_PDCLIB_aux.h>
#include <stdarg.h>

/* argument types for dios syscalls */
#include <sys/types.h>
#include <sys/task.h>
#include <sys/monitor.h>

typedef enum
{
    SYS_NONE = 0,

    #define SYSCALL(n,...) SYS_ ## n,
        #include <sys/syscall.def>
    #undef SYSCALL

    SYS_MAXSYSCALL
} _DiOS_SC;

typedef struct
{
    _DiOS_SC _syscode;
    int *_err;
    void *_ret;
    va_list _args;
} _DiOS_Syscall;

#ifndef __dios_kernel__
_PDCLIB_EXTERN_C

void __dios_syscall( int syscode, void* ret, ... ) _PDCLIB_nothrow;

#define SYSCALL(...)
#define SYSCALL_DIOS( name, sched, ret, args ) ret __dios_ ## name args __nothrow;
#include <dios/macro/no_memory_tags>
#include <sys/syscall.def>
#include <dios/macro/no_memory_tags.cleanup>
#undef SYSCALL
#undef SYSCALL_DIOS

_PDCLIB_EXTERN_END
#endif

#endif
