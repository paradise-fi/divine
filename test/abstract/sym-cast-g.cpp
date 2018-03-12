/* TAGS: sym min c++ */
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
    if ( short( x ) < 0 )
        return 0;
    short &y = *reinterpret_cast< short * >( &x );
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}

