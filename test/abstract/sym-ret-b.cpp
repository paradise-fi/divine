/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int nondet() {
    _SYM int x;
    return x;
}

int foo( int b ) {
    int x = nondet() % 42;
    int z = nondet() % 2 + 42;
    if ( b == 0 ) {
        return x;
    }
    return z;
}

int main() {
    int x = foo( 0 );
    assert( x < 42 );
    x = foo( 16 );
    assert( x == 42 || x == 43 || x == 41 );
    assert( x == 42 ); /* ERROR */
}
