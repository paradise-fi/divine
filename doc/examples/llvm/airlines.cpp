/* divine-cflags: -std=c++11 */
/*
 * Airlines
 * ========
 *
 *  Simple simulation of a system for selling airplane tickets,
 *  supporting multiple transactions running in parallel.
 *
 *  *tags*: information systems (IS), C++11
 *
 * Description
 * -----------
 *
 *  This is a C++ port of a benchmark *Airlines* written by *Zdenek Letko*
 *  from the research group [VeriFIT][1] at *FIT VUT Brno*.
 *
 *  This program is a simulation of a simple information system used for selling
 *  airplane tickets, supporting multiple transactions running in parallel.
 *  From an information theory point of view it is nothing else than just another
 *  instance of the *producers-consumers* pattern. The class *CustomerGenerator*
 *  produces requests (one per customer) and sends each of them to a randomly selected
 *  _InternetSeller_. Seller searches flight databases of all available airlines
 *  and tries to find a flight with a free seat matching the requested criteria.
 *
 *  If the program is compiled with flag `-DBUG`, than the algorithm is incorrect.
 *  Normally, when *InternetSeller* finds a possibly free seat matching the criteria,
 *  it tests whether the seat wasn't by a change sold by another seller in
 *  the meantime, if not than the ticket is bought. For this to be correct,
 *  the test for availability and the purchase itself have to be performed
 *  together atomically. But this is not the case with macro `BUG` defined.
 *
 *  [1]: http://www.fit.vutbr.cz/research/groups/verifit/.cs
 *
 * Parameters
 * ----------
 *
 *  - `BUG`: if defined than the algorithm is incorrect and violates the safety property
 *  - `NUM_OF_STHREADS`: a number of ticket sellers
 *  - `NUM_OF_CTHREADS`: a number of generators, each producing new customers
 *  - `NUM_OF_CUSTOMS_IN_THREAD`: a number of customers running in one thread
 *  - `NUM_OF_AIRLINES`: a number of airlines,
 *  - `MAX_NUM_OF_FLIGHTS`: a maximum number of flights an airline can provide
 *  - `MAX_NUM_OF_SEATS`: a maximum number of seats an airplane can have
 *  - `QUEUE_SIZE`: a size of the queue used for storing customers' requests
 *  - `NUM_OF_DESTINATIONS`: a number of airports
 *
 * Verification
 * ------------
 *
 *  - all available properties with the default values of parameters:
 *
 *         $ divine compile --llvm --cflags="-std=c++11" airlines.cpp
 *         $ divine verify -p assert airlines.bc -d
 *         $ divine verify -p safety airlines.bc -d
 *
 *  - introducing a bug:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DBUG" airlines.cpp -o airlines-bug.bc
 *         $ divine verify -p assert airlines-bug.bc -d
 *
 *  - customizing some integer parameters:
 *
 *         $ divine compile --llvm --cflags="-std=c++11 -DNUM_OF_AIRLINES=5 -DQUEUE_SIZE=10" airlines.cpp
 *         $ divine verify -p assert airlines.bc -d
 *
 * Execution
 * ---------
 *
 *       $ clang++ -std=c++11 -lpthread -o airlines.exe airlines.cpp
 *       $ ./airlines.exe
 */

// Number of ticket sellers.
#ifndef NUM_OF_STHREADS
#define NUM_OF_STHREADS           2
#endif

// Number of customer generators.
#ifndef NUM_OF_CTHREADS
#define NUM_OF_CTHREADS           2
#endif

// Number of customers running in one thread.
#ifndef NUM_OF_CUSTOMS_IN_THREAD
#define NUM_OF_CUSTOMS_IN_THREAD  2
#endif

// Number of airlines.
#ifndef NUM_OF_AIRLINES
#define NUM_OF_AIRLINES           3
#endif

// Maximum number of flights an airline can offer.
#ifndef MAX_NUM_OF_FLIGHTS
#define MAX_NUM_OF_FLIGHTS        10
#endif

