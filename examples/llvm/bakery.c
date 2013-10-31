/*
 * Name
 * ====================
 *  Bakery
 *
 * Category
 * ====================
 *  Mutual exclusion
 *
 * Short description
 * ====================
 *  This program implements the Lamport's bakery mutual exclusion algorithm.
 *
 * Long description
 * ====================
 *  When compiled with macro `BUG` defined, usage of variable `choosing` is omitted.
 *  The necessity of variable `choosing` might not be obvious at the first glance.
 *  However, suppose the variable was removed and two processes computed the same
 *  `number`. If the higher-priority process (the process with lower id) was
 *  preempted before setting his `number`, the lower-priority process will see that
 *  the other process has a number of value zero, and enter the critical section.
 *  Later, the high-priority process will ignore equal `number` for lower-priority
 *  processes, and also enter the critical section. As a result, two processes can
 *  enter the critical section at the same time.
 *  The bakery algorithm then uses the `choosing` variable to ensure that the
 *  assignment to variable `number` acts like an atomic operation: process `i` will
 *  never see a number equal to zero for a process `j` that is going to pick the same
 *  number as `i`.
 *
 * Verification
 * ====================
 *     $ divine compile --llvm [--cflags=" < flags > "] bakery.c
 *     $ divine verify -p assert bakery.bc [-d]
 *
 * Execution
 * ====================
 *     $ clang [ < flags > ] -lpthread -o bakery.exe bakery.c
 *     $ ./bakery.exe
 *
 * Standard
 * ====================
 *  C99
 */

#define NUM_OF_THREADS  2

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __divine__    // verification
#include "divine.h"

LTL(progress, G(wait0 -> F(critical0)) && G(wait1 -> F(critical1)));
LTL(exclusion, G(!(critical0 && critical1)));

#else                // native execution
#define AP( x )

#endif

enum APs { wait0, critical0, wait1, critical1 };

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

int choosing[NUM_OF_THREADS];
int number[NUM_OF_THREADS];

void lock( intptr_t id ) {
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

void unlock( intptr_t id ) {
    number[id] = 0;
}

void *thread( void *arg ) {
    intptr_t id = ( intptr_t ) arg;

    if ( id == 0 )
        AP( wait0 );
    if ( id == 1 )
        AP( wait1 );

    lock(id);

    // The critical section goes here...
    if ( id == 0 )
        AP( critical0 );
    if ( id == 1 )
        AP( critical1 );
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
        pthread_create( &threads[i], 0, thread, ( void* )( intptr_t )( i ) );
    }

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }

    return 0;
}
