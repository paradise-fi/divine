#include <rst/lart.h>
#include <sys/task.h>
#include <rst/common.h>
#include <sys/divm.h>

#include <string.h>

extern "C"
{
    _LART_INTERFACE void * __lart_unstash()
    {
        return reinterpret_cast< void * >( __dios_this_task()->__rst_stash );
    }

    _LART_INTERFACE void __lart_stash( void * val )
    {
        __dios_this_task()->__rst_stash = reinterpret_cast< uintptr_t >( val );
    }
}
