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

// this test and ...-nounwind test, test that if no hanlder is found our
// unwinder nondeterministically both unwinds and does not unwind the stack
// (and run destructors)
int main() {
    X _;
    std::set_terminate( [] {
            assert( x != 42 ); /* ERROR */
            std::exit( 0 );
        } );
    try {
        foo();
    } catch ( long ) {
        assert( 0 );
    }
    assert( 0 );
}
