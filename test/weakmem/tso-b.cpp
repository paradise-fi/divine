/* TAGS: min c++ tso ext */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory tso */

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
            ry = y.load( std::memory_order_relaxed );
            return nullptr;
        }, nullptr );
    y.store( 1, std::memory_order_relaxed );
    rx = x.load( std::memory_order_relaxed );
    pthread_join( t, nullptr );
    assert( rx == 1 || ry == 1 ); /* ERROR */
}
