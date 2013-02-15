/*
 * Discrete time simulation of the Fischer's real time mutual exclusion protocol.
 *
 * Fischer’s algorithm is based on the known-delay model. The algorithm is quite simple.
 * A process _p_ waits until the “lock” is available, which is indicated by owner = 0.
 * If _p_ reads owner = 0, then it writes its process identifier to the variable owner,
 * and delays for 2*delta time units. If the value of variable owner is still _p_ after
 * the delay, then p enters its critical section.
 *
 * Two delta time units long delay ensures, that every other thread that have passed
 * the test for ownership availability, meanwhile also updated the variable owner to contain
 * his id. But one delta time unit is not enought to satisfy the safety property in this
 * simulation. Timer can release the delayed thread "at the same time" as the thread that is
 * about to update the variable owner. The previously delayed thread can than be scheduled
 * to continue to the critical section, followed be the other thread. Prove it to yourself
 * by compiling the program with macro BUG defined and running it throught DiVinE model
 * checker.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] fischer.c
 *  $ divine verify -p assert fischer.bc [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o fischer.exe fischer.c
 *  $ ./fischer.exe
 */

#define NUM_OF_THREADS  2

#include <pthread.h>

// For native execution.
#ifndef DIVINE
#include "assert.h"
#include "stdlib.h"

#define ap( x )
#endif

enum AP { wait0, critical0, wait1, critical1 };

#ifdef DIVINE
// LTL(progress, G(wait0 -> F(critical0)) && G(wait1 -> F(critical1)));
// LTL(exclusion, G(!(critical0 && critical1)));
#endif

// Protocol constants - do not change!
#define OFF    255
#define DELTA  1

int *timer;
int owner = 0;
int finished = 0;

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *fnc_thread( void *arg ) {
    int id = (int) arg;

    if ( id == 0 )
        ap( wait0 );
    if ( id == 1 )
        ap( wait1 );

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
        ap( critical0 );
    if ( id == 1 )
        ap( critical1 );
    critical();

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
        return 1;

    int i;
    pthread_t threads[NUM_OF_THREADS + 1];

    pthread_create( &threads[0], 0, fnc_timer, NULL );
    for ( i=1; i <= NUM_OF_THREADS; i++ ) {
        timer[i-1] = OFF;
        pthread_create( &threads[i], 0, fnc_thread, ( void* )( i-1 ) );
    }

    for ( i=1; i <= NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    finished = 1;
    pthread_join( threads[0], NULL );

    return 0;
}
