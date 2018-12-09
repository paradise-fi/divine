#include <sys/cdefs.h>
#include <sys/divm.h>
#include <sys/interrupt.h>
#include <sys/stack.h>
#include <sys/fault.h>
#include <cstdint>

#define INTR                                                                                     \
    if ( flags & _VM_CF_KernelMode )                                                             \
        __dios_fault( _VM_F_Control, "oops, interrupted in kernel mode" );                       \
                                                                                                 \
    __vm_ctl_flag( _DiOS_CF_Deferred, 0 );                                                       \
    *reinterpret_cast< void ** >( __vm_ctl_get( _VM_CR_User1 ) ) = __dios_this_frame()->parent;  \
    __vm_suspend();

extern "C" __link_always __trapfn __invisible __weakmem_direct void __dios_reschedule()
{
    uint64_t flags = uint64_t( __vm_ctl_get( _VM_CR_Flags ) );

    if ( flags & _DiOS_CF_Mask )
    {
        __vm_ctl_flag( 0, _DiOS_CF_Deferred );
        return;
    }

    INTR
}

extern "C" __link_always __trapfn __invisible __weakmem_direct void __dios_suspend()
{
    uint64_t flags = uint64_t( __vm_ctl_get( _VM_CR_Flags ) );

    if ( flags & _DiOS_CF_Mask )
        return;

    INTR
}
