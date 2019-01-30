/* TAGS: sym c++ min todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit
#include <rst/domains.h>
#include <stdlib.h>
#include <assert.h>

int g;

int main() {
    for (int i = 0; i < 3; ++i) {
        g = __sym_val_i32();
    }

    return 0;
}
