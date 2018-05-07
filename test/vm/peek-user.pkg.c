/* TAGS: min c */
#include <sys/divm.h>
#include <sys/interrupt.h>
#include <assert.h>

// V: default
// V: suspend CC_OPT: -DSUSPEND

int main()
{
    int a;
    __vm_poke( &a, _VM_ML_User, 32 );
#ifdef SUSPEND
    __dios_suspend();
#endif
    int p = __vm_peek( &a, _VM_ML_User );
    assert( p == 32 );
    return 0;
}
