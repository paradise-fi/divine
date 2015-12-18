/* divine-cflags: -std=c++11 */
/*
 * Collision
 * =========
 *
 *  A simple collision avoidance protocol.
 *
 *  *tags*: communication protocol, C++11
 *
 * Description
 * -----------
 *
 *  We assume that a number of stations are connected on an *Ethernet-like* medium,
 *  that is, the basic protocol is of type *CSMA/CD*. On the top of this basic protocol
 *  we want to design a protocol without collisions. This is a simple solution
 *  with a dedicated master station, which in turn asks the other stations if they
 *  want to transmit data to another station.
 *
 *  Original idea comes from the paper listed among references.
 *  Authors of this paper presented a model which assumes an existence of some
 *  dedicated links between the master station and slave stations.
 *  Throught this links the master can obtain current state of each slave and
 *  figure out when is the right time to pass the token to the next slave in the row.
 *  However the actual token is send over the ethernet instead, hence different
 *  channels are used to manage token passing.
 *  In order to eliminate this discrepancy and make the algorithm a bit more practical,
 *  we present a modified version in which all communication between master and slave
 *  stations goes throught the ethernet medium. This is done by implementing a new message
 *  type representing a token being returned from a slave to the master.
 *  If the thread that received the token is not interested to send any message, a message
 *  returning the token to the master is send immediately (more precisely -- when the medium
 *  becomes free). Otherwise the token owner sends a message wrapping both actual data and
 *  the token to the recipient of his choice, which is than obligated to return the token
 *  to the master immediately after the message receipt (without having permission to
 *  use it).
 *
 *  When a message is send over the ethernet medium, it gets broadcasted to all connected
 *  stations (except the one from which it came from). Inherently this operation isn't
 *  atomic, and so when a slave receives the token, even thought it has got the permission
 *  to send a message, it can't start doing so immediately, but has to wait until the last
 *  broadcast finnishes and medium becomes free. This is violated when the program is built
 *  with a macro `BUG` defined.
 *
 * ### References: ###
 *
 *  1. Modelling and Analysis of a Collision Avoidance Protocol using SPIN and UPPAAL.
 *
 *           @inproceedings {
 *               Jensen96modellingand,
 *               author = "Henrik Ejersbo Jensen and Jensen Kim and Kim Guldstrand Larsen and Arne Skou",
 *               title = "Modelling and Analysis of a Collision Avoidance Protocol using SPIN and UPPAAL",
 *               booktitle = "Proceedings of the 2nd International Workshop on the SPIN Verification System",
 *               year = "1996"
 *           }
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the safety property
 *  - `NUM_OF_THREADS`: a number of stations connected to the ethernet (includes the master)
 *  - `NUM_OF_PASSES`: how many times the token is passed from one station to another
 *                     (with the aid of the master) before the algorithm finishes (`-1` = infinity)
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm --cflags="-std=c++11" collision.cpp
 *         $ divine verify -p assert collision.bc -d
 *         $ divine verify -p safety collision.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DBUG" collision.cpp -o collision-bug.bc
 *         $ divine verify -p assert collision-bug.bc -d
 *
 *  - customizing the parameters:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DNUM_OF_THREADS=4 -DNUM_OF_PASSES=3" collision.c
 *         $ divine verify -p assert collision.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -std=c++11 -lpthread -o collision.exe collision.cpp
 *       $ ./collision.exe
 */

// A number of stations connected to the ethernet (includes the master).
#ifndef NUM_OF_STATIONS
#define NUM_OF_STATIONS    3
#endif

// How many times the token is passed from one station to another (with the aid of the master)
// before the algorithm finishes.
// -1 = infinity.
#ifndef NUM_OF_PASSES
#define NUM_OF_PASSES      2
#endif

// Protocol constants -- do not change!
#define DATA     0
#define ENQUIRY  1
#define RETURN   2

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#ifdef __divine__    // verification
#include "divine.h"

#else                // native execution
#include <iostream>

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

struct Message {
    int sender;
    int receiver;
    int type;

    // Here would go the message body...

    Message() {}

    Message( int s, int r, int t )
        : sender( s ), receiver( r ), type( t ) {}
};

// Forward declaration.
struct Medium;

struct Station {
    Medium *medium;
    int id;

    bool buffered;
    Message message; // A buffer for just one message.

    void receive( const Message &msg ) {
        // This method is used by medium only => locking is not necessary.
        while ( buffered ); // Receive is blocking.
        message = msg;
        buffered = true;
    }

    void init( int _id, Medium *_medium ) {
        id = _id;
        medium = _medium;
    }

    virtual void start() = 0;

    Station() : buffered( false ) {}
};

struct Medium {
    pthread_t thread;
    pthread_mutex_t mutex;
    bool terminated;

    Station **stations;

    bool buffered;
    Message message; // Only one message can be broadcasted at any time.

    bool busy() {
        return buffered;
    }

    void send( const Message &msg ) {
        pthread_mutex_lock( &mutex );
        assert( !buffered ); // Collision detection.
        message = msg;
        buffered = true;
        pthread_mutex_unlock( &mutex );
    }

