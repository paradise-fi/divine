#define _DIOS_NORM_SYSCALLS
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/monitor.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/mount.h>
#include <sys/argpad.hpp>
#include <fcntl.h>
#include <signal.h>
#include <dios.h>

#include <sys/no_memory_tags.def>

namespace __dios
{
#define SYSCALL_DIOS(...)
#define SYSCALL( name, schedule, ret, arg )                                     \
    extern "C" ret __libc_ ## name arg noexcept                                 \
    {                                                                           \
        return unpad( &SysProxy::name, _1, _2, _3, _4, _5, _6 );                \
    }                                                                           \
    extern "C" ret name arg noexcept __attribute__ ((weak, alias ("__libc_" #name)));
#include <sys/syscall.def>

#undef SYSCALL
#undef SYSCALL_DIOS
#define SYSCALL(...)
#define SYSCALL_DIOS( name, schedule, ret, arg )                                \
    extern "C" __noinline ret __dios_ ## name arg noexcept                      \
    {                                                                           \
        return unpad( &SysProxy::name, _1, _2, _3, _4, _5, _6 );                \
    }
#include <sys/syscall.def>
#undef SYSCALL_DIOS
#undef SYSCALL

}
