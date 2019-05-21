#include <sys/metadata.h>

extern const _MD_Function __md_functions[];
extern const int __md_functions_count;

extern "C" __invisible const _MD_Function *__md_get_pc_meta( _VM_CodePointer pc )
{
    uintptr_t fun = (uintptr_t( pc ) & _VM_PM_Obj) >> _VM_PB_Off;
    fun &= ~_VM_PL_Global; // get function index from code pointer's objid
    __dios_assert_v( int( fun - 1 ) < __md_functions_count, "invalid function index" );
    return __md_functions + fun - 1; // there is no function 0
}

