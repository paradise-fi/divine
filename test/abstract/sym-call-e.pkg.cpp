/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cassert>

int* init_and_pass( int* val ) {
    *val = __sym_val_i32();
    return val;
}

int process( int* addr ) {
    auto ret = init_and_pass( addr );
    return *ret;
}

int main() {
    int i;
    int j = process( &i );
    assert( i == j );
}



