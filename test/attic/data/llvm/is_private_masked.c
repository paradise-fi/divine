#include <divine.h>
#include <pthread.h>

int *x;

void *thr( void *_ ) {
    __divine_interrupt_mask();
    int *const local = __divine_malloc( sizeof( int ) );
    assert( __divine_is_private( local ) );
    assert( !__divine_is_private( &x ) );
    x = local;
    assert( !__divine_is_private( local ) );
    __divine_interrupt_unmask();
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
