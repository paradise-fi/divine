/* divine-cflags: -std=c++11 */
/*
 * Elevator
 * ========
 *
 *  Variation on the popular (one-person) elevator theme, suggested by *Radek Pelánek*.
 *
 *  *tags*: controller, C++11
 *
 * Description
 * -----------
 *
 *  When a person calls the elevator, his request is enqueued into the queue assigned
 *  for the floor he is at. The person has to wait until the floor he is at is being
 *  served by the elevator. When the elevator comes, the person enters and selects
 *  a destination, then the person waits again, until the destination is reached.
 *  Requests from different floors are processed by the elevator in a circular fashion:
 *  at some point the floor no. 1 is served, controller dequeues and processes
 *  the oldest request from this floor, then the elevator switches his attention
 *  for the oldest request of the second floor and so on. When the last
 *  floor was served this way, the elevator goes back to serve for the first floor again.
 *  When a person leaves the elevator, there are two options available of how
 *  the elevator can continue. It can then serve for the floor that was a destination
 *  for the last passanger (`STRATEGY=1`), i.e. the floor the elevator is currently at,
 *  or the floor one above can be the next to serve (`STRATEGY=2`).
 *
 * Parameters
 * ----------
 *
 *  - `NUM_OF_PERSONS`: a number of people actively using the elevator
 *  - `NUM_OF_FLOORS`: a number of served floors
 *  - `NUM_OF_RIDES`: a number of requests to handle (`-1` = infinity)
 *  - `STRATEGY`: which strategy to follow (select between `1` and `2`)
 *
 * LTL Properties
 * --------------
 *
 *  - `exclusion`: only one person can use the elevator at a time
 *  - `progress_in`: if a persons calls the elevator, it will eventually come to his floor and allow him to enter
 *  - `progress_out`: if a person is being transported by the elevator, it will eventually stop and allow him to leave
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm --cflags="-std=c++11" elevator.cpp
 *         $ divine verify -p assert elevator.bc -d
 *         $ divine verify -p safety elevator.bc -d
 *         $ divine verify -p exclusion elevator.bc -d
 *         $ divine verify -p progress_in elevator.bc --fair -d
 *         $ divine verify -p progress_out elevator.bc --fair -d
 *
 *  - customizing some parameters:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DNUM_OF_PERSONS=5 -DNUM_OF_RIDES=-1" elevator.cpp
 *         $ divine verify -p progress_in elevator.bc --fair -d
 *         $ divine verify -p progress_out elevator.bc --fair -d
 *
 *  - changing the strategy:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DSTRATEGY=2" elevator.cpp
 *         $ divine verify -p assert elevator.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -std=c++11 -lpthread -o elevator.exe elevator.cpp
 *       $ ./elevator.exe
 */

// A number of persons and served floors, both should be >= 2.
#ifndef NUM_OF_PERSONS
#define NUM_OF_PERSONS  2
#endif
#ifndef NUM_OF_FLOORS
#define NUM_OF_FLOORS   3
#endif

// An ID of strategy to follow (select between `1` and `2`).
#ifndef STRATEGY
#define STRATEGY 1
#endif

// A number of requests to handle.
// -1 = infinity
#ifndef NUM_OF_RIDES
#define NUM_OF_RIDES    2
#endif

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#ifdef __divine__    // verification
#include "divine.h"

LTL(exclusion, G((in_elevator1 -> (!in_elevator2 W out1)) && (in_elevator2 -> (!in_elevator1 W out2))));
LTL(progress_in, G(waiting1 -> F(in_elevator1)));
LTL(progress_out, G(in_elevator1 -> F(out1)));

#else                // native execution
#include <iostream>

#define AP( x )

#define __divine_choice( x ) ( rand() % ( x ) )

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

enum APs { waiting1, in_elevator1, out1, in_elevator2, out2 };

template< typename T, int size >
struct Queue {
    // Thread safe implementation of a fixed size queue for one reader and many writers.

    T q[ size ];
    volatile int front;
    volatile int rear;
    pthread_mutex_t mutex;

    void enqueue( T x ) {
        pthread_mutex_lock( &mutex );
        while ( (front + 1) % size == rear ); // full; need to wait
        q[ front ] = x;
        front = (front + 1) % size;
        pthread_mutex_unlock( &mutex );
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

    Queue() : front( 0 ), rear( 0 ) {
        pthread_mutex_init( &mutex, NULL );
    }
};

struct Elevator {
    pthread_t thread;
    Queue< int, ( NUM_OF_PERSONS > 1 ? NUM_OF_PERSONS : 2 ) + 1 > floor_queue[ NUM_OF_FLOORS ];
    bool terminated;

    volatile int current; // Floor the elevator is currently in.
    int serving; // Floor which is being served.

    // synchronization with passanger
    volatile int waits_for;
    volatile int going_to;

