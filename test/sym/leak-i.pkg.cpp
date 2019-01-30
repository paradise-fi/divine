/* TAGS: sym c++ min todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit
#include <rst/domains.h>

#include <assert.h>
#include <stdlib.h>

struct Widget {
    int a = 0, b;
};

int main() {
    for ( int i = 0; i < 3; ++i ) {
        Widget w;
        w.b = __sym_val_i32();
    }

    return 0;
}
