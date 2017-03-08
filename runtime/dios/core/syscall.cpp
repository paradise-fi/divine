// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <dios/core/syscall.hpp>
#include <dios/core/fault.hpp>
#include <dios/core/scheduling.hpp>
#include "fault.hpp"
#include <cerrno>

namespace __sc {
// Mapping of syscodes to implementations
#define SYSCALL(n,...) extern void n ( __dios::Context& ctx, int *err, void *retval, va_list vl );
    #include <sys/syscall.def>
} // namespace __sc


namespace __sc_passthru {
// Mapping of syscodes to implementations
#define SYSCALL(n,...) extern void n ( __dios::Context& ctx, int *err, void *retval, va_list vl );
    #include <sys/syscall.def>
} // namespace __sc_passthru

namespace __dios {

const SC_Handler _DiOS_SysCalls_Virt[ SYS_MAXSYSCALL ] =
{
    #define SYSCALL(n,...)  [ SYS_ ## n ] = __sc::n,
        #include <sys/syscall.def>
    #undef SYSCALL
};

const SC_Handler _DiOS_SysCalls_Passthru[ SYS_MAXSYSCALL ] =
{
    #define SYSCALL(n,...)  [ SYS_ ## n ] = __sc_passthru::n,
        #include <sys/syscall.def>
    #undef SYSCALL
};

const SC_Handler *_DiOS_SysCalls = _DiOS_SysCalls_Virt;

const SchedCommand _DiOS_SysCallsSched[ SYS_MAXSYSCALL ] =
{
    #define SYSCALL(n, sched,...) [ SYS_ ## n ] = sched,
        #include <sys/syscall.def>
    #undef SYSCALL
};

} // namespace _dios