    // input
    int strategy;

    int transport( int from, int dest, int who ) { // runs in passanger's thread
        assert( from > 0 && dest > 0 && who > 0 );
        floor_queue[ from-1 ].enqueue( who );
        info ( "Person ", who, " called the elevator." );
        if ( who == 1 )
            AP( waiting1 );
        while ( waits_for != who );

        info ( "Person ", who, " is being transported by the elevator to the ", dest, ". floor." );
        if ( who == 1 )
            AP( in_elevator1 );
        if ( who == 2 )
            AP( in_elevator2 );
        going_to = dest;
        waits_for = 0;
        while ( current != dest );

        info ( "Person ", who, " leaved the elevator." );
        if ( who == 1 )
            AP( out1 );
        if ( who == 2 )
            AP( out2 );
        going_to = 0;
        return dest;
    }

    void _move( int direction ) { // 1 = up, -1 = down
#ifndef __divine__
        usleep( 500000 );
#endif
        assert( direction == 1 || direction == -1 );
        current += direction;
        assert( current >= 1 && current <= NUM_OF_FLOORS );
    }

    static void *run( void *_self ) {
        Elevator *self = reinterpret_cast< Elevator* >( _self );

        while ( !self->terminated ) {
            if ( !self->floor_queue[ self->serving-1 ].empty() ) {
                // Go for a passanger.
                while ( self->serving < self->current )
                    self->_move( -1 );
                while ( self->serving > self->current )
                    self->_move( 1 );

                assert( !self->floor_queue[ self->current-1 ].empty() );
                self->waits_for = self->floor_queue[ self->current-1 ].dequeue();
                while ( self->waits_for );
                // Passanger entered the elevator.

                // Passanger is being transported.
                assert( self->going_to );
                while ( self->going_to < self->current )
                    self->_move( -1 );
                while ( self->going_to > self->current )
                    self->_move( 1 );
                while( self->going_to );
                // Passanger leaved the elevator.

                if ( self->strategy == 1 ) {
                    self->serving = self->current;
                } else {
                    self->serving = ( self->serving % NUM_OF_FLOORS ) + 1;
                }

            } else {
                bool request = false;
                for ( int i = 0; i < NUM_OF_FLOORS; i++ ) {
                    if ( !self->floor_queue[i].empty() )
                        request = true;

                }
                if ( request )
                    self->serving = ( self->serving  % NUM_OF_FLOORS ) + 1;
            }
        }

        return NULL;
    }

    void terminate() {
        terminated = true;
        pthread_join( thread, NULL );
    }

    void start() {
        pthread_create( &thread, NULL, Elevator::run, reinterpret_cast< void* >( this ) );
    }

    Elevator( int strategy )
        : terminated( false ), current( 1 ), serving( 1 ), waits_for( 0 ),
          going_to( 0 ), strategy( strategy ) {}
};

struct Person {
    pthread_t thread;

    int id;
    int at_floor;

    Elevator* elevator;

    static void* run( void* _self ) {
        Person* self = reinterpret_cast< Person* >( _self );

#if ( NUM_OF_RIDES > -1 )
        for ( int i = 0; i < NUM_OF_RIDES; i++ ) {
#else
        for (;;) {
#endif
            // Randomly choose destination.
            int dest = __divine_choice( NUM_OF_FLOORS - 1 ) + 1;
            if ( dest >= self->at_floor )
                ++dest;

            self->at_floor = self->elevator->transport( self->at_floor, dest, self->id );
        }

        return NULL;
    }

    void join() {
        pthread_join( thread, NULL );
    }

    // TODO: When operators _new_ and _delete_ are provided, the following method should be
    // implemented as a constructor.
    void init( int _id, int _at_floor, Elevator *_elevator ) {
        id = _id;
        at_floor = _at_floor;
        elevator = _elevator;
    }

    void start() {
        pthread_create( &thread, NULL, Person::run, reinterpret_cast< void* >( this ) );
    }

};

int main() {
    // Create and start an elevator.
    Elevator elevator( STRATEGY );
    elevator.start();

    // Create an instance of each person.
    Person persons[ NUM_OF_PERSONS ];
    for ( int i = 0; i < NUM_OF_PERSONS; i++ ) {
        persons[i].init( i+1, ( i % NUM_OF_FLOORS ) + 1, &elevator );
        info ( "Person ", i+1," starts at the ", ( i % NUM_OF_FLOORS ) + 1, ". floor.");
        persons[i].start();
    }

#if ( NUM_OF_RIDES > -1 )
    // Finit number of rides.
    for ( int i = 0; i < NUM_OF_PERSONS; i++ )
        persons[i].join();

    elevator.terminate();
#else
    // Go infinitely (however the state space is finite).
    while ( true );
#endif

    return 0;
}
