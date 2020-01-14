/* TAGS: min c */
#include <sys/divm.h>
#include <sys/interrupt.h>
#include <assert.h>

// V: default
// V: suspend CC_OPT: -DSUSPEND

int main()
{
    int a;
    __vm_pointer_t ptr = __vm_pointer_split( &a );
    __vm_poke( _VM_ML_User, ptr.obj, ptr.off, 1, 32 );
#ifdef SUSPEND
    __dios_suspend();
#endif
    __vm_meta_t meta = __vm_peek( _VM_ML_User, ptr.obj, ptr.off, 1 );
    assert( meta.value == 32 );
    return 0;
}
