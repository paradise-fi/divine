/* TAGS: min c++ */
// VERIFY_OPTS: -o nofail:malloc

#include <thread>
#include <atomic>
#include <cassert>

std::atomic_int a;

int main() {
    std::thread t( [] {
            a = 1;
            a = 2;
        } );
    assert( a == 0 || a == 2 ); /* ERROR */
    t.join();
}
