/*
 * Lamport
 * =======
 *
 *  This program implements the Lamport's Fast mutual exclusion algorithm.
 *
 *  *tags*: mutual exclusion, C99
 *
 * Description
 * -----------
 *
 *  This algorithm is optimized for a number of read/write operations.
 *  In the absence of contention it takes only constant time (five writes
 *  and two reads). If contention is present, time complexity is linear.
 *
 *  When compiled with macro `BUG` defined, instructions for leaving the
 *  critical section are swapped, which can lead to a violation of the
 *  exclusion property. For this to happen, at least 3 threads have to
 *  be present. Compile with `-DBUG` and verify it for the assertion property
 *  using *DiVinE* model checker to see the scenario (as it is quite
 *  hard-to-see bug).
 *
 * ### References: ###
 *
 *  1. A Fast Mutual Exclusion Algorithm.
 *
 *           @article{ lamport87fast,
 *                     author = "Leslie Lamport",
 *                     title = "A Fast Mutual Exclusion Algorithm",
 *                     journal = "ACM Transactions on Computer Systems",
 *                     volume = "5",
 *                     number = "1",
 *                     pages = "1--11",
 *                     year = "1987",
 *                     url = "citeseer.ist.psu.edu/lamport86fast.html" }
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the safety and the exclusion property
 *  - `NUM_OF_THREADS`: a number of threads requesting to enter the critical section
 *
 * LTL Properties
 * --------------
 *
 *  - `progress`: if a thread requests to enter a critical section, it will eventually be allowed to do so
 *  - `exclusion`: critical section can only be executed by one process at a time
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm lamport.c
 *         $ divine verify -p assert lamport.bc -d
 *         $ divine verify -p safety lamport.bc -d
 *         $ divine verify -p progress lamport.bc --fair -d
 *         $ divine verify -p exclusion lamport.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-DBUG" lamport.c -o lamport-bug.bc
 *         $ divine verify -p assert lamport-bug.bc -d
 *         $ divine verify -p exclusion lamport-bug.bc -d
 *
 *  - customizing the number of threads:
 *
 *         $ divine compile --llvm --cflags="-DNUM_OF_THREADS=5" lamport.c
 *         $ divine verify -p assert lamport.bc -d
 *         $ divine verify -p exclusion lamport.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o lamport.exe lamport.c
 *       $ ./lamport.exe
 */

#ifndef NUM_OF_THREADS
#define NUM_OF_THREADS  3
#endif

#include <pthread.h>
#include <stdint.h>
#include <assert.h>

#ifdef __divine__    // verification
#include "divine.h"

LTL(progress, G(wait1 -> F(critical1in)) && G(wait2 -> F(critical2in)));
LTL(exclusion, G((critical1in -> (!critical2in W critical1out)) && (critical2in -> (!critical1in W critical2out))));

#else                // native execution
#define AP( x )

#endif

enum APs { wait1, critical1in, critical1out, wait2, critical2in, critical2out };

volatile char entering[NUM_OF_THREADS];
volatile intptr_t x, y = 0;

volatile int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *thread( void *arg ) {
    intptr_t id = ( intptr_t ) arg;

    if ( id == 1 )
        AP( wait1 );
    if ( id == 2 )
        AP( wait2 );

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
        AP( critical1in );
    if ( id == 2 )
        AP( critical2in );
    critical();
    if ( id == 1 )
        AP( critical1out );
    if ( id == 2 )
        AP( critical2out );

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
    pthread_t threads[NUM_OF_THREADS];

    for ( int i = 0; i < NUM_OF_THREADS; i++ )
        entering[i] = 0;

    for ( int i = 0; i < NUM_OF_THREADS; i++ )
        pthread_create( &threads[i], 0, thread, ( void* )( intptr_t )( i + 1 ) );

    for ( int i = 0; i < NUM_OF_THREADS; i++ )
        pthread_join( threads[i], NULL );

    return 0;
}
