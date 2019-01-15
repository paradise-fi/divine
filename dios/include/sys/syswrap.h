#ifndef __SYS_SYSWRAP_H__
#define __SYS_SYSWRAP_H__

#include <signal.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>

#include <dios/macro/no_memory_tags>


#define SYSCALL_DIOS(...)
#define SYSCALL( name, schedule, ret, arg )                                     \
    ret __libc_ ## name arg __nothrow;

__BEGIN_DECLS

#include <sys/syscall.def>

__END_DECLS

#undef SYSCALL
#undef SYSCALL_DIOS


#endif
