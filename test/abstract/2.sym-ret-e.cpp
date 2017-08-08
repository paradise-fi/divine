/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int nondet() {
    __sym int x;
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
