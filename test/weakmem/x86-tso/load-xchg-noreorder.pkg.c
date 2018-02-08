/* TAGS: c tso min ext */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
   figure 8-9
*/

#include <pthread.h>

volatile int x, y;
volatile int rax, rbx, ray, rby;

// V: x0y1 CC_OPT: -DCHECK=1
// V: x1y0 CC_OPT: -DCHECK=2
// V: x1y1 CC_OPT: -DCHECK=3

#ifndef CHECK
#define CHECK 1
#endif

#define REACH( X ) assert( !(X) )

void *t0( void *_ ) {
    rax = __sync_swap( &x, 1 );
    ray = y;
    return NULL;
}

void *t1( void *_ ) {
    rby = __sync_swap( &y, 1 );
    rbx = x;
    return NULL;
}

int main() {
    pthread_t t1t, t2t;
    pthread_create( &t1t, NULL, &t1, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );

    assert( rax == 0 );
    assert( ray == 0 || ray == 1 );
    assert( rbx == 0 || rbx == 1 );
    assert( rby == 0 );

    /* forbidden final states: */
    assert( !( ray == 0 && rbx == 0 ) );
    /* reachable final states: */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 1, 2, or 3" );
                break;
        case 1: REACH( rbx == 0 && ray == 1 ); /* ERR_x0y1 */
                break;
        case 2: REACH( rbx == 1 && ray == 0 ); /* ERR_x1y0 */
                break;
        case 3: REACH( rbx == 1 && ray == 1 ); /* ERR_x1y1 */
                break;
    }
}
