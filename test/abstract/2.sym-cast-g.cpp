/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int nondet() {
    __sym int x;
    return x;
}

int main() {
    int x;
    x = nondet();
    if ( short( x ) < 0 )
        return 0;
    short &y = *reinterpret_cast< short * >( &x );
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}

