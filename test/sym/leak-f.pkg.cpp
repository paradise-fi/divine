/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return VERIFY_OPTS: --leakcheck return
// V: v.state VERIFY_OPTS: --leakcheck state
// V: v.exit VERIFY_OPTS: --leakcheck exit
#include <rst/domains.h>

#include <assert.h>

int main() {
    int x = __sym_val_i32();
    x = __sym_val_i32();

    return 0;
}
