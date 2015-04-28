/*
 * Alur-Taubenfeld
 * ===============
 *
 *  Discrete time model of Alur-Taubenfeld fast timing-based mutual exclusion
 *  algorithm.
 *
 *  *tags*: mutual exclusion, C99
 *
 * Description
 * -----------
 *
 *  One significant drawback of Fischer's real time mutual exclusion protocol
 *  (see *fischer.c*) is that a process must delay itself even in the absence of
 *  contention. This problem was addressed by Lamport, who constructed a fast
 *  timing-based algorithm (i.e., one in which a process performs *O(1)*
 *  shared-memory accesses and executes no delay statements in the absence of
 *  contention). This algorithm was later improved by Alur and Tabenfeld, who
 *  eliminated the assumption of apriori-known upper bound on the critical-section
 *  execution time.
 *
 *  When compiled with `-DBUG1`, delayed thread doesn't wait long enough for other
 *  threads that have passed the test `y=0`, to update the variable `y`.
 *  Timer can release the delayed thread "at the same time" as the thread that is
 *  about to update the variable `y`. The previously delayed thread can than be
 *  scheduled to continue to the critical section (skipping `goto Start`),
 *  followed be the other thread.
 *
 *  When compiled with `-DBUG2`, thread that is leaving the CS will set `y` to 0,
 *  even if its value isn't of his identificator. Now imagine that there are two
 *  other threads: `A`,`B`, where `A` is only in the beginning of the algorithm,
 *  while `B` have already skipped the line: `goto Start` (before `y` was set to 0).
 *  `B` can continue because `z` equals 0 (there is no thread in the CS at this
 *  moment) and enter the CS. But thread `A` has nothing in his way as well,
 *  since `y` equals 0.
 *  This error can really occur only if `NUM_OF_THREADS` is at least 3.
 *
 * Parameters
 * ----------
 *
 *  - `BUG1`: if defined than the algorithm is incorrect and violates the safety and the exclusion property
 *  - `BUG2`: another artifical bug, breaking the validity of the same set of properties as `BUG1`
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
 *         $ divine compile --llvm at.c
 *         $ divine verify -p assert at.bc -d
 *         $ divine verify -p safety at.bc -d
 *         $ divine verify -p progress at.bc --fair -d
 *         $ divine verify -p exclusion at.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-DBUG1" at.c -o at-bug1.bc
 *         $ divine verify -p assert at-bug1.bc -d
 *         $ divine verify -p exclusion at-bug1.bc -d
 *
 *         $ divine compile --llvm --cflags="-DBUG2" at.c -o at-bug2.bc
 *         $ divine verify -p assert at-bug2.bc -d
 *         $ divine verify -p exclusion at-bug2.bc -d
 *
 *  - customizing the number of threads:
 *
 *         $ divine compile --llvm --cflags="-DNUM_OF_THREADS=5" at.c
 *         $ divine verify -p assert at.bc -d
 *         $ divine verify -p exclusion at.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o at.exe at.c
 *       $ ./at.exe
 */

#ifndef NUM_OF_THREADS
#ifdef BUG2
#define NUM_OF_THREADS  3
#else
#define NUM_OF_THREADS  2
#endif
#endif

#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#ifdef __divine__    // verification
#include "divine.h"

LTL(progress, G(wait1 -> F(critical1in)) && G(wait2 -> F(critical2in)));
LTL(exclusion, G((critical1in -> (!critical2in W critical1out)) && (critical2in -> (!critical1in W critical2out))));

#else                // native execution
#define AP( x )

#endif

enum APs { wait1, critical1in, critical1out, wait2, critical2in, critical2out };

// Protocol constants - do not change!
#define OFF    255
#define DELTA  1

volatile int timer[ NUM_OF_THREADS ];
volatile intptr_t x, y = 0, z = 0;
volatile int finished = 0;

volatile int _critical = 0;



void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *fnc_thread( void *arg ) {
    intptr_t id = ( intptr_t ) arg;

    if ( id == 1 )
        AP( wait1 );
    if ( id == 2 )
        AP( wait2 );

  Start:
    x = id;

    do {
        // Promise that the variable _y_ and possibly also the variable _z_ will be
        // updated within the _delta_ time interval.
        timer[id-1] = DELTA;
    } while ( y != 0 );

    y = id;

    // Wait for the timer to run out. The only purpose of this line is to prevent
    // from interleaving with the thread managing the timer in the upcoming timer
    // setup. There is no delay in the original algorithm.
    while ( timer[id-1] );

    if ( x != id ) {
#ifdef BUG1
        timer[id-1] = 1 * DELTA;
#else // correct
        timer[id-1] = 2 * DELTA;
#endif
        while ( timer[id-1] );
        timer[id-1] = OFF; // Don't forget to disable timer in order to prevent from deadlock.

        if ( y != id )
            goto Start; // Restart, since there is another thread taking this path to the CS.

        while ( z ); // Wait for the other possible thread going throught the else branch.
    } else {
        z = 1;
        timer[id-1] = OFF;
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

    z = 0; // Leave the critical section.
#ifndef BUG2
    if ( y == id )
#endif
        y = 0;

    return NULL;
}

void *fnc_timer( void *arg ) {
    int executed;

    while ( !finished ) {
        do {
            executed = 1;
            for ( int j = 0; j < NUM_OF_THREADS; j++ )
                if ( timer[j] == 0 )
                    executed = 0;
            // If executed equals zero, it means that some work needs to be done before
            // the clock can advance, to satisfy time constraints.

        } while ( !executed );

        // Decrease value of every activated timer.
        for ( int j = 0; j < NUM_OF_THREADS; j++ )
            if ( timer[j] != OFF )
                --timer[j];
    }

    return NULL;
}

int main() {
    pthread_t threads[NUM_OF_THREADS + 1];

    pthread_create( &threads[0], 0, fnc_timer, NULL );
    for ( int i = 1; i <= NUM_OF_THREADS; i++ ) {
        timer[i-1] = OFF;
        pthread_create( &threads[i], 0, fnc_thread, ( void* )( intptr_t )( i ) );
    }

    for ( int i = 1; i <= NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    finished = 1;
    pthread_join( threads[0], NULL );

    return 0;
}
