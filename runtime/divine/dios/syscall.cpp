// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <divine/dios/syscall.h>
#include <divine/dios/scheduling.h>

__dios::Syscall *__dios::Syscall::instance;

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
    __dios::Syscall::get().call( syscode, ret, vl );
    va_end( vl );
    __vm_mask( mask );
}

void ( *_DiOS_SysCalls[ _SC_LAST ] ) ( void* retval, va_list vl ) = {
    [ _SC_START_THREAD ] = __sc_start_thread,
    [ _SC_GET_THREAD_ID ] = __sc_get_thread_id,
    [ _SC_KILL_THREAD ] = __sc_kill_thread,
    [ _SC_DUMMY ] = __sc_dummy
};

