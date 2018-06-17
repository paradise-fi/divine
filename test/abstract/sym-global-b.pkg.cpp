/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cassert>

int x = 7;

int get() { return x; }

int main() {
    assert( x == 7 );

    int val = __sym_val_i32();
    x = x + val;
    assert( val + 7 == x );
}
