/* TAGS: c tso ext */
/* VERIFY_OPTS: --relaxed-memory tso */
/* CC_OPTS: */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
   figure 8-6
*/

#include <pthread.h>

volatile int x, y;
volatile int rax, rbx, ry;

// V: rax0rbx0ry0 CC_OPT: -DCHECK=0
// V: rax0rbx0ry1 CC_OPT: -DCHECK=1
// V: rax0rbx1ry0 CC_OPT: -DCHECK=2
// V: rax0rbx1ry1 CC_OPT: -DCHECK=3
// V: rax1rbx0ry0 CC_OPT: -DCHECK=4
// V: rax1rbx1ry1 CC_OPT: -DCHECK=5

#ifndef CHECK
#define CHECK 0
#endif

#define REACH( X ) assert( !(X) )

void *t0( void *_ ) {
    x = 1;
    return NULL;
}

void *t1( void *_ ) {
    rax = x;
    y = 1;
    return NULL;
}

void *t2( void *_ ) {
    ry = y;
    rbx = x;
    return NULL;
}

int main() {
    pthread_t t1t, t2t;
    pthread_create( &t1t, NULL, &t1, NULL );
    pthread_create( &t2t, NULL, &t2, NULL );
    t0( NULL );
    pthread_join( t1t, NULL );
    pthread_join( t2t, NULL );

    assert( rax == 0 || rax == 1 );
    assert( rbx == 0 || rbx == 1 );
    assert( ry == 0 || ry == 1 );

    /* forbidden final states: */
    assert( !( rax == 1 && ry == 1 && rbx == 0 ) );
    /* reachable final states: */
    switch ( CHECK ) {
        default: assert( !"please define CHECK to be 0, 1, 2, 3, 4 or 5" );
                break;
        case 0: REACH( rax == 0 && rbx == 0 && ry == 0 ); /* ERR_rax0rbx0ry0 */
                break;
        case 1: REACH( rax == 0 && rbx == 0 && ry == 1 ); /* ERR_rax0rbx0ry1 */
                break;
        case 2: REACH( rax == 0 && rbx == 1 && ry == 0 ); /* ERR_rax0rbx1ry0 */
                break;
        case 3: REACH( rax == 0 && rbx == 1 && ry == 1 ); /* ERR_rax0rbx1ry1 */
                break;
        case 4: REACH( rax == 1 && rbx == 0 && ry == 0 ); /* ERR_rax1rbx0ry0 */
                break;
        case 5: REACH( rax == 1 && rbx == 1 && ry == 1 ); /* ERR_rax1rbx1ry1 */
                break;
    }
}
