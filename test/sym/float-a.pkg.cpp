/* TAGS: sym c++ float todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>

#include <cstdint>
#include <cassert>

int main() {
    float x = __lamp_any_float32();
    float y = 0;
    if ( x != 0 )
        assert( x != y );
}
