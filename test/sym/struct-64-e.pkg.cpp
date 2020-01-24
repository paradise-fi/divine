/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cstdint>
#include <cassert>

struct Widget {
    int64_t i;
};

void init( Widget * w ) {
    int v = __lamp_any_i32();
    w->i = v % 10;
}

bool check( Widget * w ) {
    return w->i < 10;
}

int main() {
    Widget w;
    init( &w );
    assert( check( &w ) );
}

