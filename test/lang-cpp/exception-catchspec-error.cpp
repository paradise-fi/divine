/* TAGS: c++ */
#include <cassert>

/* EXPECT: --result error --symbol __unexpected */

int x = 0, y = 0, z = 0;

struct X {
    X( int &x ) : v( x ) { }
    ~X() {
        ++v;
    }

    int &v;
};

void foo() {
    X _( x );
    throw 4;
}

void bar() throw () {
    try {
        X _( y );
        foo();
    } catch ( short ) {
        assert( 0 );
    }
}

int main() {
    try {
        X _( z );
        bar();
    } catch ( long ) {
        assert( 0 );
        return 2;
    } catch ( int ) {
        assert( 0 );
        return 2;
    }
    assert( 0 );
    return 1;
}
