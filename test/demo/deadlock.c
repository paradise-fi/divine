/* TAGS: min c */
/* EXPECT: --result error --symbol pthread_mutex_lock */

#include "demo.h"

pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER,
                mtx2 = PTHREAD_MUTEX_INITIALIZER;

int x;

void *thr1( void *_ ) {
    puts( "1: locking mutex 2" );
    pthread_mutex_lock( &mtx2 );
    puts( "1: locking mutex 1" );
    pthread_mutex_lock( &mtx1 );
    ++x;
    puts( "1: unlocking mutex 1" );
    pthread_mutex_unlock( &mtx1 );
    puts( "1: unlocking mutex 2" );
    pthread_mutex_unlock( &mtx2 );
    return _;
}

int main() {
    pthread_t t1;

    pthread_create( &t1, NULL, thr1, NULL );

    puts( "0: locking mutex 1" );
    pthread_mutex_lock( &mtx1 );
    puts( "0: locking mutex 2" );
    pthread_mutex_lock( &mtx2 );
    ++x;
    pthread_mutex_unlock( &mtx2 );
    puts( "0: unlocking mutex 2" );
    pthread_mutex_unlock( &mtx1 );
    puts( "0: unlocking mutex 1" );

    pthread_join( t1, 0 );

    assert( x == 2 );
}
