/* divine-cflags: -std=c++11 */
/*
 * Leader election in an Uni-circle (Dolev, Klawe, Rodeh)
 * ======================================================
 *
 *  A simulation of a O(n*log(n)) unidirectional distributed algorithm for extrema finding
 *  in a circle.
 *
 *  *tags*: leader election, C++11
 *
 * Description:
 * ------------
 *
 *  This program is a simulation of a O(n*log(n)) unidirectional distributed algorithm
 *  for extrema finding in a circle, proposed by Dolev, Klawe and Rodeh [1]
 *  (this algorithm is referred to as "Algorithm B" in the paper).
 *
 *  Given `n` processes in a ring, communicating only with message passing to its right
 *  neighbour, the unidirectional circular extrema-finding problem (more likely known as a leader
 *  election problem) is to select a maximum (or minimum) process.
 *  Each process has a unique value, called ID, in a set with a total order. These values
 *  may be transmitted and compared. All processes are identical (except for their IDs) and `n`,
 *  the number of processes, is not initially known.
 *
 *  This algorithm is based on the Basic algorithm (also presented in [1]), but by incorporating
 *  several improvements into it, the authors were able to bound the number of messages by
 *  1.5*n*log(n) + O(n).
 *  All of this improvements focus on reduction of the `M2` message passes. It can than happen,
 *  that a process receives two `M1` messages in a row, instead of the alternation between
 *  `M1` and `M2` messages as it is in the Basic algorithm. This reductions are based on the facts
 *  that in some cases local or even global maximality can be disproved with fewer (`M2`) messages
 *  sent than in usual cases. And so when a process receives two `M1` messages consecutively,
 *  it should respond with changing it's state to *Passive*. But be aware that second `M1` message
 *  should be treated as if the process was already in the *Passive* mode before reception, thus
 *  the message should be forwarded -- however this is omitted when the program is compiled with
 *  macro `BUG` defined (this can lead to a deadlock state).
 *
 * ### References: ###
 *
 *  1. An O(n log n) Unidirectional Distributed Algorithm for Extrema Finding in a Circle.
 *
 *           @article{ dolev:an,
 *                     author = "Danny Dolev and Maria M. Klawe and Michael Rodeh",
 *                     title = "An O(n log n) Unidirectional Distributed Algorithm for Extrema
 *                              Finding in a Circle",
 *                     journal = "J. Algorithms",
 *                     pages = {245-260},
 *                     year = {1982},
 *                   }
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the deadlock-free property
 *  - `NUM_OF_PROCESSES`: a number of processes in the ring
 *  - `PIDS`: a distribution of identification numbers among processes to verify;
 *            use the syntax for static array declaration in C/C++, i.e. `{ x, y, z, ...}`;
 *            length of the array must match `NUM_OF_PROCESSES`
 *  - `MSG_BUFFER_SIZE`: how many messages can be buffered at each node
 *
 * LTL Properties
 * --------------
 *
 *  - `progress`: leader will be eventually elected (but does not cover uniqueness)
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm --cflags="-std=c++11" leader-unicircle_dkr.cpp
 *         $ divine verify -p assert leader-unicircle_dkr.bc -d
 *         $ divine verify -p deadlock leader-unicircle_dkr.bc -d
 *         $ divine verify -p progress leader-unicircle_dkr.bc -f -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DBUG" leader-unicircle_dkr.cpp -o leader-unicircle_dkr-bug.bc
 *         $ divine verify -p deadlock leader-unicircle_dkr-bug.bc -d
 *
 *  - customizing the number of processes and the distribution of IDs:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DNUM_OF_PROCESSES=3 -DPIDS=\"{2, 1, 3}\"" leader-unicircle_dkr.cpp
 *         $ divine verify -p progress leader-unicircle_dkr.bc -f -d
 *         $ divine verify -p deadlock leader-unicircle_dkr.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -std=c++11 -lpthread -lstdc++ -o leader-unicircle_dkr.exe leader-unicircle_dkr.cpp
 *       $ ./leader-unicircle_dkr.exe
 */

#ifndef NUM_OF_PROCESSES
#define NUM_OF_PROCESSES  3
#endif

// Distribution of identification numbers among processes to verify. It would be too expensive
// to try them all -- n! outgoing edges for given set of ID numbers.
// The size of the array should match value of macro NUM_OF_PROCESSES.
#ifndef PIDS
#define PIDS              { 8, 5, 3 }
#endif

// How many messages can be buffered at each node. When buffer is full, the send operation becomes
// blocking for that particular node. Larger buffer gives higher level of concurrency.
#ifndef MSG_BUFFER_SIZE
#define MSG_BUFFER_SIZE   2
#endif

// Protocol constants - do not change!
#define M1    1
#define M2    2
#define ELECT 3

#include <pthread.h>
#include <cstdlib>
#include <assert.h>
#include <unistd.h>

#ifdef __divine__    // verification
#include "divine.h"

LTL(progress, F(elected));

#else                // native execution
#include <iostream>

#define AP( x )

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename T>
void _info(const T& value) {
    std::cout << value << std::endl;
}

template <typename U, typename... T>
void _info(const U& head, const T&... tail) {
    std::cout << head;
    _info( tail... );
}
#endif

template <typename... T>
void info( const T&... args) {
#ifndef __divine__
    pthread_mutex_lock( &mutex );
    _info( args... );
    pthread_mutex_unlock( &mutex );
#endif
}

enum APs { elected };

int chef = -1;

void leader( int who ) { // Elected process should run this once.
    assert( chef == -1 );
    assert( who > -1 );
    chef = who;
    AP( elected );
}

