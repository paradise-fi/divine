/* TAGS: c tso */
/* VERIFY_OPTS: --relaxed-memory tso */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
   the last figure (linux spinlock)
*/

#include <pthread.h>
#include <assert.h>

volatile int spinlock;
volatile int critical;

void *t( void *_ ) {
    int r;
  acquire:
    r = __sync_fetch_and_sub( &spinlock, 1 );
    if ( r >= 1 )
        goto critical;
  spin:
    if ( spinlock <= 0 )
        goto spin;
    else
        goto acquire;

  critical:
    __sync_fetch_and_add( &critical, 1 );
    __sync_fetch_and_sub( &critical, 1 );

  release:
    spinlock = 1;
    return NULL;
}

int main() {
    pthread_t t0t, t1t;
    pthread_create( &t0t, NULL, &t, NULL );
    pthread_create( &t1t, NULL, &t, NULL );

    assert( critical <= 1 );

    pthread_join( t1t, NULL );
    pthread_join( t0t, NULL );
}
