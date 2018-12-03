// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

namespace __dios
{

static _PthreadAtFork atForkHandlers;

/* Process */
int pthread_atfork( void ( *prepare )( void ), void ( *parent )( void ),
                    void ( *child )( void ) ) noexcept
{
    if ( __vm_choose( 2 ) )
        return ENOMEM;
    atForkHandlers.init();
    int i = atForkHandlers.getFirstAvailable();
    atForkHandlers[i].prepare = prepare;
    atForkHandlers[i].parent = parent;
    atForkHandlers[i].child = child;
    return 0;
}

void __run_atfork_handlers( uint16_t index ) noexcept
{
    atForkHandlers.init();
    auto invoke = []( void (*h)() ){ if ( h ) h(); };
    auto &h = atForkHandlers;

    if ( index == 0 )
        for ( int i = h.count() - 1; i >= 0; --i )
            invoke( h[i].prepare );
    else
        for ( int i = 0; i < h.count(); ++ i )
            if ( index == 1 )
                invoke( h[i].parent );
            else
            {
                getThread().is_main = true;
                invoke( h[i].child );
            }
}

}
