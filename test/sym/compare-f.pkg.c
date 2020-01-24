/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os

#include <stdlib.h>
#include <assert.h>

#include <sys/lamp.h>

int main() {
    int x = __lamp_any_i32();
    assert( x <= 0 ); /* ERROR */
}
