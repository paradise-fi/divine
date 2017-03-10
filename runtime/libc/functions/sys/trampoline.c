#include <dios.h>
#include <sys/divm.h>
#include <sys/signal.h>

void __dios_signal_trampoline ( struct _VM_Frame *interrupted, void ( *handler )( int ) )
{
    handler( 0 );
    __dios_jump( interrupted, interrupted->pc, -1 );
    __builtin_unreachable();
}
