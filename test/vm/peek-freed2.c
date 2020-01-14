/* TAGS: min c */
#include <sys/divm.h>
#include <assert.h>
#include <stdlib.h>

int main()
{
    int *a = malloc( 4 );
    if ( a )
    {
        __vm_pointer_t ptr = __vm_pointer_split( a );
        __vm_poke( _VM_ML_User, ptr.obj, ptr.off, 1, 20 );
        free( a );
        __vm_peek( _VM_ML_User, ptr.obj, ptr.off, 1 ); /* ERROR */
    }
    return 0;
}