// Maximum number of seats an airplane can have.
#ifndef MAX_NUM_OF_SEATS
#define MAX_NUM_OF_SEATS          2
#endif

// Size of the queue used for storing customers' requests.
#ifndef QUEUE_SIZE
#define QUEUE_SIZE                4
#endif

// Number of airports.
#ifndef NUM_OF_DESTINATIONS
#define NUM_OF_DESTINATIONS       3
#endif

#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __divine__       // verification
#include "divine.h"

#else                   // native execution
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

#ifdef __divine__
// override new to non-failing version
#include <new>

void* operator new  ( std::size_t count ) { return __divine_malloc( count ); }
#endif

// ------------------------------------

template< typename T, int size >
class Queue {
    // Thread safe implementation of a fixed size queue for one reader and many writers.

  private:

    T q[ size ];
    volatile int front;
    volatile int rear;
    pthread_mutex_t mutex;

  public:

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

// ------------------------------------

class FlightTicket {

  private:

    int customer_id;
    int source, destination;
    int position;
    pthread_mutex_t mutex;

  public:

    FlightTicket( int src, int dst, int seat )
        : customer_id( -1 ), source( src ), destination( dst ),
          position( seat ) {
        pthread_mutex_init( &mutex, NULL );
    }

    void buy( int customer ) {
        customer_id = customer;
    }

    bool is_free() {
        return customer_id == -1;
    }

    bool buy_with_check( int customer ) {
        // It is unsafe to use just is_free() followed by buy(customer),
        // since it violates atomicity.

        bool bought = false;
        pthread_mutex_lock( &mutex );
        if ( is_free() ) {
            buy( customer );
            bought = true;
        }
        pthread_mutex_unlock( &mutex );
        return bought;
    }

    int get_position() {
        return position;
    }

    int get_source() {
        return source;
    }

    int get_destination() {
        return destination;
    }
};

// ------------------------------------

class Flight {

  private:

    int id; // unique at the scope of an airport
    int source, destination;
    int capacity;
    FlightTicket **tickets;

  public:

    Flight( int id, int src, int dst, int seats )
        : id( id ), source( src ), destination( dst ),
          capacity( seats ) {

        tickets = new FlightTicket *[ capacity ];

        for ( int i = 0; i < capacity; i++ ) {
            tickets[i] = new FlightTicket( source, destination, i );
        }
    }

    ~Flight() {
        for ( int i = 0; i < capacity; i++ )
            delete tickets[i];
        delete[] tickets;
    }

    int get_id() {
        return id;
    }

    int get_source() {
        return source;
    }

    int get_destination() {
        return destination;
    }

    FlightTicket *get_first_free_seat() {
        for ( int i = 0; i < capacity; i++ )
            if ( tickets[i]->is_free() )
                return tickets[i];
        return NULL;
    }

    int num_of_sold_seats() {
        int count = 0;

        for ( int i = 0; i < capacity; i++ )
            if ( !tickets[i]->is_free() )
                ++count;

        return count;
    }

};

// ------------------------------------

class Airlines {

  private:

    int id;
    int num_of_flights;
    Flight **flights;

  public:

    Airlines( int id ) : id( id ), flights( NULL ) {}

    ~Airlines() {
        if ( flights ) {
            for ( int i = 0; i < num_of_flights; i++ )
                delete flights[i];
            delete[] flights;
        }
    }

    void generate_flights() {
        int src, dst;

        num_of_flights = __divine_choice( MAX_NUM_OF_FLIGHTS ) + 1;
        flights = new Flight *[ num_of_flights ];

        for ( int i = 0; i < num_of_flights; i++ ) {
            src = __divine_choice( NUM_OF_DESTINATIONS );
            dst = __divine_choice( NUM_OF_DESTINATIONS - 1 );
            dst = ( dst >= src ? dst + 1 : dst );
            flights[i] = new Flight( i, src, dst,
                                     __divine_choice( MAX_NUM_OF_SEATS - 1 ) + 2 ); // at least 2 seats
        }
    }

