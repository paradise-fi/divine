/* TAGS: min c++ */
// VERIFY_OPTS: -o nofail:malloc

#include <thread>
#include <atomic>
#include <cassert>

std::atomic_int a;

int main() {
    std::thread t( [] {
            a = 1;
        } );
    int r1 = a;
    int r2 = a;
    assert( r1 == r2 ); /* ERROR */
    t.join();
}
