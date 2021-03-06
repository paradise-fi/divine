/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>

void init( int * i ) {
    int v = __lamp_any_i32();
    *i = v % 10;
}

int main() {
    int i;
    init( &i );
    assert( i < 10 );
}
