/*
 * This program implements the Anderson's queue lock mutual exclusion algorithm.
 *
 * A ring-queue like mechanism is used to order the processes requesting to enter
 * the critical section. The association of slots to processes is not fixed but
 * the ordering of the slots comprising the queue of waiting processes is.
 * A process enqueues itself by selecting the current queue's tail index to be
 * his position (variable my_place), and moving the tail to the next position.
 * This has to be done atomically in order to prevent from concurrency, therefore
 * the algorithm uses fetch_and_add instruction.
 * But when compiled with macro BUG defined, fetch_and_add is not used,
 * instead a set of simple instructions implements operation enqueue as a non-atomic
 * operation, hence the algorithm violates the safety property.
 *
 * Source: Anderson (The performance of spin lock alternatives for
 * shared-memory synchronization), model according to
 *    @article{ 966040,
 *              author = {James H. Anderson and Yong-Jik Kim and Ted Herman},
 *              title = {Shared-memory mutual exclusion: major research trends since 1986},
 *              journal = {Distributed Computing},
 *              volume = {16},
 *              number = {2-3},
 *              year = {2003},
 *              issn = {0178-2770},
 *              pages = {75--110},
 *              doi = {http://dx.doi.org/10.1007/s00446-003-0088-6},
 *              publisher = {Springer-Verlag},
 *            }
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] anderson.c
 *  $ divine verify -p assert anderson.bc [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o anderson.exe anderson.c
 *  $ ./anderson.exe
 */

#define NUM_OF_THREADS  2

#include <pthread.h>

// For native execution.
#ifndef DIVINE
#include "stdlib.h"
#include "assert.h"

#define ap( x )
#endif

enum AP { wait1, critical1, wait2, critical2 };

#ifdef DIVINE
// LTL(progress, G(wait1 -> F(critical1)) && G(wait2 -> F(critical2)));
// LTL(exclusion, G(!(critical1 && critical2)));
#endif

int *slot;
int next = 0;

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

#if defined( DIVINE ) && !defined( BUG )
int fetch_and_add ( int *ptr, int value ) {
    __divine_interrupt_mask();
    int tmp = *ptr;
    *ptr += value;
    return tmp;
}
#endif

void *thread( void *arg ) {
    int id = ( int )arg;

#ifdef BUG                 // incorrect
    int my_place = next++;
#elif defined( DIVINE )    // correct, for verification
    int my_place = fetch_and_add( &next, 1 );
#else                      // correct, native execution
    int my_place = __sync_fetch_and_add( &next, 1 );
#endif

    if ( my_place == NUM_OF_THREADS - 1 )
#ifdef BUG                 // incorrect
        next -= NUM_OF_THREADS;
#elif defined( DIVINE )    // correct, for verification
        fetch_and_add( &next, -NUM_OF_THREADS );
#else                      // correct, native execution
        __sync_fetch_and_add( &next, -NUM_OF_THREADS );
#endif
    else
        my_place %= NUM_OF_THREADS;

    if ( id == 1 )
        ap( wait1 );
    if ( id == 2 )
        ap( wait2 );

    while ( !slot[my_place] ); // wait
    slot[my_place] = 0;

    // The critical section goes here...
    if ( id == 1 )
        ap( critical1 );
    if ( id == 2 )
        ap( critical2 );
    critical();

    slot[( my_place + 1 ) % NUM_OF_THREADS] = 1;

    return NULL;
}

int main() {
    slot = (int *) malloc( sizeof( int ) * NUM_OF_THREADS );
    if ( !slot )
        return 1;

    if ( NUM_OF_THREADS > 0 )
        slot[0] = 1;

    int i;
    for ( i = 1; i < NUM_OF_THREADS; i++ )
        slot[i] = 0;

    pthread_t threads[NUM_OF_THREADS];

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_create( &threads[i], 0, thread, ( void * )( i+1 ) ) ;
    }

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    return 0;
}
