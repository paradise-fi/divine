// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <dios/syscall.h>
#include <dios/scheduling.h>

void __dios_syscall_trap() noexcept {
    int mask = __vm_mask( 1 );
    __vm_interrupt( 1 );
    __vm_mask( 0 );
    __vm_mask( 1 );
    __vm_mask( mask );
}

void __dios_syscall( int syscode, void* ret, ... ) {
    int mask = __vm_mask( 1 );

    va_list vl;
    va_start( vl, ret );
    __dios::Syscall::call( syscode, ret, vl );
    va_end( vl );
    __vm_mask( mask );
}

namespace __dios {

Syscall *Syscall::_inst;

void ( *_DiOS_SysCalls[ _SC_LAST ] ) ( Context& ctx, void* retval, va_list vl ) = {
    [ _SC_START_THREAD ] = __sc_start_thread,
    [ _SC_GET_THREAD_ID ] = __sc_get_thread_id,
    [ _SC_KILL_THREAD ] = __sc_kill_thread,
    [ _SC_DUMMY ] = __sc::dummy,
    [ _SC_CONFIGURE_FAULT ] = __sc::configure_fault,
    [ _SC_GET_FAULT_CONFIG ] = __sc::get_fault_config
};

} // namespace _dios
