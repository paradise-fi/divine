/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os

#include <stdlib.h>
#include <assert.h>

uint32_t __lamp_any_i32(); // test bitcast from different function type

int main() {
    int x = __lamp_any_i32();
    assert( x <= 0 ); /* ERROR */
}
