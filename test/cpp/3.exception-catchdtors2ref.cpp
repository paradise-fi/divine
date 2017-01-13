#include <cassert>

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

void bar() {
    try {
        X _( y );
        foo();
    } catch ( short ) {
        assert( 0 );
    }
}

void baz() {
    bar();
}

int main() {
    try {
        X _( z );
        baz();
    } catch ( long ) {
        assert( 0 );
        return 2;
    } catch ( int & ) {
        assert( x == 1 );
        assert( y == 1 );
        assert( z == 1 );
        return 0;
    }
    assert( 0 );
    return 1;
}
