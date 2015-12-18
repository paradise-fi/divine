// divine-cflags: -std=c++11
// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <weakmem.h>
#include <atomic>
#include <pthread.h>

enum APs { w0in, w0out, w1in, w1out };
LTL( exclusion, G( w0in -> ((!w1in) W w0out ) ) && G( w1in -> ((!w0in) W w1out )) );

bool flag[2];
int turn;

constexpr int other( int x ) { return x == 0 ? 1 : 0; }
const APs in[] = { w0in, w1in };
const APs out[] = { w0out, w1out };

volatile int critical;

template< int tid >
void *worker( void * ) {
    flag[ tid ] = true;
    turn = other( tid );
    while ( flag[ other( tid ) ] && turn == other( tid ) ) { }
    // critical start
    AP( in[ tid ] );
    ++critical;
    __sync_synchronize();
    --critical;
    __sync_synchronize();
    AP( out[ tid ] );
    // end
    flag[ tid ] = false;
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
