/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>

#include <cassert>

int main() {
    uint64_t x = __lamp_any_i64();
    uint64_t y = __lamp_any_i64();

    y = 0;

    assert( y == 0 );
    assert( x != y ); /* ERROR */
}
