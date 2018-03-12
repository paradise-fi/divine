/* TAGS: sym todo min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>

int nondet() {
    _SYM int x;
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

