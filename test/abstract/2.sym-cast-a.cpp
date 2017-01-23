/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    __sym int x;
    if ( x < 0 )
        return 0;
    short y = x;
    ++y;
    assert( x + 1 == y ); /* ERROR */
}
