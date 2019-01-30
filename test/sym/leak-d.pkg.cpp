/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit
#include <rst/domains.h>

#include <assert.h>

int first(int a, int b) { return a; }

int main() {
    int x = __sym_val_i32();
    int y = __sym_val_i32();

    int z = first(x, y);

    return 0;
}
