/* TAGS: c++ */
#include <cassert>
#include <exception>

void foo() {
    throw 4;
}

int main() {
    std::set_terminate( [] {
            assert( false ); /* ERROR */
        } );
    try {
        foo();
    } catch ( long ) {
        assert( 0 );
    }
    assert( 0 );
}
