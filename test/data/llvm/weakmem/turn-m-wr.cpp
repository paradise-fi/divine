// divine-cflags: -std=c++11

#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< bool > turn{ true };
std::atomic< int > x{ 0 };

int main() {
    pthread_t t;
    pthread_create( &t, nullptr, []( void * ) -> void * {
            while ( turn.load( std::memory_order_relaxed ) ) { }
            std::atomic_thread_fence( std::memory_order_acquire );
            assert( x.load( std::memory_order_relaxed ) == 42 );
            return nullptr;
        }, nullptr );
    x.store( 42, std::memory_order_relaxed );
    turn.store( false, std::memory_order_release );
    pthread_join( t, nullptr );
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
divine: --csdr
*/
