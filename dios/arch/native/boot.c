#include <sys/divm.h>
#include <sys/trace.h>
#include <sys/cdefs.h>

void __dios_boot( const struct _VM_Env * );

__link_always void __dios_native_boot()
{
    struct _VM_Env env[] = { { "divine.bcname", "dios_native", 7 }, { 0, 0, 0 } };
    __vm_trace( _VM_T_Text, "about to __boot" );
    __dios_boot( env );
    __vm_trace( _VM_T_Text, "about to run the scheduler" );
    ( ( void(*)() ) __vm_ctl_get( _VM_CR_Scheduler ) )();
}
