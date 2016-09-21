#include <cassert>

/* ERRSPEC: __terminate */

void foo() {
    throw 4;
}

int main() {
    try {
        foo();
    } catch ( long ) {
        assert( 0 );
    }
    assert( 0 );
}