template< typename T, int size >
struct Queue {
    // A lock-free inter-thread ring buffer.

    T q[ size ];
    volatile int front;
    volatile int rear;

    void enqueue( const T &x ) {
        while ( (front + 1) % size == rear ); // full; need to wait
        q[ front ] = x;
        front = (front + 1) % size;
    }

    T dequeue() {
        while ( rear == front ); // empty; nothing to read
        T x = q[ rear ];
        rear = (rear + 1) % size;
        return x;
    }

    bool empty() {
        return rear == front;
    }

    Queue() : front( 0 ), rear( 0 ) {}
};

struct Message {
    int type;
    int pid;
    int phase;
    int counter;

    Message( int t, int pid, int phase, int c )
        : type( t ), pid( pid ), phase( phase ), counter( c ) {}

    Message( int t, int pid )
        : type( t ), pid( pid ), phase( -1 ), counter( 0 ) {}

    Message() : type( 0 ), pid( 0 ), phase( -1 ), counter( 0 ) {}
};

struct Process {
    pthread_t thread;
    Queue< Message, MSG_BUFFER_SIZE > msg_buffer;

    int id;
    int elected;

    Process* next;

    enum State { Active, Waiting, Passive };

    void send( const Message &msg, int sender ) {
#ifndef __divine__
        usleep( 100000 );
#endif
        info( "Process ", sender, " sent a message (", msg.type, ",", msg.pid, ") to process ", id, "." );
        msg_buffer.enqueue( msg );
    }

    static void *run( void *_self ) {
        Process *self = reinterpret_cast< Process* >( _self );
        int max = self->id, phase = 0;
        State state = Active;
        Message msg;

        // Initiate leader election process.
        self->next->send( Message( M1, max, phase, 1 << phase ), self->id );

        while (1) {

            msg = self->msg_buffer.dequeue(); // Receive.

            if ( msg.type == ELECT ) { // Some process was elected.
                self->elected = msg.pid;
                self->next->send( msg, self->id );
                if ( self->elected == self->id )
                    leader( self->id ); // I'am the leader!
                break;
            }

            if ( msg.pid == self->id ) {
                // We have a leader and it's me!
                self->next->send( Message( ELECT, self->id ), self->id );
                continue;
            }

            switch ( state ) {
            case Active: {
                assert( msg.type == M1 );
                if ( msg.pid > max ) {
                    max = msg.pid;
                    state = Waiting; // Waiting for M2.
                    info( "Process ", self->id, " became Waiting." );
                } else {
                    state = Passive;
                    info( "Process ", self->id, " became Passive." );
                    self->next->send( Message( M2, max ), self->id );
                }
                break;
            }
            case Waiting: {
                if ( msg.type == M2 ) {
                    assert( msg.pid == max );
                    ++phase;
                    state = Active;
                    info( "Process ", self->id, " became Active." );
                    self->next->send( Message( M1, max, phase, 1 << phase ), self->id );
                    break;
                } else {
                    // M1
                    state = Passive;
                    info( "Process ", self->id, " became Passive." );
#ifdef BUG
                    break; // Incorrect, the message should be forwarded.
#endif
                    // Continue with the case for Passive state..
                }
            }
            case Passive: {
                if ( msg.type == M1 ) {
                    if ( msg.pid >= max && msg.counter >= 1 ) {
                        max = msg.pid;
                        if ( msg.counter > 1 ) {
                            --msg.counter; // Decrease message counter.
                            self->next->send( msg, self->id );
                        } else if ( msg.counter == 1 ) {
                            // This process is the stopping point for the next (if any) M2 message.
                            state = Waiting;
                            self->next->send( Message( M1, msg.pid, msg.phase, 0 ), self->id );
                            phase = msg.phase;
                        }
                    } else {
                        self->next->send( Message( M1, msg.pid, msg.phase, 0 ), self->id );
                    }
                } else {
                    // M2
                    // Filter out IDs which can't make it for the global maxima and forward others.
                    if ( msg.pid >= max ) {
                        self->next->send( msg, self->id );
                    }
                }
            }
            }
        }

        return NULL;
    }

    void join() {
        pthread_join( thread, NULL );
    }

    void start() {
        pthread_create( &thread, NULL, Process::run, reinterpret_cast< void* >( this ) );
    }

    // TODO: When operators _new_ and _delete_ are provided, the following method should be
    // implemented as a constructor.
    void init( int _id, Process *_next) {
        elected = -1;
        id = _id;
        next = _next;
    }

};

int main() {
    assert( NUM_OF_PROCESSES > 0 );

    Process process[NUM_OF_PROCESSES];
    int pids[] = PIDS;
    int i, max = -1;

    // Let's find out who should be the leader.
    for ( i = 0; i < NUM_OF_PROCESSES; i++ )
        if ( pids[i] > max )
            max = pids[i];

    // Initiate and start the election.
    for ( i = 0; i < NUM_OF_PROCESSES; i++ ) {
        process[i].init( pids[i], process + ( (i + 1) % NUM_OF_PROCESSES ) );
        process[i].start();
    }

    for ( i = 0; i < NUM_OF_PROCESSES; i++ )
        process[i].join();

    assert( chef > -1 );   // Someone should be elected..
    assert( chef == max ); // .. and we know who should it be.
    for ( i = 0; i < NUM_OF_PROCESSES; i++ ) // Everyone should agree on the same leader.
        assert( process[i].elected == chef );

    info ( "Process ", chef, " was elected to be the leader." );
    return 0;
}

