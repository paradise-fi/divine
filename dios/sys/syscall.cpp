// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <signal.h>
#include <cerrno>

#include <dios/sys/syscall.hpp>

namespace __dios
{

#include <sys/no_memory_tags.def>
#define SYSCALL( name, schedule, ret, arg ) \
    ret BaseContext:: name arg { \
        __dios_trace( 0, "Syscall " #name " not implemented in this configuration" ); \
        __dios_fault( (_VM_Fault) _DiOS_F_Assert, "Syscall not implemented" ); \
        __builtin_unreachable(); \
    }
#include <sys/syscall.def>
#include <sys/no_memory_tags.undef>
#undef SYSCALL

} // namespace _dios

