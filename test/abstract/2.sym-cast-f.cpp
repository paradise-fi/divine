/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int nondet() {
    __sym int x;
    return x;
}

int main() {
    union {
        int x;
        short y;
    };
    x = nondet();
    if ( x < 0 )
        return 0;
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}
