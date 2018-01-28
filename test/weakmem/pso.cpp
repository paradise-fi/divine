/* TAGS: min c++ pso */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory pso:3 */
#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< int > x, y;
int rx, ry;

int main() {
    __sync_synchronize();
    pthread_t t;
    pthread_create( &t, nullptr, []( void * ) -> void * {
            x.store( 1, std::memory_order_relaxed );
            y.store( 1, std::memory_order_relaxed );
            return nullptr;
        }, nullptr );
    ry = y.load( std::memory_order_relaxed );
    rx = x.load( std::memory_order_relaxed );
    pthread_join( t, nullptr );
    assert( ry != 1 || rx == 1 ); /* ERROR */
}

