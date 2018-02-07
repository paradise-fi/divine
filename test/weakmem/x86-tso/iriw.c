/* TAGS: ext c tso */
/* VERIFY_OPTS: --relaxed-memory tso */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
*/

#include <pthread.h>

volatile int x, y;
volatile int r2x, r2y, r3x, r3y;

void *t0( void *_ ) {
    x = 1;
    return NULL;
}

void *t1( void *_ ) {
    y = 1;
    return NULL;
}

void *t2( void *_ ) {
    r2x = x;
    r2y = y;
    return NULL;
}

void *t3( void *_ ) {
    r3y = y;
    r3x = x;
    return NULL;
}

int main() {
    pthread_t t1t, t2t, t3t;
    pthread_create( &t1t, NULL, &t1, NULL );
    pthread_create( &t2t, NULL, &t2, NULL );
    pthread_create( &t3t, NULL, &t3, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );
    pthread_join( t2t, NULL );
    pthread_join( t3t, NULL );

    assert( !(r2x == 1 && r2y == 0 && r3y == 1 && r3x == 0 ) ); // not reachable under x86-TSO
}
