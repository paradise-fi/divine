/* TAGS: c tso */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
*/

#include <pthread.h>
#include <assert.h>

volatile int x;
volatile int rax, rbx;

// V: rax0rbx0x1 CC_OPT: -DCHECK=0
// V: rax0rbx0x2 CC_OPT: -DCHECK=1
// V: rax2rbx0x1 CC_OPT: -DCHECK=2
// V: rax0rbx1x2 CC_OPT: -DCHECK=3

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t0( void *_ ) {
    rax = x;
    x = 1;
    return NULL;
}

void *t1( void *_ ) {
    rbx = x;
    x = 2;
    return NULL;
}

int main() {
    pthread_t t1t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );

    assert( x == 1 || x == 2 );
    assert( rax == 0 || rax == 2 );
    assert( rbx == 0 || rbx == 1 );

    /* unreachable final states: */
    assert( !( rax == 2 && rbx == 1 ) );
    assert( !( rax == 2 && rbx == 0 && x == 2 ) );
    assert( !( rax == 0 && rbx == 1 && x == 1 ) );
    /* reachable final states: */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1, 2, or 3" );
                break;
        case 0: REACH( rax == 0 && rbx == 0 && x == 1 ); /* ERR_rax0rbx0x1 */
                break;
        case 1: REACH( rax == 0 && rbx == 0 && x == 2 ); /* ERR_rax0rbx0x2 */
                break;
        case 2: REACH( rax == 2 && rbx == 0 && x == 1 ); /* ERR_rax2rbx0x1 */
                break;
        case 3: REACH( rax == 0 && rbx == 1 && x == 2 ); /* ERR_rax0rbx1x2 */
                break;
    }
}
