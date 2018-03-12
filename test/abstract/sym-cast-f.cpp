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
    union {
        int x;
        short y;
    };
    x = nondet();
    if ( short( x ) < 0 )
        return 0;
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}
