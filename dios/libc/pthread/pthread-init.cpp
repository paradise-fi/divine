// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

using namespace __dios;

void __pthread_initialize() noexcept
{
    // initialize implicitly created main thread
    __init_thread( __dios_this_task(), PTHREAD_CREATE_DETACHED );
    getThread().is_main = true;
}

void __pthread_finalize() noexcept
{
    // __vm_obj_free( &getThread() );
}

__noinline void __pthread_start( void *_args )
{
    __dios::FencedInterruptMask mask;

    Entry *args = static_cast< Entry * >( _args );
    auto tid = __dios_this_task();
    _PThread &thread = getThread( tid );

    // copy arguments
    void *arg = args->arg;
    auto entry = args->entry;
    __vm_obj_free( _args );

    // call entry function
    mask.without( [&] {
        thread.result = entry( arg );
    } );

    assert( thread.sleeping == false );

    _clean_and_become_zombie( mask, tid );
}

namespace __dios
{

void __init_thread( const __dios_task gtid, const pthread_attr_t attr ) noexcept
{
    __dios_assert( gtid );

    if ( __vm_obj_size( gtid ) < _PthreadTLS::raw_size( 0 ) )
        __vm_obj_resize( gtid, _PthreadTLS::raw_size( 0 ) );
    auto *thread = static_cast< _PThread * >( __vm_obj_make( sizeof( _PThread ) ) );
    new ( thread ) _PThread();
    tls( gtid ).thread = thread;

    // initialize thread metadata
    thread->running = true;
    thread->detached = ( ( attr & _THREAD_ATTR_DETACH_MASK ) == PTHREAD_CREATE_DETACHED );
    thread->condition = nullptr;
    thread->cancel_state = PTHREAD_CANCEL_ENABLE;
    thread->cancel_type = PTHREAD_CANCEL_DEFERRED;
}

_Noreturn void _clean_and_become_zombie( __dios::FencedInterruptMask &mask,
                                         __dios_task tid ) noexcept
{
    _PThread &thread = getThread( tid );
    // An  optional  destructor  function may be associated with each key
    // value.  At thread exit, if a key value has a non-NULL destructor
    // pointer, and the thread has a non-NULL value associated with that key,
    // the value of the key is set to NULL, and then the function pointed to is
    // called with the previously associated value as its sole argument. The
    // order of destructor calls is unspecified if more than one destructor
    // exists for a thread when it exits.
    //
    // If, after all the destructors have been called for all non-NULL values
    // with associated destructors, there are still some non-NULL values  with
    // associated  destructors, then  the  process  is  repeated.  If,  after
    // at  least {PTHREAD_DESTRUCTOR_ITERATIONS}  iterations  of destructor
    // calls for outstanding non-NULL values, there are still some non-NULL
    // values with associated destructors, implementations may stop calling
    // destructors, or they may continue calling destructors until no
    // non-NULL values with associated destructors exist, even though this
    // might result in an infinite loop.
    int iter = 0;
    bool done;

    auto &tls = __dios::tls( tid );
    do {
        done = true;
        for ( int i = 0; i < tls.keyCount(); ++i )
            if ( tls.getKey( i ) )
            {
                done = false;
                tls.destroy( i );
            }
        ++iter;
    } while ( iter <= PTHREAD_DESTRUCTOR_ITERATIONS && !done );

    thread.running = false;

    if ( thread.is_main )
        exit( 0 );

    if ( thread.detached )
        releaseAndKillThread( tid );
    else // wait until detach / join kills us
        wait( mask, [&] { return true; } );
    __builtin_trap();
}

}
