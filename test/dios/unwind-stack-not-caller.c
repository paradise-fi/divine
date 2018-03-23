/* TAGS: min c */
#include <assert.h>
#include <sys/divm.h>
#include <dios.h>
#include <stddef.h>

int main()
{
    struct _VM_Frame *frame = __vm_ctl_get( _VM_CR_Frame );
    __dios_unwind( NULL, frame, NULL );
    assert( 0 );
}
