/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int main() {
    __sym union {
        int x;
        short y;
    };
    if ( short( x ) < 0 )
        return 0;
    ++y;
    assert( y == -32768 || y >= 1 );
    assert( y != -32768 ); /* ERROR */
}
