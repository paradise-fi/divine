/* TAGS: c++ */
#include <cassert>
#include <cstdlib>
#include <exception>

void foo() {
    throw 4;
}

int main() {
    std::set_terminate( [] {
            std::exit( 0 );
        } );
    try {
        foo();
    } catch ( long ) {
        assert( 0 );
    }
    assert( 0 );
}
