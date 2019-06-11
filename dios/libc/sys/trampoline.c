#include <dios.h>
#include <sys/divm.h>
#include <sys/signal.h>

int __dios_signal_trampoline_ret( void *a, void * b, int ret )
{
    void (*handler)( int ) = a;
    int arg = (int) b;
    handler( arg );
    return ret;
}

int __dios_signal_trampoline_noret( void *a, void *b, int ret )
{
    ( void ) ret;
    void (*handler)( int ) = a;
    int arg = (int) b;
    handler( arg );
    struct _VM_Frame *self = __vm_ctl_get( _VM_CR_Frame );
    __vm_ctl_set( _VM_CR_Frame, self->parent );
    __builtin_unreachable();
}

int __dios_simple_trampoline( void *a, void * b, int ret )
{
    ( void ) a;
    ( void ) b;
    return ret;
}
