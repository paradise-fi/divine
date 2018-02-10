/* TAGS: min c tso ext */
/* VERIFY_OPTS: --relaxed-memory tso:2 */

/* this is a regression test demonstrating error in an early version of lazy
 * x86-TSO in DIVINE */

#include <pthread.h>
#include <sys/divm.h>
#include <sys/lart.h>

#define REACH( X ) assert( !(X) )

volatile int x, y;

void *t0( void *_ ) {
    x = 1;
    y = 1;
    __sync_synchronize();
    return NULL;
}

void *t1( void *_ ) {
    x = 2;
    y = 2;
    __sync_synchronize();
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );

    REACH( x == 1 && y == 2 ); /* ERROR */
}

