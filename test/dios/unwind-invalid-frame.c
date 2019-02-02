/* TAGS: min c */
#include <assert.h>
#include <sys/divm.h>
#include <dios.h>
#include <stddef.h>

/* ERRSPEC: __dios_unwind */

void f( void (*pc)( void ) )
{
    struct _VM_Frame *fakeFrame = __vm_obj_make( sizeof( struct _VM_Frame ), _VM_PT_Heap );
    fakeFrame->pc = 0;
    fakeFrame->parent = 0;
    __dios_unwind( NULL, NULL, fakeFrame );
    assert( 0 );
}

int main()
{
    f( &&next );
    assert( 0 );
next:
    return 0;
}