    FlightTicket *get_first_free_seat( int src, int dst ) {

        FlightTicket *ticket = NULL;

        for ( int i = 0; i < num_of_flights; i++ ) {
            if ( flights[i]->get_destination() == dst &&
                 flights[i]->get_source() == src ) {
                ticket = flights[i]->get_first_free_seat();
                if ( ticket )
                    return ticket;
            }
        }

        return NULL;
    }

    int num_of_sold_seats() {
        int count = 0;

        for ( int i = 0; i < num_of_flights; i++ )
            count += flights[i]->num_of_sold_seats();

        return count;
    }

    int get_id() {
        return id;
    }
};

// ------------------------------------

class Customer {

  private:

    int id;
    int source, destination;
    FlightTicket *ticket;
    int gen_id;

  public:

    Customer( int id, int src, int dst, int gen_id )
        : id( id ), source( src ), destination( dst ),
          ticket( NULL ), gen_id( gen_id ) {}

    int get_id() {
        return id;
    }

    int get_destination() {
        return destination;
    }

    int get_source() {
        return source;
    }

    void set_ticket( FlightTicket *t ) {
        ticket = t;
    }

    FlightTicket *get_ticket() {
        return ticket;
    }

    int get_generator_id() {
        return gen_id;
    }
};

// ------------------------------------

class InternetSeller {

  private:

    int id;
    pthread_t thread;
    bool terminated;
    int sold;
    Queue< Customer *, QUEUE_SIZE > queue;
    Airlines **airlines; // all airlines

  public:

    static void *run( void *_self ) {
        InternetSeller *self = reinterpret_cast< InternetSeller* >( _self );

        while ( !self->terminated ) {
            while ( self->queue.empty() && !self->terminated );

            if ( self->terminated )
                break;

            InternetSeller::sell_ticket( self );
        }
        return NULL;
    }

    static FlightTicket *sell_ticket( InternetSeller *self ) {
        Customer *customer = self->queue.dequeue();
        FlightTicket *ticket;
        bool bought = false;
        int i = 0;

        while ( !bought && i < NUM_OF_AIRLINES ) {
            do {
                Airlines *airline =  *( self->airlines + i );
                ticket = airline->get_first_free_seat( customer->get_source(),
                                                       customer->get_destination() );
                if ( ticket == NULL )
                    // This airline either doesn't provide any flight on this course,
                    // or all of them are already completely sold.
                    break;
#ifdef BUG
                if ( ticket->is_free() ) {
                    ticket->buy( customer->get_id() );
                    customer->set_ticket( ticket );
                    ++self->sold;
                    bought = true;
                }
#else // correct:
                if ( ticket->buy_with_check( customer->get_id() ) ) { // atomic test & set
                    customer->set_ticket( ticket );
                    ++self->sold;
                    bought = true;
                }
#endif
                if ( bought ) {
                    info( "Customer ID = ",
                          customer->get_generator_id(), "/", customer->get_id(),
                          " bought a seat with the number ", ticket->get_position(),
                          " in a flight from ", ticket->get_source(),
                          " to ", ticket->get_destination(),
                          " provided by the airlines ID = ", airline->get_id(), "." );
                }
            } while ( !bought );
            ++i;
        }

        if ( !bought ) {
            assert( ticket == NULL );
            info( "Failed to find a ticket for the customer ID = ",
                  customer->get_generator_id(), "/", customer->get_id(), "." );
        }

        return ticket;
    }

    void new_customer( Customer *customer, int gen ) {
        queue.enqueue( customer );
        info( "Customer generator with ID = ", gen,
              " enqueued the customer with ID = ", gen, "/", customer->get_id(),
              " to the queue of the internet seller with ID = ", id );
    }

    int num_of_sold() {
        return sold;
    }

    void terminate() {
        terminated = true;
        pthread_join( thread, NULL );
    }

    void start() {
        pthread_create( &thread, NULL, InternetSeller::run, reinterpret_cast< void* >( this ) );
    }

    void join() {
        pthread_join( thread, NULL );
    }

    InternetSeller( int id, Airlines **airlines )
        : id( id ), terminated( false ), sold( 0 ), airlines( airlines ) {}

};

// ------------------------------------

class CustomerGenerator {

