/* VERIFY_OPTS: --symbolic */

#include <cassert>
#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int nondet() {
    __sym int x;
    return x;
}

int foo() {
    int x = nondet();
    return x + 1;
}

int main() {
    int x = foo();
    assert( x == 0 ); /* ERROR */
}
