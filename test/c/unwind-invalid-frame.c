#include <assert.h>
#include <divine.h>
#include <dios.h>

/* ERRSPEC: __dios_unwind */

void f( void (*pc)( void ) )
{
    struct _VM_Frame *fakeFrame = __vm_obj_make( sizeof( struct _VM_Frame ) );
    fakeFrame->pc = 0;
    fakeFrame->parent = 0;
    __dios_unwind( fakeFrame, pc );
    assert( 0 );
}

int main()
{
    f( &&next );
    assert( 0 );
next:
    return 0;
}
