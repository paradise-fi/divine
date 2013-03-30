/*
 * Dicrete time model of Alur-Taubenfeld fast timing-based mutual exclusion
 * algorithm.
 *
 * One significant drawback of Fischer's real time mutual exclusion protocol
 * (see fischer.c) is that a process must delay itself even in the absence of
 * contention. This problem was addressed by Lamport, who constructed a fast
 * timing-based algorithm (i.e., one in which a process performs O(1)
 * shared-memory accesses and executes no delay statements in the absence of
 * contention). This algorithm was later improved by Alur and Tabenfeld, who
 * eliminated the assumption of apriori-known upper bound on the critical-section
 * execution time.
 *
 * When compiled with -DBUG1, delayed thread doesn't wait long enough for other
 * threads that have passed the test _y=0_, to update the variable _y_.
 * Timer can release the delayed thread "at the same time" as the thread that is
 * about to update the variable _y_. The previously delayed thread can than be
 * scheduled to continue to the critical section (skipping _goto Start_),
 * followed be the other thread.
 *
 * When compiled with -DBUG2, thread that is leaving the CS will set _y_ to 0,
 * even if its value isn't of his identificator. Now imagine that there are two
 * other threads: A,B, where A is only in the beginning of the algorithm,
 * while B have already skipped the line: _goto Start_ (before _y_ was set to 0).
 * B can continue because _z_ equals 0 (there is no thread in the CS at this moment)
 * and enter the CS. But thread A has nothing in his way as well, since _y_
 * equals 0.
 * This error can really occur only if NUM_OF_THREADS is at least 3.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] at.c
 *  $ divine verify -p assert at.bc [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o at.exe at.c
 *  $ ./at.exe
 */

#ifdef BUG2
#define NUM_OF_THREADS  3
#else
#define NUM_OF_THREADS  2
#endif

#include <pthread.h>
#include <stdint.h>

// For native execution.
#ifndef DIVINE
#include "assert.h"
#include "stdlib.h"

#define ap( x )
#endif

enum AP { wait1, critical1, wait2, critical2 };

#ifdef DIVINE
LTL(progress, G(wait1 -> F(critical1)) && G(wait2 -> F(critical2)));
LTL(exclusion, G(!(critical1 && critical2)));
#endif

// Protocol constants - do not change!
#define OFF    255
#define DELTA  1

int *timer;
intptr_t x, y = 0, z = 0;
int finished = 0;

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

void *fnc_thread( void *arg ) {
    intptr_t id = ( intptr_t ) arg;

    if ( id == 1 )
        ap( wait1 );
    if ( id == 2 )
        ap( wait2 );

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
        ap( critical1 );
    if ( id == 2 )
        ap( critical2 );
    critical();

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
    timer = (int *) malloc( sizeof( int ) * NUM_OF_THREADS );
    if ( !timer )
        return 1;

    int i;
    pthread_t threads[NUM_OF_THREADS + 1];

    pthread_create( &threads[0], 0, fnc_timer, NULL );
    for ( i=1; i <= NUM_OF_THREADS; i++ ) {
        timer[i-1] = OFF;
        pthread_create( &threads[i], 0, fnc_thread, ( void* )( intptr_t )( i ) );
    }

    for ( i=1; i <= NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    finished = 1;
    pthread_join( threads[0], NULL );

    return 0;
}
