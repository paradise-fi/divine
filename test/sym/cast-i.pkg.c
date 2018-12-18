/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <assert.h>

unsigned long __VERIFIER_nondet_ulong( void );
void *foo() { /* creates inttoptr instuction */
    return __VERIFIER_nondet_ulong();
}


int main() {
    _Bool b = foo();
    assert( !b ); /* ERROR */
}
