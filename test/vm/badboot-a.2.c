#include <divine.h>

void __boot()
{
    void *st = __vm_obj_make( 4 );
    __vm_control( _VM_CA_Set, _VM_CR_State, st );
    __vm_obj_free( st );
}

int main() {}
