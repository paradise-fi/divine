/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1 TAGS: todo
// V: v.O2 CC_OPT: -O2 TAGS: todo
// V: v.Os CC_OPT: -Os TAGS: todo
#include <sys/lamp.h>
#include <assert.h>
#include <cstdint>

int main() {
    int x = __lamp_any_i32();
    x = x % 32;

    int sum = 0;

    for ( int i = 0; i < x; ++i ) {
        sum += i;
    }

    assert( sum <= 465 );
}