  private:

    int id;
    pthread_t thread;
    Customer **customers;
    InternetSeller **sellers; // all internet sellers

  public:

    static void *run( void *_self ) {
        CustomerGenerator *self = reinterpret_cast< CustomerGenerator* >( _self );
        int src, dst;

        for ( int i = 0; i < NUM_OF_CUSTOMS_IN_THREAD; i++ ) {
            src = __divine_choice( NUM_OF_DESTINATIONS );
            dst = __divine_choice( NUM_OF_DESTINATIONS - 1 );
            dst = ( dst >= src ? dst + 1 : dst );
            *( self->customers + i ) = new Customer( i, src, dst, self->id );
            InternetSeller *seller = *( self->sellers + __divine_choice( NUM_OF_STHREADS ) );
            seller->new_customer( *( self->customers + i ), self->id );
        }

        return NULL;
    }

    int get_id() {
        return id;
    }

    void start() {
        pthread_create( &thread, NULL, CustomerGenerator::run, reinterpret_cast< void* >( this ) );
    }

    void join() {
        pthread_join( thread, NULL );
    }

    CustomerGenerator( int id, InternetSeller **sellers )
        : id( id ), sellers( sellers ) {

        customers = new Customer *[ NUM_OF_CUSTOMS_IN_THREAD ];

        for ( int i = 0; i < NUM_OF_CUSTOMS_IN_THREAD; i++ )
            customers[i] = NULL;
    }

    ~CustomerGenerator() {
        for ( int i = 0; i < NUM_OF_CUSTOMS_IN_THREAD; i++ )
            if ( customers[i] != NULL)
                delete customers[i];
        delete[] customers;
    }

};

// ------------------------------------

class Simulator {

  private:

    pthread_t thread;

  public:

    static void *run( void * ) {
        int i;

        Airlines **airlines = new Airlines *[ NUM_OF_AIRLINES ];

        for ( i = 0; i < NUM_OF_AIRLINES; i++ ) {
            airlines[i] = new Airlines( i );
            airlines[i]->generate_flights();
        }

        InternetSeller **sellers = new InternetSeller *[ NUM_OF_STHREADS ];

        for ( i = 0; i < NUM_OF_STHREADS; i++ ) {
            sellers[i] = new InternetSeller( i, airlines );
            sellers[i]->start();
        }

        CustomerGenerator **custgens = new CustomerGenerator *[ NUM_OF_CTHREADS ];

        for ( i = 0; i < NUM_OF_CTHREADS; i++ ) {
            custgens[i] = new CustomerGenerator( i, sellers );
            custgens[i]->start();
        }


        // wait for Customer generators
        for ( i = 0; i < NUM_OF_CTHREADS; i++ ) {
            custgens[i]->join();
        }

        // terminate Internet sellers
        for ( i = 0; i < NUM_OF_CTHREADS; i++ ) {
            sellers[i]->terminate();
            sellers[i]->join();
        }

        // verify corectness
        int tickets = 0, passangers = 0;
        for ( i = 0; i < NUM_OF_AIRLINES; i++ )
            tickets += airlines[i]->num_of_sold_seats();
        for ( i = 0; i < NUM_OF_STHREADS; i++ )
            passangers += sellers[i]->num_of_sold();
        assert( tickets == passangers );

        for ( i = 0; i < NUM_OF_AIRLINES; i++ )
            delete airlines[i];

        for ( i = 0; i < NUM_OF_STHREADS; i++ )
            delete sellers[i];

        for ( i = 0; i < NUM_OF_CTHREADS; i++ )
            delete custgens[i];

        delete[] airlines;
        delete[] sellers;
        delete[] custgens;
        return NULL;
    }

    void start() {
        pthread_create( &thread, NULL, Simulator::run, reinterpret_cast< void* >( this ) );
    }

    void join() {
        pthread_join( thread, NULL );
    }
};

// ------------------------------------

int main() {
    Simulator simulator;
    simulator.start();
    simulator.join();
    return 0;
}

