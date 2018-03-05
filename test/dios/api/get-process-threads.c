/* TAGS: min threads c */
#include <dios.h>
#include <assert.h>


void routine( void * x ){
    while( 1 );
}

int count( __dios_task * thr )
{
    return __vm_obj_size( thr ) / sizeof( __dios_task );
}

int isPresent( __dios_task id, __dios_task *thr )
{
    int c = count( thr );
    for ( int i = 0; i != c; i++ )
        if ( thr[ i ] == id )
            return 1;
    return 0;
}

int main()
{
    __dios_task thr1 = __dios_start_task( routine, NULL, 0 );
    __dios_task *t = __dios_this_process_tasks();
    assert( count(t) == 2 );
    assert( isPresent( __dios_this_task(), t ) );
    assert( isPresent( thr1, t ) );
    __vm_obj_free( t );

    __dios_task thr2 = __dios_start_task( routine, NULL, 0 );
    t = __dios_this_process_tasks();
    assert( count( t ) == 3 );
    assert( isPresent( __dios_this_task(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    __vm_obj_free( t );

    __dios_task thr3 = __dios_start_task( routine, NULL, 0 );
    t = __dios_this_process_tasks();
    assert( count( t ) == 4 );
    assert( isPresent( __dios_this_task(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    __vm_obj_free( t );

    __dios_task thr4 = __dios_start_task( routine, NULL, 0 );
    t = __dios_this_process_tasks();
    assert( count( t ) == 5 );
    assert( isPresent( __dios_this_task(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    assert( isPresent( thr4, t ) );
    __vm_obj_free( t );

    __dios_task thr5 = __dios_start_task( routine, NULL, 0 );
    t = __dios_this_process_tasks();
    assert( count( t ) == 6 );
    assert( isPresent( __dios_this_task(), t ) );
    assert( isPresent( thr1, t ) );
    assert( isPresent( thr2, t ) );
    assert( isPresent( thr3, t ) );
    assert( isPresent( thr4, t ) );
    assert( isPresent( thr5, t ) );
    __vm_obj_free( t );

    assert( 0 ); /* ERROR */
}
