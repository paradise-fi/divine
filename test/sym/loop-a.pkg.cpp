/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>
#include <assert.h>
#include <cstdint>

int main() {
    uint16_t x = __sym_val_i16();

    uint32_t y = x;
    for ( int i = 0; i < 10; ++i )
        y++;
    assert( y > 9 );
}
