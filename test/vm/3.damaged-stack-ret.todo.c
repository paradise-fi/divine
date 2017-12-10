#include <assert.h>
#include <sys/divm.h>
#include <dios.h>

void fun()
{
    struct _VM_Frame *frame = __vm_control( _VM_CA_Get, _VM_CR_Frame );
    __vm_obj_free( frame->parent );
}

int main()
{
    fun(); /* ERROR */
    assert( 0 );
}
