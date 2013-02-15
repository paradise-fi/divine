/*
 * This program implements the Lamport's bakery mutual exclusion algorithm.
 *
 * When compiled with macro BUG defined, usage of variable _choosing_ is omitted.
 * The necessity of variable _choosing_ might not be obvious at the first glance.
 * However, suppose the variable was removed and two processes computed the same
 * _number_. If the higher-priority process (the process with lower id) was
 * preempted before setting his _number_, the lower-priority process will see that
 * the other process has a number of zero, and enter the critical section.
 * Later, the high-priority process will ignore equal _number_ for lower-priority
 * processes, and also enter the critical section. As a result, two processes can
 * enter the critical section at the same time.
 * The bakery algorithm then uses the _choosing_ variable to ensure that the
 * assignment to variable _number_ acts like an atomic operation: process _i_ will
 * never see a number equal to zero for a process _j_ that is going to pick the same
 * number as _i_.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] bakery.c
 *  $ divine verify -p assert bakery.bc [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o bakery.exe bakery.c
 *  $ ./bakery.exe
 */

#define NUM_OF_THREADS  2

#include <pthread.h>

// For native execution.
#ifndef DIVINE
#include "stdlib.h"
#include "assert.h"

#define ap( x )
#endif

enum AP { wait0, critical0, wait1, critical1 };

#ifdef DIVINE
// LTL(progress, G(wait0 -> F(critical0)) && G(wait1 -> F(critical1)));
// LTL(exclusion, G(!(critical0 && critical1)));
#endif

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

int choosing[NUM_OF_THREADS];
int number[NUM_OF_THREADS];

void lock( int id ) {
    int max = 0;
    choosing[id] = 1;
    for ( int j = 0; j < NUM_OF_THREADS; j++)
        if ( max < number[j] )
            max = number[j];
    number[id] = max + 1;
    choosing[id] = 0;

    for ( int j = 0; j < NUM_OF_THREADS; j++) {

#ifndef BUG
        // Wait until thread j chooses its number.
        while ( choosing[j] );
#endif

       // Wait until all threads with smaller numbers or with the same
       // number, but with higher priority (lower id), finish their work.
        while ( number[j] != 0  &&
                ( number[j] < number[id] || ( number[j] == number[id] && j < id ) ) );
    }
}

void unlock( int id ) {
    number[id] = 0;
}

void *thread( void *arg ) {
    int id = (int) arg;

    if ( id == 0 )
        ap( wait0 );
    if ( id == 1 )
        ap( wait1 );

    lock(id);

    // The critical section goes here...
    if ( id == 0 )
        ap( critical0 );
    if ( id == 1 )
        ap( critical1 );
    critical();

    unlock(id);

    return NULL;
}

int main() {
    int i;
    pthread_t threads[NUM_OF_THREADS];

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        choosing[i] = 0;
        number[i] = 0;
        pthread_create( &threads[i], 0, thread, ( void* )( i ) );
    }

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    return 0;
}
