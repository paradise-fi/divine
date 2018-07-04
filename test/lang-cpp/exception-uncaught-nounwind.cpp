/* TAGS: c++ */
#include <cassert>
#include <cstdlib>
#include <exception>

int x = 0;

struct X {
    ~X() { x = 42; }
};

void foo() {
    throw 4;
}

// In DIVINE (as on any platform using Itanium-ABI-based exception handling)
// destructors are not called in case exception is not thrown.
int main() {
    X _;
    std::set_terminate( [] {
            assert( x != 0 ); /* ERROR */
            std::exit( 0 );
        } );
    try {
        foo();
    } catch ( long ) {
        assert( 0 );
    }
    assert( 0 );
}
