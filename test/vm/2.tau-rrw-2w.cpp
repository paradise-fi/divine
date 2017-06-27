// VERIFY_OPTS: -o nofail:malloc

#include <thread>
#include <atomic>
#include <cassert>
#include <cstdint>

union {
    struct {
        std::atomic< int32_t > a;
        std::atomic< int32_t > b;
    };
    std::atomic< int64_t > all;
} x;

std::atomic_int r;

int main() {
    std::thread t( [] {
            int a = x.a;
            int b = x.b;
            r = a ^ b;
        } );
    std::thread t2( [] {
            assert( r == 0 ); /* ERROR */
        } );
    x.all = uint64_t( 42 ) << 32 | uint64_t( 42 );
    t.join();
    t2.join();
}
