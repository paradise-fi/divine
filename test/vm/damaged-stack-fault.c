/* TAGS: todo c */
#include <assert.h>
#include <sys/divm.h>
#include <dios.h>

void fun()
{
    struct _VM_Frame *frame = __vm_ctl_get( _VM_CR_Frame );
    __vm_obj_free( frame->parent );
    assert( 0 );
}

int main()
{
    fun(); /* ERROR */
    assert( 0 );
}
