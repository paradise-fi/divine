#include <cassert>

int x = 0;

// note: order of C++ constructors is unspecified
struct X { X() { ++x; } };
struct Y { Y() { ++x; } };

X a, b;
Y c;

int main() {
    assert( x == 3 );
}

struct Dtor {
    ~Dtor() {
        // check destructors run
        assert( false ); /* ERROR */
    }
};

Dtor d;
