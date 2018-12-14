/* TAGS: min c tso ext */
/* VERIFY_OPTS: --relaxed-memory tso:3 */
/* CC_OPTS: */

/* this is a regression test demonstrating error in an early version of lazy
 * x86-TSO in DIVINE */

#include <pthread.h>
#include <assert.h>

volatile int x, y;

// V: y2x1 CC_OPT: -DCHECK=0
// V: y2x3 CC_OPT: -DCHECK=1
// V: y3x1 CC_OPT: -DCHECK=2
// V: y3x3 CC_OPT: -DCHECK=3

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t1( void *_ ) {
    x = 1;
    y = 1;
    y = 2;
    return NULL;
}

void *t2( void *_ ) {
    x = 3;
    y = 3;
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t2( NULL );
    pthread_join( t1t, NULL );

    /* unreachable final states */
    assert( y != 1 );
    /* reachable final states */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1, 2, or 3" );
                break;
        case 0: REACH( y == 2 && x == 1 ); /* ERR_y2x1 */
                break;
        case 1: REACH( y == 2 && x == 3 ); /* ERR_y2x3 */
                break;
        case 2: REACH( y == 3 && x == 1 ); /* ERR_y3x1 */
                break;
        case 3: REACH( y == 3 && x == 3 ); /* ERR_y3x3 */
                break;
    }
}
