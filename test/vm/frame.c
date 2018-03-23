/* TAGS: min c */
#include <assert.h>
#include <sys/divm.h>
#include <dios.h>

void f()
{
    struct _VM_Frame *frame = __vm_ctl_get( _VM_CR_Frame );
    __vm_ctl_set( _VM_CR_Frame, frame->parent );
    assert( 0 );
}

int main()
{
    f();
}
