/* TAGS: c++ */

#include <cassert>

void foo() {
    throw 4;
}

void bar() {
    try {
        foo();
    } catch ( short ) {
        assert( 0 );
    }
}

int main() {
    try {
        bar();
    } catch ( long ) {
        assert( 0 );
        return 2;
    } catch ( int x ) {
        assert( x == 4 );
        return 0;
    }
    assert( 0 );
    return 1;
}
