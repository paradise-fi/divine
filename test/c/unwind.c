#include <assert.h>
#include <divine.h>
#include <dios.h>

void f( void (*pc)( void ) )
{
    struct _VM_Frame *frame = __vm_control( _VM_CA_Get, _VM_CR_Frame );
    __dios_unwind( frame->parent, pc );
    assert( 0 );
}

int main()
{
    f( &&next );
    assert( 0 );
next:
    return 0;
}
