/* TAGS: min c */
#include <assert.h>
#include <sys/divm.h>
#include <dios.h>

void f( void (*pc)( void ) )
{
    struct _VM_Frame *frame = __vm_ctl_get( _VM_CR_Frame );
    __dios_jump( frame->parent, pc, -1 );
    assert( 0 );
}

int main()
{
    f( &&next );
    assert( 0 );
next:
    return 0;
}
