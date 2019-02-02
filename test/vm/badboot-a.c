/* TAGS: min c */
#include <sys/divm.h>

void __boot() /* BOOT ERROR */
{
    void *st = __vm_obj_make( 4, _VM_PT_Heap );
    __vm_ctl_set( _VM_CR_State, st );
    __vm_obj_free( st );
}

int main() {}
