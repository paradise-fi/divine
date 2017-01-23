/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int y = 0;

int main() {
    __sym int x;
    // loaded value should be lifted
    assert( x != y ); /* ERROR */
}
