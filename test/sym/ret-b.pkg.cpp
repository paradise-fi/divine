/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>

int nondet() {
    return __lamp_any_i32();
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
