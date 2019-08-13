#include <rst/lart.h>
#include <sys/task.h>
#include <rst/common.hpp>

#include <string.h>

using namespace __dios::rst::abstract;

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

    _LART_INTERFACE void __lart_freeze( void * value, void * addr )
    {
        poke_object( value, addr );
    }

    _LART_INTERFACE void * __lart_thaw( void * addr )
    {
        return peek_object< void * >( addr );
    }
}
