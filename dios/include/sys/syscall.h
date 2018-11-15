#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <_PDCLIB/cdefs.h>
#include <stdarg.h>

/* argument types for dios syscalls */
#include <sys/types.h>
#include <sys/task.h>
#include <sys/monitor.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/utsname.h>

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

#ifdef __cplusplus
#include <signal.h>

namespace __dios
{
    struct SysProxy
    {
        #define SYSCALL( name, schedule, ret, arg ) virtual ret name arg = 0;
        #include <dios/macro/no_memory_tags>
        #include <sys/syscall.def>
        #include <dios/macro/no_memory_tags.cleanup>
        #undef SYSCALL
    };
}

#endif

#ifndef __dios_kernel__
_PDCLIB_EXTERN_C

long syscall( int number, ... ) __nothrow;

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
