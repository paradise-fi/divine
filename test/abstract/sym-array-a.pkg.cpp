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

int main() {
    uint64_t array[ 4 ] = { 1, 2, 3, 4 };
    array[ 0 ] = __sym_val_i64();

    if ( array[ 0 ] > array[ 3 ] ) {
        assert( array[ 0 ] - array[ 3 ] > 0 );
    }
}
