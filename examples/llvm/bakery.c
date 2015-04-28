/*
 * Bakery
 * ======
 *
 *  This program implements the Lamport's bakery mutual exclusion algorithm.
 *
 *  *tags*: mutual exclusion, C99
 *
 * Description
 * -----------
 *
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
 *         $ divine compile --llvm bakery.c
 *         $ divine verify -p assert bakery.bc -d
 *         $ divine verify -p safety bakery.bc -d
 *         $ divine verify -p exclusion bakery.bc -d
 *         $ divine verify -p progress bakery.bc --fair -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-DBUG" bakery.c -o bakery-bug.bc
 *         $ divine verify -p assert bakery-bug.bc -d
 *         $ divine verify -p exclusion bakery-bug.bc -d
 *
 *  - customizing the number of threads:
 *
 *         $ divine compile --llvm --cflags="-DNUM_OF_THREADS=4" bakery.c
 *         $ divine verify -p exclusion bakery.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o bakery.exe bakery.c
 *       $ ./bakery.exe
 */

#ifndef NUM_OF_THREADS
#define NUM_OF_THREADS  2
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __divine__    // verification
#include "divine.h"

LTL(progress, G(wait0 -> F(critical0in)) && G(wait1 -> F(critical1in)));
LTL(exclusion, G((critical0in -> (!critical1in W critical0out)) && (critical1in -> (!critical0in W critical1out))));

#else                // native execution
#define AP( x )

#endif

enum APs { wait0, critical0in, critical0out, wait1, critical1in, critical1out };

volatile int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

volatile int choosing[NUM_OF_THREADS];
volatile int number[NUM_OF_THREADS];

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
        AP( critical0in );
    if ( id == 1 )
        AP( critical1in );
    critical();
    if ( id == 0 )
        AP( critical0out );
    if ( id == 1 )
        AP( critical1out );

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
