/* TAGS: min threads c++ */
#include <thread>
#include <cassert>
#include <stdexcept>

volatile int x = 0;

int main() {
    // not not disable malloc failure for this test, it is rather small
    // and we should test that there are no malloc-related bugs in std::thread
    try {
        std::thread t( [] { x = 1; } );
        t.join();
        assert( x == 1 );
    } catch ( std::bad_alloc & ) { }
}
