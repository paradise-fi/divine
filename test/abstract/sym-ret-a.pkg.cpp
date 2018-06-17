/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cassert>

int nondet() {
    return __sym_val_i32();
}

int foo() {
    int x = nondet();
    return x + 1;
}

int main() {
    int x = foo();
    assert( x == 0 ); /* ERROR */
}
