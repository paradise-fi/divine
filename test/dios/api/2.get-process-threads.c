#include <dios.h>
#include <assert.h>


void routine( void * x ){
    while( 1 );
}

int count( _DiOS_TaskHandle* thr ) {
    return __vm_obj_size( thr ) / sizeof( _DiOS_TaskHandle );
}

int isPresent( _DiOS_TaskHandle id, _DiOS_TaskHandle* thr ) {
    int c = count( thr );
    for ( int i = 0; i != c; i++ )
        if ( thr[ i ] == id )
            return 1;
    return 0;
}

int main() {
    _DiOS_TaskHandle thr1 = __dios_start_task( routine, NULL, 0 );
    _DiOS_TaskHandle *t = __dios_get_process_tasks();
    assert( count(t) == 2 );
    assert( isPresent( __dios_get_task_handle(), t ) );
    assert( isPresent( thr1, t ) );
    __vm_obj_free( t );

    _DiOS_TaskHandle thr2 = __dios_start_task( routine, NULL, 0 );
    t = __dios_get_process_tasks();
    assert( count( t ) == 3 );
    assert( isPresent( __dios_get_task_handle(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    __vm_obj_free( t );

    _DiOS_TaskHandle thr3 = __dios_start_task( routine, NULL, 0 );
    t = __dios_get_process_tasks();
    assert( count( t ) == 4 );
    assert( isPresent( __dios_get_task_handle(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    __vm_obj_free( t );

    _DiOS_TaskHandle thr4 = __dios_start_task( routine, NULL, 0 );
    t = __dios_get_process_tasks();
    assert( count( t ) == 5 );
    assert( isPresent( __dios_get_task_handle(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    assert( isPresent( thr4, t ) );
    __vm_obj_free( t );

    _DiOS_TaskHandle thr5 = __dios_start_task( routine, NULL, 0 );
    t = __dios_get_process_tasks();
    assert( count( t ) == 6 );
    assert( isPresent( __dios_get_task_handle(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    assert( isPresent( thr4, t ) );
    assert( isPresent( thr5, t ) );
    __vm_obj_free( t );

    assert( 0 ); /* ERROR */
}
