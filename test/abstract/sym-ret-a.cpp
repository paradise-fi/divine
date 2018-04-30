/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
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
