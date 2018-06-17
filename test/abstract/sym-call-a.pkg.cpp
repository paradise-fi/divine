/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cassert>

bool is_zero( int v ) { return v == 0; }

int main() {
    int input = __sym_val_i32();
    assert( is_zero( input ) ); /* ERROR */
}
