/* TAGS: sym c++ todo */
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
    uint32_t array[ 2 ] = { 0 };
    uint32_t x = __sym_val_i32();
    array[ 1 ] = x;

    uint64_t val = *reinterpret_cast< uint64_t * >( array );
    assert( array[ 1 ] == val );
}