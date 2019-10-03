/* TAGS: min c++ */
#include <cassert>

/* EXPECT: --result error --symbol __terminate */

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
