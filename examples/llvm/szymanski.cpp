/*
 * Name
 * ====================
 *  Szymanski
 *
 * Category
 * ====================
 *  Mutual exclusion
 *
 * Short description
 * ====================
 *  This program implements the Szymanski's mutual exclusion algorithm.
 *
 * Long description
 * ====================
 *  When compiled with `-DBUG`, the algorithm is incorrect, because the
 *  flag `waiting_for_others` and its usage is omitted. This leads to the scenario
 *  in which threads enter the "room", but don't wait for each other to update
 *  their flags.
 *  Thread with lower id (higher priority) can be interrupted by
 *  scheduler after entering the room, but just before updating his flag,
 *  while the other thread continues into the critical section, even thought he
 *  shouldn't as there is this thread with higher prority.
 *
 * Verification
 * ====================
 *     $ divine compile --llvm [--cflags=" < flags > "] szymanski.cpp
 *     $ divine verify -p assert szymanski.bc [-d]
 *
 * Execution
 * ====================
 *     $ clang++ [ < flags > ] -lpthread -o szymanski.exe szymanski.cpp
 *     $ ./szymanski.exe
 *
 * Standard
 * ====================
 *  C++98
 */

#define NUM_OF_THREADS  2

#include <pthread.h>
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

typedef enum { non_participant    = 1,
               outside            = 2,
               waiting_for_others = 4,
               at_doorway         = 8,
               at_critical        = 16 } Flag;
Flag *flags;

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

bool all( int set, int from, int to ) {
    for ( int i = from; i < to; i++ )
        if ( !( flags[i] & set ) )
            return false;
    return true;
}

bool any ( int set, int from, int to ) {
    for ( int i = from; i < to; i++ )
        if ( flags[i] & set )
            return true;
    return false;
}

void wait( bool (*cond) (int, int, int), int set, int from = 0, int to = NUM_OF_THREADS ) {
    while ( !cond( set, from, to ) );
}

void *thread( void *arg ) {
    long int id = (long int)arg;

    if ( id == 0 )
        AP( wait0 );
    if ( id == 1 )
        AP( wait1 );

    flags[id] = outside;  // standing outside waiting room
    wait( all, non_participant | outside | waiting_for_others );  // wait for open door
    flags[id] = at_doorway;  // standing in doorway

#ifndef BUG
    if ( any( outside, 0, NUM_OF_THREADS ) ) {  // another process is waiting to enter
        flags[id] = waiting_for_others;  // waiting for other processes to enter
        wait( any, at_critical );  // wait for a process to enter and close the door
    }
#endif

    flags[id] = at_critical; // the door is closed
    wait( all, non_participant | outside, 0, id );  // wait for everyone of lower ID to finish exit protocol

    // Critical section.
    if ( id == 0 )
        AP( critical0 );
    if ( id == 1 )
        AP( critical1 );
    critical();

    // Ensure that everyone in the waiting room has realized that the door is supposed to be closed.
    wait( all, non_participant | outside | at_critical, id+1, NUM_OF_THREADS );
    flags[id] = non_participant; // leave and reopen door if nobody is still in the waiting room
    return NULL;
}

int main() {
    flags = reinterpret_cast< Flag * >( malloc( sizeof( Flag ) * NUM_OF_THREADS ) );
    if ( !flags )
        return 1;

    int i;
    for ( i = 0; i < NUM_OF_THREADS; i++ )
        flags[i] = non_participant;

    pthread_t threads[NUM_OF_THREADS];

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_create( &threads[i], 0, thread, reinterpret_cast< void* >( i ) );
    }

    for ( i=0; i < NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }
    return 0;
}

