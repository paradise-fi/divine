/* TAGS: c tso */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
*/

#include <pthread.h>

volatile int x;
volatile int rax, rbx;

// V: rax1rbx1x1 CC_OPT: -DCHECK=0 ERR: ERR_rax1rbx1x1
// V: rax1rbx2x1 CC_OPT: -DCHECK=1 ERR: ERR_rax1rbx2x1
// V: rax1rbx2x2 CC_OPT: -DCHECK=2 ERR: ERR_rax1rbx2x2
// V: rax2rbx2x2 CC_OPT: -DCHECK=3 ERR: ERR_rax2rbx2x2

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t0( void *_ ) {
    x = 1;
    rax = x;
    return NULL;
}

void *t1( void *_ ) {
    x = 2;
    rbx = x;
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );

    assert( x == 1 || x == 2 );
    assert( rax != 0 );
    assert( rbx != 0 );

    /* unreachable final states: */
    assert( !( rax == 1 && rbx == 1 && x == 2 ) );
    assert( !( rax == 2 && rbx == 1 ) );
    assert( !( rax == 2 && rbx == 2 && x == 1 ) );
    /* reachable final states: */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1, 2, or 3" );
                break;
        case 0: REACH( rax == 1 && rbx == 1 && x == 1 ); /* ERR_rax1rbx1x1 */
                break;
        case 1: REACH( rax == 1 && rbx == 2 && x == 1 ); /* ERR_rax1rbx2x1 */
                break;
        case 2: REACH( rax == 1 && rbx == 2 && x == 2 ); /* ERR_rax1rbx2x2 */
                break;
        case 3: REACH( rax == 2 && rbx == 2 && x == 2 ); /* ERR_rax2rbx2x2 */
                break;
    }
}
