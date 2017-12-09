#include <dios.h>
#include <assert.h>

int main()
{
    __dios::InterruptMask mask;
    int *a = NULL;
    __dios_configure_fault( _VM_F_Memory, _DiOS_FC_Ignore );
    *a = 42;

    __dios_configure_fault( _VM_F_Memory, _DiOS_FC_Abort );
    *a = 42; /* ERROR */
}
