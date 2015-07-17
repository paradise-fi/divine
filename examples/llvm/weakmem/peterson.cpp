// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#define __STORE_BUFFER_SIZE 2
#include <weakmem.h>
#include <atomic>
#include <pthread.h>

enum APs { w0in, w0out, w1in, w1out };
LTL( exclusion, G( w0in -> ((!w1in) W w0out ) ) && G( w1in -> ((!w0in) W w1out )) );

bool flag[2] = { false, false };
int turn;

constexpr int other( int x ) { return x == 0 ? 1 : 0; }
const APs in[] = { w0in, w1in };
const APs out[] = { w0out, w1out };

std::atomic< int > critical;

template< int tid >
void *worker( void * ) {
    lart::weakmem::store< lart::weakmem::TSO >( &flag[ tid ], true );
    lart::weakmem::store< lart::weakmem::TSO >( &turn, other( tid ) );
    while ( lart::weakmem::load< lart::weakmem::TSO >( &flag[ other( tid ) ] )
            && lart::weakmem::load< lart::weakmem::TSO >( &turn ) == other( tid ) )
    { }
    // critical start
    AP( in[ tid ] );
    ++critical;
    --critical;
    AP( out[ tid ] );
    // end
    lart::weakmem::store< lart::weakmem::TSO >( &flag[ tid ], false );
    return nullptr;
}

int main() {
    pthread_t t1, t2;
    pthread_create( &t1, nullptr, &worker< 0 >, nullptr );
    pthread_create( &t2, nullptr, &worker< 1 >, nullptr );
    while ( true )
        assert( critical <= 1 );
    pthread_join( t1, nullptr );
    pthread_join( t2, nullptr );
    return 0;
}
