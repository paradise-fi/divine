/* TAGS: c++ tso ext */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory tso */
#include <atomic>
#include <thread>
#include <cassert>

std::atomic< int > x, y;
int rx, ry;

int main() {
    __sync_synchronize();
    std::thread t( [] {
            x.store( 1, std::memory_order_relaxed );
            ry = y.load( std::memory_order_relaxed );
        } );
    y.store( 1, std::memory_order_relaxed );
    rx = x.load( std::memory_order_relaxed );
    t.join();
    assert( rx == 1 || ry == 1 ); /* ERROR */
}
