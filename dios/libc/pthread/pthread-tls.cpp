// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

using namespace __dios;

using _PthreadTLSDestructors = _PthreadHandlers< Destructor >;
static _PthreadTLSDestructors tlsDestructors;

void __pthread_tls_fini() noexcept { tlsDestructors.fini(); }

void *_PthreadTLS::raw() noexcept
{
    return reinterpret_cast< char * >( this ) - sizeof( __dios_tls );
}

int _PthreadTLS::keyCount() noexcept
{
    return ( __vm_obj_size( raw() ) - raw_size( 0 ) ) / sizeof( void * );
}

void _PthreadTLS::makeFit( int count ) noexcept
{
    int now = keyCount();
    if ( count > now )
    {
        __vm_obj_resize( raw(), raw_size( count ) );
        for ( int i = now; i < count; ++i )
            keys[ i ] = nullptr;
    }
}

void _PthreadTLS::shrink() noexcept
{
    int count = keyCount();
    int toDrop = 0;
    for ( int i = count - 1; i >= 0; --i )
        if ( keys[ i ] == nullptr )
            ++toDrop;
        else
            break;
    if ( toDrop )
        __vm_obj_resize( raw(), raw_size( count - toDrop ) );
}

void *_PthreadTLS::getKey( int key ) noexcept
{
    assert( key >= 0 && key < tlsDestructors.count() );
    if ( key >= keyCount() )
        return nullptr;
    return keys[ key ];
}

void _PthreadTLS::setKey( int key, const void *value ) noexcept
{
    assert( key >= 0 && key < tlsDestructors.count() );
    const int c = keyCount();
    if ( value == nullptr && key >= c )
        return;
    if ( key >= c )
        makeFit( key + 1 );
    keys[ key ] = const_cast< void * >( value );
    if ( value == nullptr )
        shrink();
}

void _PthreadTLS::destroy( int key ) noexcept
{
    auto data = getKey( key );
    setKey( key, nullptr );
    tlsDestructors[ key ]( data );
}

/* DESCRIPTION
 * The pthread_key_create() function shall create a thread-specific data key
 * visible to all threads in the process. Key values provided by
 * pthread_key_create() are opaque objects used to locate thread-specific data.
 * Although the same key value may be used by different threads, the values
 * bound to the key by pthread_setspecific() are maintained on a per-thread
 * basis and persist for the life of the calling thread.

 * Upon key creation, the value NULL shall be associated with the new key in all
 * active threads. Upon thread creation, the value NULL shall be associated with
 * all defined keys in the new thread.

 * An optional destructor function may be associated with each key value. At
 * thread exit, if a key value has a non-NULL destructor pointer, and the thread
 * has a non-NULL value associated with that key, the value of the key is set to
 * NULL, and then the function pointed to is called with the previously associated
 * value as its sole argument. The order of destructor calls is unspecified if
 * more than one destructor exists for a thread when it exits.
 *
 * RETURN VALUE
 * If successful, the pthread_key_create() function shall store the newly created
 * key value at *key and shall return zero. Otherwise, an error number shall be
 * returned to indicate the error.
 *
 * ERRORS
 *
 * The pthread_key_create() function shall fail if:
 *
 * [EAGAIN] The system lacked the necessary resources to create another
 * thread-specific data key, or the system-imposed limit on the total number of
 * keys per process {PTHREAD_KEYS_MAX} has been exceeded.
 *
 * [ENOMEM] Insufficient memory exists to create the key.
 *
 * The pthread_key_create() function shall not return an error code of [EINTR].
 * */
int pthread_key_create( pthread_key_t *p_key, void ( *destructor )( void * ) ) noexcept
{
    __dios::FencedInterruptMask mask;
    tlsDestructors.init();

    *p_key = tlsDestructors.getFirstAvailable();;
    tlsDestructors[ *p_key ] = destructor ?: []( void * ) { };

    // TODO: simulate errors?
    return 0;
}

