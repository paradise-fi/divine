/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cstdint>
#include <cassert>

struct S {
    int64_t x;
};

int main() {
    int x = __sym_val_i32();
    S s;
    s.x = x;
    assert( s.x == x );
}
