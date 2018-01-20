/* TAGS: min c++ */
// VERIFY_OPTS: -o nofail:malloc

#include <thread>
#include <atomic>
#include <cassert>

std::atomic_int a;

int main() {
    std::thread t( [] {
            int r = a;
            assert( r == 1 || r == 2 ); /* ERROR */
        } );
    a = 1;
    a = 2;
    t.join();
}