/* DESCRIPTION
 * The pthread_key_delete() function shall delete a thread-specific data key
 * previously returned by pthread_key_create(). The thread-specific data values
 * associated with key need not be NULL at the time pthread_key_delete() is
 * called. It is the responsibility of the application to free any application
 * storage or perform any cleanup actions for data structures related to the
 * deleted key or associated thread-specific data in any threads; this cleanup
 * can be done either before or after pthread_key_delete() is called. Any
 * attempt to use key following the call to pthread_key_delete() results in
 * undefined behavior.
 *
 * The pthread_key_delete() function shall be callable from within destructor
 * functions. No destructor functions shall be invoked by pthread_key_delete().
 * Any destructor function that may have been associated with key shall no
 * longer be called upon thread exit.
 *
 * RETURN VALUE
 * If successful, the pthread_key_delete() function shall return zero;
 * otherwise, an error number shall be returned to indicate the error.
 *
 * ERRORS
 * The pthread_key_delete() function may fail if:
 *
 * [EINVAL]
 * The key value is invalid.
 *
 * The pthread_key_delete() function shall not return an error code of [EINTR].
 */
int pthread_key_delete( pthread_key_t key ) noexcept
{
    __dios::FencedInterruptMask mask;

    if ( key >= tlsDestructors.count() )
        return EINVAL;

    iterateThreads( [&]( __dios_task tid ) {
            tls( tid ).setKey( key, nullptr );
            tls( tid ).shrink();
        } );
    tlsDestructors[ key ] = nullptr;
    tlsDestructors.shrink();
    return 0;
}

/* DESCRIPTION
 * The pthread_setspecific() function shall associate a thread-specific value
 * with a key obtained via a previous call to pthread_key_create(). Different
 * threads may bind different values to the same key. These values are typically
 * pointers to blocks of dynamically allocated memory that have been reserved for
 * use by the calling thread.
 *
 * The effect of calling pthread_setspecific() with a key value not obtained
 * from pthread_key_create() or after key has been deleted with
 * pthread_key_delete() is undefined.
 *
 * Function pthread_setspecific() may be called from a thread-specific data
 * destructor function. Calling pthread_setspecific() from a thread-specific
 * data destructor routine may result either in lost storage (after at least
 * PTHREAD_DESTRUCTOR_ITERATIONS attempts at destruction) or in an infinite loop.
 *
 * RETURN VALUE
 * If successful, the pthread_setspecific() function shall return zero; otherwise,
 * an error number shall be returned to indicate the error.
 *
 * ERRORS
 * The pthread_setspecific() function shall fail if:
 *
 * [ENOMEM]
 * Insufficient memory exists to associate the value with the key.
 * The pthread_setspecific() function may fail if:
 *
 * [EINVAL]
 * The key value is invalid.
 *
 * The pthread_setspecific() functions shall not return an error code of [EINTR].
 * */
int pthread_setspecific( pthread_key_t key, const void *data ) noexcept
{
    __dios::FencedInterruptMask mask;

    tls( __dios_this_task() ).setKey( key, data );
    // TODO: simulate errors?
    return 0;
}

/* DESCRIPTION
 * The pthread_getspecific() function shall return the value currently bound to
 * the specified key on behalf of the calling thread.
 *
 * The effect of calling pthread_getspecific() with a key value not obtained
 * from pthread_key_create() or after key has been deleted with
 * pthread_key_delete() is undefined.
 *
 * Function pthread_getspecific() may be called from a thread-specific
 * data destructor function. A call to pthread_getspecific() for
 * the thread-specific data key being destroyed shall return the value NULL,
 * unless the value is changed (after the destructor starts) by a call to
 * pthread_setspecific().
 *
 * RETURN VALUE
 * The pthread_getspecific() function shall return the thread-specific data value
 * associated with the given key. If no thread-specific data value is associated
 * with key, then the value NULL shall be returned.
 *
 * ERRORS
 * No errors are returned from pthread_getspecific().
 * */
void *pthread_getspecific( pthread_key_t key ) noexcept
{
    __dios::FencedInterruptMask mask;
    return tls( __dios_this_task() ).getKey( key );
}
