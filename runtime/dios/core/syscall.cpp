// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <dios/core/syscall.hpp>
#include <dios/core/fault.hpp>
#include <dios/core/scheduling.hpp>
#include "fault.hpp"
#include <signal.h>
#include <cerrno>

namespace __dios {

#include <dios/macro/syscall_common>
#include <dios/macro/no_memory_tags>
#define SYSCALL( name, schedule, ret, arg ) \
    ret BaseContext:: name ( int * IF(NOT(EMPTY arg ))(,) UNNAMED_ARGS arg ) { \
        __dios_trace( 0, "Syscall " #name " not implemented in this configuration" ); \
        __dios_fault( (_VM_Fault) _DiOS_F_Assert, "Syscall not implemented" ); \
        __builtin_unreachable(); \
    }
#define SYSCALLSEP( ... ) EVAL( SYSCALL( __VA_ARGS__ ) )

    #include <sys/syscall.def>

#include <dios/macro/no_memory_tags.cleanup>
#include <dios/macro/syscall_common.cleanup>
#undef SYSCALL
#undef SYSCALLSEP

} // namespace _dios

