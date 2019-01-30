/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit
#include <rst/domains.h>

#include <assert.h>

int nondet() { return __sym_val_i32(); }

int main() {
    int x = nondet();
    return 0;
}
