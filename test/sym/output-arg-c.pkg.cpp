/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>

void init_impl( int * i, int v ) {
    *i = v % 10;
}

void init( int * i ) {
    int v = __lamp_any_i32();
    init_impl( i, v );
}

int main() {
    int i;
    init( &i );
    assert( i < 10 );
}
