/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <cassert>
#include <limits>

int sum( int a, int b ) {
    if ( a == 0 )
        return b;
    else
        return sum( a - 1, b + 1 );
}

int main() {
    int b = __sym_val_i32();
    int s = sum( 10, b );
    assert( s == 10 + b );
    return 0;
}
