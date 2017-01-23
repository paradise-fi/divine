/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    union {
        __sym int x;
        short y;
    };
    if ( x < 0 )
        return 0;
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}
