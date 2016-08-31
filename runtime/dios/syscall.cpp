// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <dios/syscall.hpp>
#include <dios/scheduling.hpp>

void __dios_syscall_trap() noexcept
{
    uintptr_t fl = reinterpret_cast< uintptr_t >(
        __vm_control( _VM_CA_Get, _VM_CR_Flags,
                      _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask, _VM_CF_Mask ) );
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, _VM_CF_Interrupted );
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Mask | _VM_CF_Interrupted, fl ); /*  restore */
}

void __dios_syscall( int syscode, void* ret, ... ) {
    va_list vl;
    va_start( vl, ret );
    __dios::Syscall::call( syscode, ret, vl );
    va_end( vl );
}

namespace __dios {

Syscall *Syscall::_inst;

void ( *_DiOS_SysCalls[ _SC_LAST ] ) ( Context& ctx, void* retval, va_list vl ) = {
    [ _SC_START_THREAD ] = __sc::start_thread,
    [ _SC_GET_THREAD_ID ] = __sc::get_thread_id,
    [ _SC_KILL_THREAD ] = __sc::kill_thread,
    [ _SC_DUMMY ] = __sc::dummy,

    [ _SC_CONFIGURE_FAULT ] = __sc::configure_fault,
    [ _SC_GET_FAULT_CONFIG ] = __sc::get_fault_config
};

} // namespace _dios
