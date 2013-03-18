/*
 * Variation on the popular (one-person) elevator theme, suggested by Radek Pelanek.
 *
 * When a person calls the elevator, his request is enqueued into the queue assigned
 * for the floor he is at. The person has to wait until the floor he is at is being
 * served by the elevator. When the elevator comes, the person enters and selects
 * a destination, then the person waits again, until the destination is reached.
 * Requests from different floors are processed by the elevator in a circular fashion:
 * at some point the floor no. 1 is served, which means that the oldest request is
 * dequeued and processed, then the elevator switches his attention for the oldest
 * request of the second floor and so on. When the last floor was served this way,
 * the elevator goes back to serve for the first floor.
 * When a person leaves the elevator, there are two options available of how
 * the elevator can continue. It can then serve for the floor that was a destination
 * for the last passanger (STRATEGY=1), i.e. the floor the elevator is currently in,
 * or the floor one above can be the next to serve (STRATEGY=2).
 *
 * Verify with:
 *  $ divine compile --llvm --cflags="-std=c++11 "[" < flags > "] elevator.cpp
 *  $ divine verify -p assert elevator.bc [-d]
 * Execute with:
 *  $ clang++ -std=c++11 [ < flags > ] -lpthread -lstdc++ -o elevator.exe elevator.cpp
 *  $ ./elevator.exe
 */

// A number of persons and served floors, both should be >= 2.
#define PERSONS  2
#define FLOORS   3

#define STRATEGY 1

// -1 = infinite
#define NUM_OF_RIDES    2

#include <pthread.h>
#include <cstdlib>

// For native execution.
#ifndef DIVINE
#include <cassert>
#include <iostream>
#include <unistd.h>

#define ap( x )

pthread_mutex_t mutex;

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
#ifndef DIVINE
    pthread_mutex_lock( &mutex );
    _info( args... );
    pthread_mutex_unlock( &mutex );
#endif
}

enum AP { waiting1, in_elevator1, out1, in_elevator2 };

#ifdef DIVINE
LTL(exclusion, G(!(in_elevator1 && in_elevator2)));
LTL(progress1, G(waiting1 -> F(in_elevator1)));
LTL(progress2, G(in_elevator1 -> F(out1)));
#endif

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
    Queue< int, ( PERSONS > 1 ? PERSONS : 2 ) + 1 > floor_queue[ FLOORS ];
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
            ap( waiting1 );
        while ( waits_for != who );

        info ( "Person ", who, " is being transported by the elevator to the ", dest, ". floor." );
        if ( who == 1 )
            ap( in_elevator1 );
        if ( who == 2 )
            ap( in_elevator2 );
        going_to = dest;
        waits_for = 0;
        while ( current != dest );

        info ( "Person ", who, " leaved the elevator." );
        if ( who == 1 )
            ap( out1 );
        going_to = 0;
        return dest;
    }

    void _move( int direction ) { // 1 = up, -1 = down
#ifndef DIVINE
        usleep( 500000 );
#endif
        assert( direction == 1 || direction == -1 );
        current += direction;
        assert( current >= 1 && current <= FLOORS );
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
                    self->serving = ( self->serving % FLOORS ) + 1;
                }

            } else {
                bool request = false;
                for ( int i = 0; i < FLOORS; i++ ) {
                    if ( !self->floor_queue[i].empty() )
                        request = true;

                }
                if ( request )
                    self->serving = ( self->serving  % FLOORS ) + 1;
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
        : current( 1 ), serving( 1 ), waits_for( 0 ), going_to( 0 ),
          strategy( strategy ), terminated( false ) {}
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
#ifdef DIVINE
            int dest = __divine_choice( FLOORS - 1 ) + 1;
#else // native execution
            int dest = ( rand() % (FLOORS - 1) ) + 1;
#endif
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
    Person persons[ PERSONS ];
    for ( int i = 0; i < PERSONS; i++ ) {
        persons[i].init( i+1, ( i % FLOORS ) + 1, &elevator );
        info ( "Person ", i+1," starts at the ", ( i % FLOORS ) + 1, ". floor.");
        persons[i].start();
    }

#if ( NUM_OF_RIDES > -1 )
    // Finit number of rides.
    for ( int i = 0; i < PERSONS; i++ )
        persons[i].join();

    elevator.terminate();
#else
    // Go infinitely (however the state space is finite).
    while ( true );
#endif

    return 0;
}
