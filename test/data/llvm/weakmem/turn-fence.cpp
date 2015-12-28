// divine-cflags: -std=c++11

#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< bool > turn{ true };
int x{ 0 };

int main() {
    pthread_t t;
    pthread_create( &t, nullptr, []( void * ) -> void * {
            while ( turn.load( std::memory_order_relaxed ) ) { }
            std::atomic_thread_fence( std::memory_order_acquire );
            assert( x == 42 );
            return nullptr;
        }, nullptr );
    x = 42;
    std::atomic_thread_fence( std::memory_order_release );
    turn.store( false, std::memory_order_relaxed );
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
