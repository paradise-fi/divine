/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
#include <rst/domains.h>

#include <cstdint>
#include <cassert>

int main() {
    uint64_t array[ 4 ];
    for ( int i = 0; i < 4; ++i )
        array[ i ] = __sym_val_i64();
    if ( array[ 4 ] ) /* ERROR */ // this is undefined behaviour, it is optimized out with -O2
        return 0;
    return 1;
}
