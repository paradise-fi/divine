/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int nondet() {
    __sym int x;
    return x;
}

int main() {
    int x;
    x = nondet();
    if ( x < 0 )
        return 0;
    short &y = *reinterpret_cast< short * >( &x );
    y = 0;
    ++y;
    assert( y == 1 );
    assert( x <= 0x7fff0001 );
    assert( x == 0 ); /* ERROR */
}

