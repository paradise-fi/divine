#include <rst/common.hpp>

uint64_t __tainted = 0;

// TODO call from abstraction init
__attribute__((constructor)) void __tainted_init()
{
    __vm_pointer_t ptr = __vm_pointer_split( &__tainted );
    __vm_poke( _VM_ML_Taints, ptr.obj, ptr.off, 4, 0xF );
    __vm_poke( _VM_ML_Taints, ptr.obj, ptr.off + 4, 4, 0xF );
}

