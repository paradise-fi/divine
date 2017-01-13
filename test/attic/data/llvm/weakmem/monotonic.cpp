// divine-cflags: -std=c++11

#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< int > x;

int main() {
    pthread_t t;
    pthread_create( &t, nullptr, []( void * ) -> void * {
            x.fetch_add( 1, std::memory_order_relaxed );
            return nullptr;
        }, nullptr );
    x.fetch_add( 1, std::memory_order_relaxed );
    pthread_join( t, nullptr );
    assert( x == 2 );
}

/* divine-test
holds: true
*/
/* divine-test
lart: weakmem:tso:3
holds: true
*/
/* divine-test
lart: weakmem:std:3
holds: true
*/
