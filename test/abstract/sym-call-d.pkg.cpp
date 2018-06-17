/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cassert>

// test output argument
void init( int *i, int v ) { *i = v; }

int main() {
    int val = __sym_val_i32();
    int i;

    init( &i, val );

    assert( i == val );
}
