#include <dios.h>
#include <cassert>

int main() {
    __dios::InterruptMask m;
    volatile int *a = nullptr;
    {
        __dios::SetFaultTemporarily f( _VM_F_Memory, _DiOS_FC_Report );
        int x = *a;
    }
    assert( uintptr_t( __vm_control( _VM_CA_Get, _VM_CR_Flags,
                          _VM_CA_Bit, _VM_CR_Flags, uintptr_t( _VM_CF_Error ), uintptr_t( 0 ) ) ) & _VM_CF_Error );
}
