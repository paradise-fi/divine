/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
// V: v.leakcheck CC_OPT: -Os V_OPT: --leakcheck exit TAGS: todo

#include <rst/domains.h>
#include <cassert>
#include <cstdint>

struct Widget {
    int64_t value;
};

Widget w;

int main() {
    int val = __sym_val_i32();
    w.value = val % 10;
    assert( w.value < 10 );
}