    static void *run( void *_self ) {
        Medium *self = reinterpret_cast< Medium* >( _self );
        int i;

        while ( !self->terminated ) {
            while ( !self->buffered && !self->terminated );

            if ( self->terminated )
                break;

            // Broadcast the message.
            for ( i = 0; i < NUM_OF_STATIONS; i++ ) {
                if ( i != self->message.sender )
                    self->stations[i]->receive( self->message );
#ifndef __divine__
                usleep( 100000 );
#endif
            }

            // When _buffered_ equals _true_, only medium can modify it
            // and so locking is not necessary in this case.
            self->buffered = false;
        }

        return NULL;
    }

    void terminate() {
        terminated = true;
        pthread_join( thread, NULL );
    }

    void start() {
        pthread_create( &thread, NULL, Medium::run, reinterpret_cast< void* >( this ) );
    }

    Medium( Station **stations )
        : terminated( false ), stations( stations ), buffered( false ) {
        pthread_mutex_init( &mutex, NULL );
    }
};

struct Slave : public Station {
    pthread_t thread;
    bool terminated;

    static void *run( void *_self ) {
        Slave *self = reinterpret_cast< Slave* >( _self );
        int receiver;

        while ( !self->terminated ) {
            while ( !self->buffered && !self->terminated );

            if ( self->terminated )
                break;

            if ( self->message.receiver == self->id ) {
                // Process the received message.
                if ( self->message.type == ENQUIRY ) {
#ifndef BUG
                    // Even if a thread owns the token and so is allowed to send a message,
                    // it still has to check the busy condition of the medium to prevent from collision.
                    while ( self->medium->busy() );
#endif
                    // Decide what to do with the token.
                    int choice = __divine_choice( 2 );
                    if ( choice == 0 ) {
                        // send a message
                        receiver = __divine_choice( NUM_OF_STATIONS - 2 ) + 1;
                        receiver += ( receiver >= self->id ? 1 : 0 );
                        self->medium->send( Message( self->id, receiver, DATA ) );
                        info("Station ", self->id,
                             " received the token and sent a message to the station ", receiver, "." );
                    } else {
                        // not interested
                        self->medium->send( Message( self->id, 0, RETURN ) ); // Return the token immediately.
                        info("Station ", self->id,
                             " received the token, but wasn't interested to send any message." );
                    }
                } else if ( self->message.type == DATA ) {
                    // Here the station would read and process the message body...
                    info("Station ", self->id, " received a message from the station ", self->message.sender, "." );
                    // The receiver is responsible to return the token to the master.
                    while ( self->medium->busy() );
                    self->medium->send( Message( self->id, 0, RETURN ) );
                }
            }

            self->buffered = false;
        }

        return NULL;
    }

    void terminate() {
        terminated = true;
        pthread_join( thread, NULL );
    }

    virtual void start() {
        pthread_create( &thread, NULL, Slave::run, reinterpret_cast< void* >( this ) );
    }

    Slave() : terminated( false ) {}
};

struct Master : public Station {
    pthread_t thread;

    static void *run( void *_self ) {
        Master *self = reinterpret_cast< Master* >( _self );
        int next = 0;
        bool has_token = true;

#if ( NUM_OF_PASSES > -1 )
        for ( int i = 0; i < NUM_OF_PASSES; i++ ) {
#else
        while ( 1 ) {
#endif
            while ( self->medium->busy() );

            next = ( next % ( NUM_OF_STATIONS - 1 ) ) + 1;
            info( "Master: \"Sending token to the station ", next, "\"." );
            self->medium->send( Message( 0, next, ENQUIRY ) );
            has_token = false;

            // Wait for the token to return
            while ( !has_token ) {
                while ( !self->buffered );

                if ( self->message.receiver == 0 && self->message.type == RETURN ) {
                    has_token = true;
                    info( "The master station received the token back." );
                }

                self->buffered = false;
            }
        }

        return NULL;
    }

    void join() {
        pthread_join( thread, NULL );
    }

    virtual void start() {
        pthread_create( &thread, NULL, Master::run, reinterpret_cast< void* >( this ) );
    }

};


int main() {
    assert( NUM_OF_STATIONS > 2 );

    // TODO: When operators _new_ and _delete_ are provided, the array _stations_ should be
    // constructed in a more standard way (use the heap for stations, instead of the stack).
    Slave slaves[NUM_OF_STATIONS-1];
    Master master;
    Station *stations[NUM_OF_STATIONS];
    stations[0] = &master;
    for ( int i = 1; i < NUM_OF_STATIONS; i++ )
        stations[i] = &slaves[i-1];

    Medium medium( stations );
    medium.start();

    for ( int i = 0; i < NUM_OF_STATIONS; i++ ) {
        // The master has ID = 0 !
        stations[i]->init( i, &medium );
        stations[i]->start();
    }

#if ( NUM_OF_TURNS > -1 )
    // Finite number of turns.
    master.join();

    for ( int i = 0; i < NUM_OF_STATIONS-1; i++ )
        slaves[i].terminate();

    medium.terminate();
#else
    // Go infinitely (however the state space is (should be) finite).
    while ( true );
#endif

    return 0;
}

