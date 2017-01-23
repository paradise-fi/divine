/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int nondet() {
    __sym int x;
    return x;
}

int foo( int y ) {
    int x = nondet() % 42;
    if ( x == y ) {
        return x;
    }
    return 42;
}

int main() {
    int x = foo( 16 );
    assert( x == 42 || x == 16 );
}
