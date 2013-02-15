/*
 * This program implements the Lamport's Fast mutual exclusion algorithm.
 *
 * This algorithm is optimized for a number of read/write operations.
 * In the absence of contention it takes only constant time (five writes
 * and two reads). If contention is present, time complexity is linear.
 *
 * When compiled with macro BUG defined, instructions for leaving the
 * critical section are swapped, which can lead to a violation of the
 * exclusion property. For this to happen, at least 3 threads have to
 * be present. Compile with -DBUG and verify it for the assertion property
 * using DiVinE model checker to see the scenario (as it is quite
 * hard-to-see bug).
 *
 * Source:
 *     @article{ lamport87fast,
 *               author = "Leslie Lamport",
 *               title = "A Fast Mutual Exclusion Algorithm",
 *               journal = "ACM Transactions on Computer Systems",
 *               volume = "5",
 *               number = "1",
 *               pages = "1--11",
 *               year = "1987",
 *               url = "citeseer.ist.psu.edu/lamport86fast.html" }
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] lamport.c
 *  $ divine verify -p assert lamport.bc [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o lamport.exe lamport.c
 *  $ ./lamport.exe
 */

#define NUM_OF_THREADS  3

#include <pthread.h>

// For native execution.
#ifndef DIVINE
#include "assert.h"

#define ap( x )
#endif

enum AP { wait1, critical1, wait2, critical2 };

#ifdef DIVINE
// LTL(progress, G(wait1 -> F(critical1)) && G(wait2 -> F(critical2)));
// LTL(exclusion, G(!(critical1 && critical2)));
#endif

char entering[NUM_OF_THREADS];
int x, y = 0;

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *thread( void *arg ) {
    int id = (int) arg;

    if ( id == 1 )
        ap( wait1 );
    if ( id == 2 )
        ap( wait2 );

  Start:
    entering[id-1] = 1;
    x = id;

    if ( y != 0 ) {
        entering[id-1] = 0;
        while ( y != 0 );
        goto Start;
    }

    y = id;

    if ( x != id ) {
        entering[id-1] = 0;
        for ( int i = 0; i < NUM_OF_THREADS; i++ )
            while ( entering[i] );
        if ( y != id ) {
            while ( y != 0 );
            goto Start;
        }
    }

    // The critical section goes here...
    if ( id == 1 )
        ap( critical1 );
    if ( id == 2 )
        ap( critical2 );
    critical();

    // Leave the critical section.
#ifdef BUG
    entering[id-1] = 0;
    y = 0;
#else // correct
    y = 0;
    entering[id-1] = 0;
#endif

    return NULL;
}

int main() {
    int i;
    pthread_t threads[NUM_OF_THREADS];

    for ( i = 0; i < NUM_OF_THREADS; i++ )
        entering[i] = 0;

    for ( i = 0; i < NUM_OF_THREADS; i++ )
        pthread_create( &threads[i], 0, thread, ( void* )( i + 1 ) );

    for ( i = 0; i < NUM_OF_THREADS; i++ )
        pthread_join( threads[i], NULL );

    return 0;
}
