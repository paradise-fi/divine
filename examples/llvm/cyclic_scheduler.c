/*
 * This program describes a (distributed) scheduler for N concurrent processes.
 * The processes are scheduled in cyclic fashion so that the first process is
 * reactivated after the Nth process has been activated.
 *
 * The original idea is from R. Milner, A Calculus of Communicating Systems, 1980;
 * this specific model is inspired by http://laser.cs.umass.edu/counterexamplesearch/
 *
 * There is no buffer between scheduler and consumer, therefore scheduler shouldn't
 * assign a new job to the consumer he manages, if the consumer hasn't fully finished his
 * previous job yet. In such a case, scheduler waits even if he is the current owner of
 * the token. But when compiled with macro BUG defined, this wait is not implemented.
 *
 * Verify with:
 *  $ divine compile --llvm [--cflags=" < flags > "] cyclic_scheduler.c
 *  $ divine verify -p assert cyclic_scheduler.bc [-d]
 * Execute with:
 *  $ clang [ < flags > ] -lpthread -o cyclic_scheduler.exe cyclic_scheduler.c
 *  $ ./cyclic_scheduler.exe
 */

#define NUM_OF_CONSUMERS   2
#define NUM_OF_JOBS        3

#include <pthread.h>

// For native execution.
#ifndef DIVINE
#include "stdlib.h"
#include "assert.h"
#endif

int assigned = 0;
int token = 0;

int job[NUM_OF_CONSUMERS];
int finished[NUM_OF_CONSUMERS];

void *consumer( void *arg ) {
    int id = (int) arg;
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
    int id = (int) arg;

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
        pthread_create( &schedulers[i], 0, scheduler, ( void* )( i ) );
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
