/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <cassert>
#include <limits>

int nondet() {
    return __sym_val_i32();
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
