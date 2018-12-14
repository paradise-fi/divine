/* TAGS: min c tso ext */
/* VERIFY_OPTS: */
/* CC_OPTS: */

/* this is a regression test demonstrating error in an early version of lazy
 * x86-TSO in DIVINE */

#include <pthread.h>
#include <assert.h>

volatile int x, y, z;

// V: z2y2x2 CC_OPT: -DCHECK=0 V_OPT: --relaxed-memory tso:5
// V: z2y3x2 CC_OPT: -DCHECK=1 V_OPT: --relaxed-memory tso:5
// V: z2y2x3 CC_OPT: -DCHECK=2 V_OPT: --relaxed-memory tso:5
// V: z2y3x3 CC_OPT: -DCHECK=3 V_OPT: --relaxed-memory tso:5
// V: z3y2x2 CC_OPT: -DCHECK=4 V_OPT: --relaxed-memory tso:5
// V: z3y3x2 CC_OPT: -DCHECK=5 V_OPT: --relaxed-memory tso:5
// V: z3y2x3 CC_OPT: -DCHECK=6 V_OPT: --relaxed-memory tso:5
// V: z3y3x3 CC_OPT: -DCHECK=7 V_OPT: --relaxed-memory tso:5

// V: z2y2x2.short CC_OPT: -DCHECK=0 V_OPT: --relaxed-memory tso:2
// V: z2y3x2.short CC_OPT: -DCHECK=1 V_OPT: --relaxed-memory tso:2
// V: z2y2x3.short CC_OPT: -DCHECK=2 V_OPT: --relaxed-memory tso:2
// V: z2y3x3.short CC_OPT: -DCHECK=3 V_OPT: --relaxed-memory tso:2
// V: z3y2x2.short CC_OPT: -DCHECK=4 V_OPT: --relaxed-memory tso:2
// V: z3y3x2.short CC_OPT: -DCHECK=5 V_OPT: --relaxed-memory tso:2
// V: z3y2x3.short CC_OPT: -DCHECK=6 V_OPT: --relaxed-memory tso:2
// V: z3y3x3.short CC_OPT: -DCHECK=7 V_OPT: --relaxed-memory tso:2

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t1( void *_ ) {
    x = 1;
    y = 1;
    x = 2;
    y = 2;
    z = 2;
    return NULL;
}

void *t2( void *_ ) {
    x = 3;
    y = 3;
    z = 3;
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t2( NULL );
    pthread_join( t1t, NULL );

    /* reachable final states */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1, 2, 3, 4, 5, 6 or 7" );
                break;
        case 0: REACH( z == 2 && y == 2 && x == 2 ); /* ERR_z2y2x2 */
                break;
        case 1: REACH( z == 2 && y == 3 && x == 2 ); /* ERR_z2y3x2 */
                break;
        case 2: REACH( z == 2 && y == 2 && x == 3 ); /* ERR_z2y2x3 */
                break;
        case 3: REACH( z == 2 && y == 3 && x == 3 ); /* ERR_z2y3x3 */
                break;
        case 4: REACH( z == 3 && y == 2 && x == 2 ); /* ERR_z3y2x2 */
                break;
        case 5: REACH( z == 3 && y == 3 && x == 2 ); /* ERR_z3y3x2 */
                break;
        case 6: REACH( z == 3 && y == 2 && x == 3 ); /* ERR_z3y2x3 */
                break;
        case 7: REACH( z == 3 && y == 3 && x == 3 ); /* ERR_z3y3x3 */
                break;
    }
}
