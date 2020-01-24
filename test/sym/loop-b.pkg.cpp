/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <assert.h>
#include <cstdint>

int main() {
    uint16_t x = __lamp_any_i16();
    uint32_t y = x;

    int mod = __lamp_any_i32();
    for ( int j = mod; j % 8 != 0; ++j )
        y++;

    assert( ( mod % 8 == 0 ) || ( y > 0 ) );
}
