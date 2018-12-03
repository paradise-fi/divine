// -*- C++ -*- (c) 2013 Milan Lenco <lencomilan@gmail.com>
//             (c) 2014-2018 Vladimír Štill <xstill@fi.muni.cz>
//             (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

/* Includes */
#include <sys/thread.h>

using namespace __dios;

using _PthreadTLSDestructors = _PthreadHandlers< Destructor >;
static _PthreadTLSDestructors tlsDestructors;

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

/* Thread specific data */
int pthread_key_create( pthread_key_t *p_key, void ( *destructor )( void * ) ) noexcept
{
    __dios::FencedInterruptMask mask;
    tlsDestructors.init();

    *p_key = tlsDestructors.getFirstAvailable();;
    tlsDestructors[ *p_key ] = destructor ?: []( void * ) { };

    // TODO: simulate errors?
    return 0;
}

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

int pthread_setspecific( pthread_key_t key, const void *data ) noexcept
{
    __dios::FencedInterruptMask mask;

    tls( __dios_this_task() ).setKey( key, data );
    return 0;
}

void *pthread_getspecific( pthread_key_t key ) noexcept
{
    __dios::FencedInterruptMask mask;
    return tls( __dios_this_task() ).getKey( key );
}
