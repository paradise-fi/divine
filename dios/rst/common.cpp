#include <rst/common.h>

using namespace abstract;

uint64_t __tainted = 0;
void * __tainted_ptr = nullptr;

__attribute__((constructor)) void __tainted_init()
{
    __vm_poke( &__tainted, _VM_ML_Taints, 0xF );
}

