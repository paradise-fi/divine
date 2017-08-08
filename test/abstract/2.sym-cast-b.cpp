/* VERIFY_OPTS: --symbolic */

#include <cassert>
#include <limits>
#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int main() {
    __sym int x;
    if ( x < 0 )
        return 0;
    short y = x;
    ++y;
    assert( y != -32768 ); /* ERROR */
//    assert( y != std::numeric_limits< short >::min() ); /* ERROR */
}
