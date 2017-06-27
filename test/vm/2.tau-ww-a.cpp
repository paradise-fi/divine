// VERIFY_OPTS: -o nofail:malloc

#include <thread>
#include <atomic>
#include <cassert>

std::atomic_int a, b;

int main() {
    std::thread t( [] {
            a = 1;
            b = 2;
        } );
    assert( a != 1 || b == 2 ); /* ERROR */
    t.join();
}
