#include <divine.h>
#include <pthread.h>

int *x;

void *thr( void *_ ) {
    int *const local = __divine_malloc( sizeof( int ) );
    assert( __divine_is_private( local ) );
    assert( !__divine_is_private( &x ) );
    for ( int i = 0; i < 2; ++i ) { }
    x = local;
    assert( !__divine_is_private( local ) );
    return 0;
}

int main() {
    pthread_t t;
    pthread_create( &t, 0, thr, 0 );
    pthread_join( t, 0 );
}

/* divine-test
holds: true
*/
