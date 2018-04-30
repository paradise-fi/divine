/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>

int nondet() {
    return __sym_val_i32();
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
