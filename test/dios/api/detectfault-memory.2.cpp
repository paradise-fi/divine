#include <dios.h>
#include <cassert>

int main() {
    volatile int *a = nullptr;
    {
        __dios::InterruptMask m;
        __dios::DetectFault f( _VM_F_Memory );
        int x = *a;
        assert( f.triggered() );
    }
    // avoid having two errors in one state (for the sake of sim)
    __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Interrupted, _VM_CF_Interrupted );
    assert( !__dios::DetectFault::triggered() );
    assert( __dios_get_fault_config( _VM_F_Memory ) == _DiOS_FC_Abort );
    int x = *a; /* ERROR */
    return x;
}
