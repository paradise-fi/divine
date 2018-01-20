/* TAGS: min c++ */
#include <cassert>

int main() {
    try {
        throw 4;
    } catch ( long x ) {
        assert( 0 );
        return 1;
    } catch ( int x ) {
        assert( x == 4 );
        return 0;
    }
    assert( 0 );
    return 1;
}
