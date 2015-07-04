// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#define __STORE_BUFFER_SIZE 2
#include "memorder.h"
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
    __dsb_store( &flag[ tid ], true );
    __dsb_store( &turn, other( tid ) );
    while ( __dsb_load( &flag[ other( tid ) ] ) && __dsb_load( &turn ) == other( tid ) ) { }
    // critical start
    AP( in[ tid ] );
    ++critical;
    --critical;
    AP( out[ tid ] );
    // end
    __dsb_store( &flag[ tid ], false );
    return nullptr;
}

int main() {
    pthread_t tid;
    pthread_create( &tid, nullptr, &worker< 0 >, nullptr );
    pthread_create( &tid, nullptr, &worker< 1 >, nullptr );
    while ( true )
        assert( critical <= 1 );
}
