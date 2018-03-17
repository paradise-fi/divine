/* TAGS: min c++ tso */
/* VERIFY_OPTS: -o nofail:malloc --relaxed-memory tso */
#include <atomic>
#include <cassert>

std::atomic< int > x, y;

void f() {
    std::atomic< int > z;
    z.store( 1, std::memory_order_relaxed );
    assert( z.load( std::memory_order_relaxed ) == 1 );
    z.store( 2, std::memory_order_relaxed );
    assert( z.load( std::memory_order_relaxed ) == 2 );
    y.store( 16, std::memory_order_relaxed );
}

int main() {
    x.store( 42, std::memory_order_relaxed );
    assert( x.load( std::memory_order_relaxed ) == 42 );
    f();
    assert( y.load( std::memory_order_relaxed ) == 16 );
    assert( x.load( std::memory_order_relaxed ) == 42 );
    __sync_synchronize();
    assert( x.load( std::memory_order_relaxed ) == 42 );
    assert( y.load( std::memory_order_relaxed ) == 16 );
}
