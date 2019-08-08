#include <rst/common.hpp>

uint64_t __tainted = 0;

// TODO call from abstraction init
__attribute__((constructor)) void __tainted_init()
{
    __vm_poke( &__tainted, _VM_ML_Taints, 0xF );
    __vm_poke( &__tainted_ptr, _VM_ML_Taints, 0xF );
}

