#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <_PDCLIB_aux.h>
#include <stdarg.h>

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

void __dios_trap( int syscode, int* err, void* ret, va_list *args ) _PDCLIB_nothrow;
void __dios_syscall( int syscode, void* ret, ... ) _PDCLIB_nothrow;

#endif
