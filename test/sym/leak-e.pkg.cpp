/* TAGS: sym c++ min todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return TAGS: todo
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit
#include <rst/domains.h>

#include <assert.h>

int main() {
    int x = __sym_val_i32();
    int y = __sym_val_i32();

    int a; /* freeze rewrite test */
    a = x;
    a = y;

    return 0;
}