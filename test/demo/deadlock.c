/* ERRSPEC: pthread_mutex_lock */

#include "demo.h"

pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER,
                mtx2 = PTHREAD_MUTEX_INITIALIZER;

int x;

void *thr1( void *_ ) {
    print( "1: locking mutex 2" );
    pthread_mutex_lock( &mtx2 );
    print( "1: locking mutex 1" );
    pthread_mutex_lock( &mtx1 );
    ++x;
    print( "1: unlocking mutex 1" );
    pthread_mutex_unlock( &mtx1 );
    print( "1: unlocking mutex 2" );
    pthread_mutex_unlock( &mtx2 );
    return _;
}

int main() {
    pthread_t t1;

    pthread_create( &t1, NULL, thr1, NULL );

    print( "0: locking mutex 1" );
    pthread_mutex_lock( &mtx1 );
    print( "0: locking mutex 2" );
    pthread_mutex_lock( &mtx2 );
    ++x;
    pthread_mutex_unlock( &mtx2 );
    print( "0: unlocking mutex 2" );
    pthread_mutex_unlock( &mtx1 );
    print( "0: unlocking mutex 1" );

    pthread_join( t1, 0 );

    assert( x == 2 );
}
