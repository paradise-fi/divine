/*
 * This is a DiOS configuration used in the static checking (SHOOP) mode.
 * At the moment, it is derived from the null configuration (i.e. it traps on every syscall)
 */

#include <sys/types.h>
#include <sys/syscall.h>
#include <dios/macro/no_memory_tags>

#define SYSCALL( name, schedule, ret, arg ) ret SysProxy::name arg noexcept { __builtin_trap(); }

namespace __dios
{
    #include <sys/syscall.def>
}

#undef SYSCALL
#include <dios/macro/no_memory_tags.cleanup>

#include <sys/divm.h>
#include <sys/task.h>
#include <sys/cdefs.h>

#include <dios.h>

__BEGIN_DECLS

// This is the hook point for the currently tested function
void __tester();

void __fault_handler( _VM_Fault, _VM_Frame *, void (*)() )
{
    __vm_ctl_flag( 0, _VM_CF_Error );

    __vm_suspend();
}

void __scheduler()
{
    __dios_task task = __CAST( __dios_task, __vm_obj_make( sizeof( struct __dios_tls ), _VM_PT_Heap ) );
    __vm_ctl_set( _VM_CR_User2, task );

    __tester();

    __vm_suspend();
}

void __dios_boot( const _VM_Env * )
{
    __vm_ctl_set( _VM_CR_FaultHandler, reinterpret_cast< void * >( __fault_handler ) );
    __vm_ctl_set( _VM_CR_Scheduler, reinterpret_cast< void * >( __scheduler ) );

    __vm_ctl_set( _VM_CR_State, __vm_obj_make( 4, _VM_PT_Heap ) );

    __vm_suspend();
}

void __boot( const _VM_Env *env )
{
    __dios_boot( env );
    __vm_suspend();
}

__END_DECLS