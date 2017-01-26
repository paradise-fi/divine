/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int foo() { return 0; }

int main() {
    __sym int x;
    // returned value should be lifted
    assert( x != foo() ); /* ERROR */
}
