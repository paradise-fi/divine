/*
 * Cyclic scheduler
 * ================
 *
 *  Milner's cyclic scheduler.
 *
 *  *tags*: scheduler, C99
 *
 * Description
 * -----------
 *
 *  This program describes a (distributed) scheduler for `N` concurrent processes.
 *  The processes are scheduled in cyclic fashion so that the first process is
 *  reactivated after the Nth process has been activated.
 *
 *  The original idea is from R. Milner, A Calculus of Communicating Systems, 1980;
 *  this specific model is inspired by [1].
 *
 *  There is no buffer between scheduler and consumer, therefore scheduler shouldn't
 *  assign a new job to the consumer he manages if the consumer hasn't fully finished his
 *  previous job yet. In such a case, scheduler waits even if he is the current owner of
 *  the token. But when compiled with macro `BUG` defined, this wait is not implemented.
 *
 * ### References: ###
 *
 *  1. http://laser.cs.umass.edu/counterexamplesearch/
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the safety property
 *  - `NUM_OF_CONSUMERS`:  a number of processes among which jobs are distributed by the scheduling algorithm
 *  - `NUM_OF_JOBS`: a number of jobs to be scheduled
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm cyclic_scheduler.c
 *         $ divine verify -p assert cyclic_scheduler.bc -d
 *         $ divine verify -p safety cyclic_scheduler.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-DBUG" cyclic_scheduler.c -o cyclic_scheduler-bug.bc
 *         $ divine verify -p assert cyclic_scheduler-bug.bc -d
 *
 *  - customizing the parameters:
 *
 *         $ divine compile --llvm --cflags="-DNUM_OF_CONSUMERS=4 -DNUM_OF_JOBS=5" cyclic_scheduler.c
 *         $ divine verify -p assert cyclic_scheduler.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang -lpthread -o cyclic_scheduler.exe cyclic_scheduler.c
 *       $ ./cyclic_scheduler.exe
 */

// A number of processes among which jobs are distributed by the scheduling algorithm.
#ifndef NUM_OF_CONSUMERS
#define NUM_OF_CONSUMERS   2
#endif

// A number of jobs to be scheduled.
#ifndef NUM_OF_JOBS
#define NUM_OF_JOBS        3
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __divine__
#include "divine.h"
#endif

volatile int assigned = 0;
volatile int token = 0;

volatile int job[NUM_OF_CONSUMERS];
volatile int finished[NUM_OF_CONSUMERS];

void *consumer( void *arg ) {
    intptr_t id = (intptr_t) arg;
    finished[id] = 0;

    for (;;) {
        while ( job[id] == 0 );

        if ( job[id] == -1 )
            return NULL;

        // Do your work...
        // Scheduler shouldn't assign a new job yet, as the previous one is not fully finished.

        job[id] = 0; // Job is now finished.
        ++finished[id];
    }
}

void *scheduler( void *arg ) {
    intptr_t id = ( intptr_t ) arg;

    job[id] = 0;
    pthread_t my_cons;
    pthread_create( &my_cons, NULL, consumer, arg );

    for (;;) {
        while ( ( assigned < NUM_OF_JOBS ) && ( ( token % NUM_OF_CONSUMERS ) != id ) );
#ifndef BUG
        while ( job[id] != 0 );
#endif

        if ( assigned < NUM_OF_JOBS ) {
            job[id] = ++assigned;
            ++token; // Pass the token to the next scheduler.
        } else {
            job[id] = -1; // No more jobs, so tell the consumer to finish.
            pthread_join( my_cons, NULL );
            return NULL;
        }
    }
}

int main() {
    int i;
    pthread_t schedulers[NUM_OF_CONSUMERS];

    for ( i=0; i<NUM_OF_CONSUMERS; i++ ) {
        // Each consumer has its own sub-scheduler.
        // In order to synchronize all this sub-schedulers, technique of token passing is used.
        pthread_create( &schedulers[i], 0, scheduler, ( void* )( intptr_t )( i ) );
    }

    for ( i=0; i<NUM_OF_CONSUMERS; i++ ) {
        pthread_join( schedulers[i], NULL );
    }

    assert( assigned == NUM_OF_JOBS );
    for ( i=0; i<NUM_OF_CONSUMERS; i++ ) {
        assert( finished[i] == ( assigned / NUM_OF_CONSUMERS ) +
                               ( i < ( assigned % NUM_OF_CONSUMERS ) ? 1 : 0 ) );
    }


    return 0;
}
