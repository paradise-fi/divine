/* TAGS: min c */
#include <dios.h>
#include <assert.h>
#include <stdlib.h>

int main() {
    int orig = __dios_get_fault_config( _VM_F_Memory );
    int ret = __dios_configure_fault( _VM_F_Memory, _DiOS_FC_NoFail );
    assert( ret == _DiOS_FC_EInvalidCfg );
    assert( orig == __dios_get_fault_config( _VM_F_Memory ) );
    ret = __dios_configure_fault( _VM_F_Memory, _DiOS_FC_SimFail );
    assert( ret == _DiOS_FC_EInvalidCfg );
    assert( orig == __dios_get_fault_config( _VM_F_Memory ) );

    int pre = __dios_get_fault_config( _VM_F_Memory );
    ret = __dios_configure_fault( _VM_F_Memory, _DiOS_FC_Ignore );
    __dios_trace_f( "%d %d", pre, ret);
    assert( ret == pre );
    int *a = NULL;
    *a = 42;

    pre = __dios_get_fault_config( _VM_F_Memory );
    ret = __dios_configure_fault( _VM_F_Memory, _DiOS_FC_Report );
    assert( ret == pre );
    int *b = NULL;
    *b = 42; /* ERROR */
}
