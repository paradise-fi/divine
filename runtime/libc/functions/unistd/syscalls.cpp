#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/monitor.h>
#include <signal.h>
#include <dios.h>

#include <dios/macro/syscall_common>
#include <dios/macro/no_memory_tags>

#define SYSCALL(name, schedule, ret, arg) \
    extern "C" ret name ( NAMED_ARGS arg ) { \
        IF(IS_VOID(ret)) ( \
            __dios_syscall( SYS_ ## name, nullptr ARG_NAMES arg ); \
        ) \
        IF(NOT(IS_VOID(ret))) ( \
            ret returnVal; \
            __dios_syscall( SYS_ ## name, &returnVal ARG_NAMES arg ); \
            return returnVal; \
        ) \
    } \

#define SYSCALLSEP(...)

    #include <sys/syscall.def>
#include <dios/macro/no_memory_tags.cleanup>
#include <dios/macro/syscall_common.cleanup>

#undef SYSCALL
#undef SYSCALL
