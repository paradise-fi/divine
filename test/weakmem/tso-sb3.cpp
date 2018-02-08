/* TAGS: min c++ tso ext */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory tso:3 */
/* this test has no property violation for store buffer of size 2, but it has
 * assertion violation for store size 3, therefore it is used as a test that
 * store size configuration works */
#include <atomic>
#include <pthread.h>
#include <cassert>

std::atomic< int > x, y;
std::atomic< int > a, b, c, d;
int rx, ry;

int main() {
    __sync_synchronize();
    pthread_t t;
    pthread_create( &t, nullptr, []( void * ) -> void * {
            x.store( 1, std::memory_order_relaxed );
            a.store( 1, std::memory_order_relaxed );
            b.store( 1, std::memory_order_relaxed );
            ry = y.load( std::memory_order_relaxed );
            return nullptr;
        }, nullptr );
    y.store( 1, std::memory_order_relaxed );
    c.store( 1, std::memory_order_relaxed );
    d.store( 1, std::memory_order_relaxed );
    rx = x.load( std::memory_order_relaxed );
    pthread_join( t, nullptr );
    assert( rx == 1 || ry == 1 ); /* ERROR */
}
