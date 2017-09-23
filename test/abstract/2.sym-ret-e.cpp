/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <cassert>

int nondet() {
    _SYM int x;
    return x;
}

int foo( int x ) {
    x = nondet();
    return x;
}

int main() {
    int x;
    foo( x );
    assert( x == 0 ); /* ERROR */
}
