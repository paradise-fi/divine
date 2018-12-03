// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

using namespace __dios;

/* Thread cancellation */
int pthread_setcancelstate( int state, int *oldstate ) noexcept
{
    __dios::FencedInterruptMask mask;

    if ( state & ~0x1 )
        return EINVAL;

    _PThread &thread = getThread();
    if ( oldstate )
        *oldstate = thread.cancel_state;
    thread.cancel_state = state & 1;
    return 0;
}

int pthread_setcanceltype( int type, int *oldtype ) noexcept
{
    __dios::FencedInterruptMask mask;

    if ( type & ~0x1 )
        return EINVAL;

    _PThread &thread = getThread();
    if ( oldtype )
        *oldtype = thread.cancel_type;
    thread.cancel_type = type & 1;
    return 0;
}

int pthread_cancel( pthread_t gtid ) noexcept
{
    __dios::FencedInterruptMask mask;

    _PThread *thread = nullptr;
    {
        __dios::DetectFault f( _VM_F_Memory );
        thread = &getThread( gtid );
        if ( f.triggered() )
            return ESRCH;
    }

    if ( thread->cancel_state == PTHREAD_CANCEL_ENABLE )
        thread->cancelled = true;

    return 0;
}

void pthread_testcancel( void ) noexcept
{
    __dios::FencedInterruptMask mask;

    if ( _canceled() )
        _cancel( mask );
}

void pthread_cleanup_push( void ( *routine )( void * ), void *arg ) noexcept
{
    __dios::FencedInterruptMask mask;

    assert( routine != NULL );

    _PThread &thread = getThread();
    CleanupHandler *handler = reinterpret_cast< CleanupHandler * >(
            __vm_obj_make( sizeof( CleanupHandler ) ) );
    handler->routine = routine;
    handler->arg = arg;
    handler->next = thread.cleanup_handlers;
    thread.cleanup_handlers = handler;
}

void pthread_cleanup_pop( int execute ) noexcept
{
    __dios::FencedInterruptMask mask;

    _PThread &thread = getThread();
    CleanupHandler *handler = thread.cleanup_handlers;
    if ( handler != NULL ) {
        thread.cleanup_handlers = handler->next;
        if ( execute ) {
            handler->routine( handler->arg );
        }
        __vm_obj_free( handler );
    }
}
