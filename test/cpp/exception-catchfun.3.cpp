
#include <cassert>

void foo() {
    throw 4;
}

int main() {
    try {
        foo();
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
