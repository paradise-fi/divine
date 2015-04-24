/*
 * Fischer
 * =======
 *
 *  Discrete time simulation of the Fischer's real time mutual exclusion protocol.
 *
 *  *tags*: mutual exclusion, C99
 *
 * Description
 * -----------
 *
 *  Fischer's algorithm is based on the known-delay model. The algorithm is quite simple.
 *  A process `p` waits until the "lock" is available, which is indicated by `owner = 0`.
 *  If `p` reads `owner = 0`, then it writes its process identifier to the variable `owner`,
 *  and delays for `2*delta` time units. If the value of the variable `owner` is still
 *  `p` after the delay, then `p` enters its critical section.
 *
 *  Two delta time units long delay ensures, that every other thread that have passed
 *  the test for ownership availability, meanwhile also updated the variable `owner` to contain
 *  his id. But one `delta` time unit is not enought to satisfy the safety property in this
 *  simulation. Timer can release the delayed thread "at the same time" as the thread that is
 *  about to update the variable `owner`. The previously delayed thread can than be scheduled
 *  to continue to the critical section, followed be the other thread. Prove it to yourself
 *  by compiling the program with macro `BUG` defined and running it throught *DiVinE* model
 *  checker.
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
 *         $ divine compile --llvm fischer.c
 *         $ divine verify -p assert fischer.bc -d
 *         $ divine verify -p deadlock fischer.bc -d
 *         $ divine verify -p progress fischer.bc -f -d
 *         $ divine verify -p exclusion fischer.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-DBUG" fischer.c -o fischer-bug.bc
 *         $ divine verify -p assert fischer-bug.bc -d
 *         $ divine verify -p exclusion fischer-bug.bc -d
 *
 *  - customizing the number of threads:
 *
 *         $ divine compile --llvm --cflags="-DNUM_OF_THREADS=5" fischer.c
 *         $ divine verify -p assert fischer.bc -d
 *         $ divine verify -p exclusion fischer.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o fischer.exe fischer.c
 *       $ ./fischer.exe
 */

#ifndef NUM_OF_THREADS
#define NUM_OF_THREADS  2
#endif

#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#ifdef __divine__    // verification
#include "divine.h"

LTL(progress, G(wait0 -> F(critical0)) && G(wait1 -> F(critical1)));
LTL(exclusion, G((critical0in -> (!critical1in W critical0out)) && (critical1in -> (!critical0in W critical1out))));

#else                // native execution
#define AP( x )

#endif

enum APs { wait0, critical0in, critical0out, wait1, critical1in, critical1out };

// Protocol constants - do not change!
#define OFF    255
#define DELTA  1

int *timer;
intptr_t owner = 0;
int finished = 0;

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *fnc_thread( void *arg ) {
    intptr_t id = (intptr_t) arg;

    if ( id == 0 )
        AP( wait0 );
    if ( id == 1 )
        AP( wait1 );

    do {
        do {
            // Promise that the variable owner will be updated within _delta_
            // time interval.
            timer[id] = DELTA;
        } while ( owner != 0 ); // Wait until the "lock" is available.

        // The thread declares himself to be the "lock" owner.
        owner = id + 1;

        // Wait for the timer to run out. The only purpose of this line is to prevent
        // from interleaving with the thread managing the timer in the upcoming timer
        // set up.
        while ( timer[id] );

        // Wait for the other possible threads that have passed the test for ownership
        // availability to proceed with the update of the variable _owner_ as well.
#ifdef BUG
        timer[id] = 1 * DELTA;
#else // correct
        timer[id] = 2 * DELTA;
#endif
        while ( timer[id] );
        timer[id] = OFF; // Don't forget to disable timer in order to prevent from deadlock.

    } while ( owner != id + 1 );

    // The critical section goes here...
    if ( id == 0 )
        AP( critical0in );
    if ( id == 1 )
        AP( critical1in );
    critical();
    if ( id == 0 )
        AP( critical0out );
    if ( id == 1 )
        AP( critical1out );

    owner = 0; // Leave the critical section.

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
    timer = (int *) malloc( sizeof( int ) * NUM_OF_THREADS );
    if ( !timer )
        return 0;

    int i;
    pthread_t threads[NUM_OF_THREADS + 1];

    pthread_create( &threads[0], 0, fnc_timer, NULL );
    for ( i=1; i <= NUM_OF_THREADS; i++ ) {
        timer[i-1] = OFF;
        pthread_create( &threads[i], 0, fnc_thread, ( void* )( intptr_t )( i-1 ) );
    }

    for ( i=1; i <= NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    finished = 1;
    pthread_join( threads[0], NULL );

    return 0;
}
