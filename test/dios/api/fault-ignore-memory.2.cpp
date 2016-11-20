#include <dios.h>
#include <cassert>

int main() {
    __dios::InterruptMask m;
    volatile int *a = nullptr;
    {
        __dios::SetFaultTemporarily f( _VM_F_Memory, _DiOS_FC_Ignore );
        int x = *a;
    }
}
