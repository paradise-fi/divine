/* TAGS: min c tso ext */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
*/

#include <pthread.h>

volatile int x, y;
volatile int rx, ry;
volatile int lrx, lry;

// V: x0y0 CC_OPT: -DCHECK=0
// V: x0y1 CC_OPT: -DCHECK=1
// V: x1y0 CC_OPT: -DCHECK=2
// V: x1y1 CC_OPT: -DCHECK=3

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t1( void *_ ) {
    x = 1;
    ry = y;
    lrx = x;
    return NULL;
}

void *t2( void *_ ) {
    y = 1;
    rx = x;
    lry = y;
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t2( NULL );
    pthread_join( t1t, NULL );

    assert( lrx == 1 );
    assert( lry == 1 );
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1, 2, or 3" );
                break;
        case 0: REACH( rx == 0 && ry == 0 ); /* ERR_x0y0 */
                break;
        case 1: REACH( rx == 0 && ry == 1 ); /* ERR_x0y1 */
                break;
        case 2: REACH( rx == 1 && ry == 0 ); /* ERR_x1y0 */
                break;
        case 3: REACH( rx == 1 && ry == 1 ); /* ERR_x1y1 */
                break;
    }
}
