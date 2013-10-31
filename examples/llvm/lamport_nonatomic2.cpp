/*
 * Name
 * ====================
 *  Lamport non-atomic 2
 *
 * Category
 * ====================
 *  Mutual exclusion
 *
 * Short description
 * ====================
 *  Improved Lamport mutual exclusion protocol with nonatomic operations.
 *
 * Long description
 * ====================
 *  This program implements the Lamport's Three-Bit mutual exclusion algorithm and
 *  alongside with that simulates a computational environment in which execution
 *  of an operation on a shared variable has its duration and may be concurrent.
 *  For the Three-bit algorithm it is sufficient to consider single-writer nonatomic
 *  shared variables only. If a write of a communication variable is concurrent with
 *  another operation on that same variable, then the other operation must be a read.
 *  A read of a shared variable that is concurrent with a write of the same variable
 *  is allowed to return any value from the domain of the variable. A read that does
 *  not overlap such a write simply returns the variable's current value.
 *
 *  Non-determinism caused by non-atomicity Lamport reduced simply by using only
 *  one-bit long shared variables as well as skipping redundant writes (the new value
 *  is the same as the old one) wherever possible. But when compiled with `-DBUG`,
 *  redundant writes are present and may cause deadlock (althought in reality it is
 *  very unprobable).
 *
 *  The Three-Bit algorithm ensures exclusion using the *Wait-Die scheme*. Higher-priority
 *  thread (thread with lower id) waits for lower-priority thread, but in order to
 *  break a symmetry and hence prevent from deadlock, thread with lower priority
 *  dies if some higher-priority thread is also attempting to enter the critical
 *  section.
 *
 *  The Three-Bit algorithm is an improvement over the One-Bit algorithm (see
 *  lamport_nonatomic1.cpp) as it ensures starvation-free property. Lamport introduced
 *  a "renumbering" function, whereby processes contending in their entry sections
 *  are dynamically ordered so that any such process eventually has highest priority.
 *  To implement this, we used terminology from a well-known token passing technique.
 *
 * References:
 * --------------------
 *
 *  1. The mutual exclusion problem: partII -- statement and solutions.
 *
 *           @article{ Lamport:1986:MEP:5383.5385,
 *              author = {Lamport, Leslie},
 *              title = {The mutual exclusion problem: partII -- statement and solutions},
 *              journal = {J. ACM},
 *              issue_date = {April 1986},
 *              volume = {33},
 *              number = {2},
 *              month = apr,
 *              year = {1986},
 *              issn = {0004-5411},
 *              pages = {327--348},
 *              numpages = {22},
 *              publisher = {ACM},
 *              address = {New York, NY, USA},
 *            }
 *
 * Verification
 * ====================
 *     $ divine compile --llvm [--cflags=" < flags > "] lamport_nonatomic2.cpp
 *     $ divine verify -p assert lamport_nonatomic2.bc [-d]
 *
 * Execution
 * ====================
 *     $ clang++ [ < flags > ] -lpthread -o lamport_nonatomic2.exe lamport_nonatomic2.cpp
 *     $ ./lamport_nonatomic2.exe
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

#define __divine_choice( x ) ( rand() % ( x ) )

#endif

enum APs { wait0, critical0, wait1, critical1 };

int _critical = 0;

void critical() {
    assert( !_critical );
    _critical = 1;
    assert( _critical );
    _critical = 0;
}

struct NonAtomicBit {
    bool bit;
    int state;

    bool read() {
        if ( state != -1 )
            return ( __divine_choice( 2 ) == 1 );
        return bit;
    }

    void write( bool value ) {
        state = value;
        bit = ( state == 1 );
        state = -1;
    }

    NonAtomicBit () : bit( 0 ), state( -1 ) {}

};

typedef NonAtomicBit BitFlags[NUM_OF_THREADS];

BitFlags entering, active, token_ring;

int has_token() {
    int first, previous, token;
    first = previous = token = -1;

    for ( int i = 0; i < NUM_OF_THREADS; i++ ) {
        if ( entering[i].read() ) {
            if ( first == -1 )
                first = i;
            if ( token == -1 && previous != -1 &&
                 ( token_ring[previous].read() != token_ring[i].read() ) )
                token = i;
            previous = i;
        }
    }

    if ( first != -1 && ( token_ring[first].read() == token_ring[previous].read() ) )
        token = first;

    return token;
}

void *thread( void *arg ) {
    long id = reinterpret_cast< long >( arg );
    int i, k;

    if ( id == 0 )
        AP( wait0 );
    if ( id == 1 )
        AP( wait1 );

    entering[id].write( true );
  Reactivate:
    active[id].write( true );
  Wait:

    k = has_token(); // Note: this operation is not atomic,
                     //       but the algorithm can deal with it.
    assert( k > -1 );
    i = k;
    while ( i != id ) {
        if ( entering[i].read() ) {
#ifndef BUG
            if ( active[id].read() ) // Without this line a deadlock can occur (rare).
#endif
                // Die (in the terminology of the Wait-Die scheme).
                active[id].write( false );
            goto Wait; // Retrieve current token position again.
        }
        i++;
        i %= NUM_OF_THREADS;
    }

    if ( !active[id].read() )
        goto Reactivate;

    i = ( id + 1 ) % NUM_OF_THREADS;
    while ( i != k ) {
        if ( active[i].read() )
            goto Wait; // Retrieve current token position again.
        i++;
        i %= NUM_OF_THREADS;
    }

    // The critical section goes here...
    if ( id == 0 )
        AP( critical0 );
    if ( id == 1 )
        AP( critical1 );
    critical();

    // Pass the token to the next active thread.
    token_ring[id].write( !token_ring[id].read() );
    // Leave the critical section.
    active[id].write( false );
    entering[id].write( false );

    return NULL;
}

int main() {
    int i;
    pthread_t threads[NUM_OF_THREADS];

    for ( i = 0; i < NUM_OF_THREADS; i++ ) {
        pthread_create( &threads[i], 0, thread, reinterpret_cast< void* >( i ) );
    }

    for ( i = 0; i < NUM_OF_THREADS; i++ ) {
        pthread_join( threads[i], NULL );
    }
    return 0;
}
