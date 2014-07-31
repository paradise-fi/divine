/* divine-cflags: -std=c++11 */
/*
 * Leader election in an Uni-circle (Basic algorithm)
 * ==================================================
 *
 *  A simulation of a "Basic" O(n*log(n)) unidirectional distributed algorithm for
 *  extrema finding in a circle.
 *
 *  *tags*: leader election, C++11
 *
 * Description
 * -----------
 *
 *  Given `n` processes in a ring, communicating only with message passing to its right
 *  neighbour, the unidirectional circular extrema-finding problem (more likely known as a leader
 *  election problem) is to select a maximum (or minimum) process.
 *  Each process has a unique value, called ID, in a set with a total order. These values
 *  may be transmitted and compared. All processes are identical (except for their IDs) and `n`,
 *  the number of processes, is not initially known.
 *
 *  The algorithm presented here is due to Danny Dolev and Maria M. Klawe and Michael Rodeh [1]
 *  as well as Gary L. Peterson [2] who had submitted their papers at around the same time.
 *  Even thought the algorithm is quite simple, it has some good properties, especially the
 *  upper bound on message passes, which is only: 2*n*log(n) + O(n).
 *
 *  The idea of the algorithm is to find all local maximas (among active IDs) and keep them active
 *  also for the next iteration (= phase), while the rest is dismisshed. This way the number of
 *  active processes lowers after each phase by a half at least. But while doing so, each maximum
 *  moves to the next active process on the right side, as it is the only reasonable method as to
 *  compare 3 adjacent IDs in unidirectional circle. And so after each phase the active process
 *  becomes either passive or it starts to represent the maximum previously owned by its nearest active
 *  process to the left. But when compiled with `-DBUG`, local maximas don't make the move and
 *  therefore get dismisshed.
 *
 * ### References: ###
 *
 *  1. An O(n log n) Unidirectional Distributed Algorithm for Extrema Finding in a Circle
 *
 *           @article{ dolev:an,
 *                    author = "Danny Dolev and Maria M. Klawe and Michael Rodeh",
 *                    title = "An O(n log n) Unidirectional Distributed Algorithm for Extrema
 *                             Finding in a Circle",
 *                    journal = "J. Algorithms",
 *                    pages = {245-260},
 *                    year = {1982},
 *                  }
 *
 *  2. An O(nlog n) Unidirectional Algorithm for the Circular Extrema Problem.
 *
 *           @article{ Peterson:1982:ONL:69622.357194,
 *                    author = "Peterson, Gary L.",
 *                    title = "An O(nlog n) Unidirectional Algorithm for the Circular Extrema Problem",
 *                    journal = "ACM Trans. Program. Lang. Syst.",
 *                    year = {1982},
 *                    issn = {0164-0925},
 *                    pages = {758--762},
 *                    publisher = "ACM",
 *                    address = "New York, NY, USA",
 *                  }
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the safety property
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
 *         $ divine compile --llvm --cflags="-std=c++11" leader-unicircle_basic.cpp
 *         $ divine verify -p assert leader-unicircle_basic.bc -d
 *         $ divine verify -p deadlock leader-unicircle_basic.bc -d
 *         $ divine verify -p progress leader-unicircle_basic.bc -f -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DBUG" leader-unicircle_basic.cpp -o leader-unicircle_basic-bug.bc
 *         $ divine verify -p assert leader-unicircle_basic-bug.bc -d
 *
 *  - customizing the number of processes and the distribution of IDs:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DNUM_OF_PROCESSES=3 -DPIDS=\"{2, 1, 3}\"" leader-unicircle_basic.cpp
 *         $ divine verify -p progress leader-unicircle_basic.bc -f -d
 *         $ divine verify -p assert leader-unicircle_basic.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -std=c++11 -lpthread -lstdc++ -o leader-unicircle_basic.exe leader-unicircle_basic.cpp
 *       $ ./leader-unicircle_basic.exe
 */

// A number of processes in the ring.
#ifndef NUM_OF_PROCESSES
#define NUM_OF_PROCESSES  5
#endif

// Distribution of identification numbers among processes to verify. It would be too expensive
// to try them all -- n! outgoing edges for given set of ID numbers.
// The size of the array should match value of macro NUM_OF_PROCESSES.
#ifndef PIDS
#define PIDS              { 5, 6, 4, 7, 8 }
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
#include <stdlib.h>
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

    Message( int t, int p )
        : type( t ), pid( p ) {}

    Message() : type( 0 ), pid( 0 ) {}
};

struct Process {
    pthread_t thread;
    Queue< Message, MSG_BUFFER_SIZE > msg_buffer;

    int id;
    int elected;

    Process* next;

    void send( const Message &msg, int sender ) {
#ifndef __divine__
        usleep( 100000 );
#endif
        info( "Process ", sender, " sent a message (", msg.type, ",", msg.pid, ") to process ", id, "." );
        msg_buffer.enqueue( msg );
    }

    static void *run( void *_self ) {
        Process *self = reinterpret_cast< Process* >( _self );
        int max = self->id, left, phase = 0;
        bool active = true;
        Message msg;

        // Initiate leader election process.
        self->next->send( Message( M1, max ), self->id );

        while (1) {

            msg = self->msg_buffer.dequeue(); // Receive.

            if ( msg.type == ELECT ) { // Some process was elected.
                self->elected = msg.pid;
                self->next->send( msg, self->id );
                if ( self->elected == self->id )
                    leader( self->id ); // I'am the leader!
                break;
            }

            if ( active ) {
                if ( msg.type == M1 ) {
                    assert( phase == 0 );
                    if ( msg.pid != max ) {
                        self->next->send( Message( M2, msg.pid ), self->id );
                        left = msg.pid;
                    } else {
                        // We have a leader!
                        // When local maximum makes one complete turn, we know that it is
                        // the global maximum.
                        self->next->send( Message( ELECT, max ), self->id );
                    }
                }

                if ( msg.type == M2 ) {
                    assert( phase == 1 );
                    if ( left > msg.pid && left > max ) {
                        // My nearest active process to the left owns current local maxima.
#ifndef BUG
                        max = left; // Local maxima makes the move.
#endif
                        self->next->send( Message( M1, max ), self->id );
                    } else {
                        active = false;
                        info( "Process ", self->id, " became Passive." );
                    }
                }

                phase = 1 - phase;
            } else { // Passive = all messages are just forwarded.
                self->next->send( msg, self->id );
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
