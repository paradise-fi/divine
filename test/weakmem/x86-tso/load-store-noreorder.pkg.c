/* TAGS: c tso */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
   figure 8-2
*/

#include <pthread.h>
#include <assert.h>

volatile int x, y;
volatile int rx, ry;

// V: rx0ry0 CC_OPT: -DCHECK=0
// V: rx0ry1 CC_OPT: -DCHECK=1
// V: rx1ry0 CC_OPT: -DCHECK=2

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t0( void *_ ) {
    rx = x;
    y = 1;
    return NULL;
}

void *t1( void *_ ) {
    ry = y;
    x = 1;
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );

    assert( x == 1 );
    assert( y == 1 );
    assert( rx == 0 || rx == 1 );
    assert( ry == 0 || ry == 1 );

    /* forbidden final states: */
    assert( !( rx == 1 && ry == 1 ) );
    /* reachable final states: */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1 or 2" );
                break;
        case 0: REACH( rx == 0 && ry == 0 ); /* ERR_rx0ry0 */
                break;
        case 1: REACH( rx == 0 && ry == 1 ); /* ERR_rx0ry1 */
                break;
        case 2: REACH( rx == 1 && ry == 0 ); /* ERR_rx1ry0 */
                break;
    }
}
