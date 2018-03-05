/* TAGS: min c++ */
// VERIFY_OPTS: -o nofail:malloc

#include <thread>
#include <atomic>
#include <cassert>
#include <sys/divm.h>

std::atomic_int a;

int main() {
    std::thread t( [] {
            a = 1;
        } );
    int r1 = a;
    __dios_mask( 1 );
    int r2 = a;
    __dios_mask( 0 );
    assert( r1 == r2 ); /* ERROR */
    t.join();
}
