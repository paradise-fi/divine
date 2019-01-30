/* TAGS: sym c++ min */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.return V_OPT: --leakcheck return
// V: v.state V_OPT: --leakcheck state
// V: v.exit V_OPT: --leakcheck exit
#include <rst/domains.h>

#include <array>

#include <assert.h>
#include <stdlib.h>

struct Widget {
    int a = 0, b;
};

int main() {
    std::array< Widget, 5 > arr;
    for ( int i = 0; i < 2; ++i ) {
        for ( auto& w : arr ) {
            w.b = __sym_val_i32();
        }
    }

    return 0;
}
