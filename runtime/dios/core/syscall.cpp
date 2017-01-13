// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <dios/core/syscall.hpp>
#include <dios/core/fault.hpp>
#include <dios/core/scheduling.hpp>
#include "fault.hpp"
#include <cerrno>

void __dios_syscall( int syscode, void* ret ... ) {
    uintptr_t fl = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) );
    va_list vl;
    va_start( vl, ret );
    int err = 0;
    __dios::Syscall::trap( syscode, &err, ret,  vl );
    while ( err == _DiOS_SYS_RETRY ) {
            err = 0;
         __dios::Syscall::trap( syscode, &err, ret,  vl );
    }
    if( err != 0) {
        errno = err;
    }
    va_end( vl );
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, fl | _VM_CF_Interrupted ); /*  restore */
}

namespace __sc {
// Mapping of syscodes to implementations
#define SYSCALL(n,...) extern void n ( __dios::Context& ctx, int *err, void *retval, va_list vl );
    #include <dios/core/syscall.def>
} // namespace __sc


namespace __sc_passthru {
// Mapping of syscodes to implementations
#define SYSCALL(n,...) extern void n ( __dios::Context& ctx, int *err, void *retval, va_list vl );
    #include <dios/core/syscall.def>
} // namespace __sc_passthru

namespace __dios {

void ( *_DiOS_SysCalls_Virt[ _SC_LAST ] ) ( Context& ctx, int *err, void* retval, va_list vl ) = {
    #define SYSCALL(n,...)  [ _SC_ ## n ] = __sc::n,
        #include <dios/core/syscall.def>
    #undef SYSCALL
};

void ( *_DiOS_SysCalls_Passthru[ _SC_LAST ] ) ( Context& ctx, int *err, void* retval, va_list vl ) = {
    #define SYSCALL(n,...)  [ _SC_ ## n ] = __sc_passthru::n,
        #include <dios/core/syscall.def>
    #undef SYSCALL
};

void ( **_DiOS_SysCalls )( Context& ctx, int *err, void* retval, va_list vl ) = _DiOS_SysCalls_Virt;

const SchedCommand _DiOS_SysCallsSched[ _SC_LAST ] = {
    #define SYSCALL(n, sched,...) [ _SC_ ## n ] = sched,
        #include <dios/core/syscall.def>
    #undef SYSCALL
};

} // namespace _dios
