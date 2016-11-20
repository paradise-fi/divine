#include <dios.h>
#include <assert.h>


void routine( void * x ){
    while( 1 );
}

int count( _DiOS_ThreadId* thr ) {
    return __vm_obj_size( thr ) / sizeof( _DiOS_ThreadId );
}

int isPresent( _DiOS_ThreadId id, _DiOS_ThreadId* thr ) {
    int c = count( thr );
    for ( int i = 0; i != c; i++ )
        if ( thr[ i ] == id )
            return 1;
    return 0;
}

int main() {
    _DiOS_ThreadId thr1 = __dios_start_thread( routine, NULL, _DiOS_TLS_Reserved );
    _DiOS_ThreadId *t = __dios_get_process_threads();
    assert( count(t) == 2 );
    assert( isPresent( __dios_get_thread_id(), t ) );
    assert( isPresent( thr1, t ) );
    __vm_obj_free( t );

    _DiOS_ThreadId thr2 = __dios_start_thread( routine, NULL, _DiOS_TLS_Reserved );
    t = __dios_get_process_threads();
    assert( count( t ) == 3 );
    assert( isPresent( __dios_get_thread_id(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    __vm_obj_free( t );

    _DiOS_ThreadId thr3 = __dios_start_thread( routine, NULL, _DiOS_TLS_Reserved );
    t = __dios_get_process_threads();
    assert( count( t ) == 4 );
    assert( isPresent( __dios_get_thread_id(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    __vm_obj_free( t );

    _DiOS_ThreadId thr4 = __dios_start_thread( routine, NULL, _DiOS_TLS_Reserved );
    t = __dios_get_process_threads();
    assert( count( t ) == 5 );
    assert( isPresent( __dios_get_thread_id(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    assert( isPresent( thr4, t ) );
    __vm_obj_free( t );

    _DiOS_ThreadId thr5 = __dios_start_thread( routine, NULL, _DiOS_TLS_Reserved );
    t = __dios_get_process_threads();
    assert( count( t ) == 6 );
    assert( isPresent( __dios_get_thread_id(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    assert( isPresent( thr4, t ) );
    assert( isPresent( thr5, t ) );
    __vm_obj_free( t );

    assert( 0 ); /* ERROR */
}
