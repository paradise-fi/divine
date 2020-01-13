#include <sys/divm.h>
#include <dios.h> /* __dios_assert */

void __dios_assert_debug()
{
    __dios_assert( ( __vm_ctl_flag( 0, 0 ) & _VM_CF_DebugMode ) && __vm_ctl_get( _VM_CR_User3 ) );
}
