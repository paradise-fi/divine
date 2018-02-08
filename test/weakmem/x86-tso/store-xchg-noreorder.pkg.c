/* TAGS: c tso ext */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
   figure 8-10
*/

#include <pthread.h>

volatile int x, y;
volatile int rx, ry;

// V: x0y1 CC_OPT: -DCHECK=0 ERR: ERR_x0y0
// V: x1y0 CC_OPT: -DCHECK=2 ERR: ERR_x1y0
// V: x1y1 CC_OPT: -DCHECK=3 ERR: ERR_x1y1

#ifndef CHECK
#define CHECK 1
#endif

#define REACH( X ) assert( !(X) )

void *t0( void *_ ) {
    int r = __sync_swap( &x, 1 );
    assert( r == 0 );
    y = 1;
    return NULL;
}

void *t1( void *_ ) {
    ry = y;
    rx = x;
    return NULL;
}

int main() {
    pthread_t t1t, t2t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );

    assert( rx == 0 || rx == 1 );
    assert( ry == 0 || ry == 1 );

    /* forbidden final states: */
    assert( !( rx == 0 && ry == 1 ) );
    /* reachable final states: */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 2, or 3" );
                break;
        case 0: REACH( rx == 0 && ry == 0 ); /* ERR_x0y0 */
                break;
        case 2: REACH( rx == 1 && ry == 0 ); /* ERR_x1y0 */
                break;
        case 3: REACH( rx == 1 && ry == 1 ); /* ERR_x1y1 */
                break;
    }
}
