/* TAGS: c tso */
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

// V: rx1ry0x1 CC_OPT: -DCHECK=0
// V: rx1ry0x2 CC_OPT: -DCHECK=1
// V: rx1ry2x1 CC_OPT: -DCHECK=2
// V: rx1ry2x2 CC_OPT: -DCHECK=3
// V: rx2ry2x2 CC_OPT: -DCHECK=4
// V: rx2ry2x1 CC_OPT: -DCHECK=5

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t0( void *_ ) {
    x = 1;
    rx = x;
    ry = y;
    return NULL;
}

void *t1( void *_ ) {
    y = 2;
    x = 2;
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );

    assert( y == 2 );
    assert( x == 1 || x == 2 );
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1, 2, 3, 4 or 5" );
                break;
        case 0: REACH( rx == 1 && ry == 0 && x == 1 ); /* ERR_rx1ry0x1 */
                break;
        case 1: REACH( rx == 1 && ry == 0 && x == 2 ); /* ERR_rx1ry0x2 */
                break;
        case 2: REACH( rx == 1 && ry == 2 && x == 1 ); /* ERR_rx1ry2x1 */
                break;
        case 3: REACH( rx == 1 && ry == 2 && x == 2 ); /* ERR_rx1ry2x2 */
                break;
        case 4: REACH( rx == 2 && ry == 2 && x == 2 ); /* ERR_rx2ry2x2 */
                break;
        case 5: assert( !( rx == 2 && ry == 2 && x == 1 ) ); /* unreachable */
                break;
    }
}
