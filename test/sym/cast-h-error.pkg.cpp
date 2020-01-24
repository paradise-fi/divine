/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>
#include <limits>

int nondet() {
    return __lamp_any_i32();
}

int main() {
    int x;
    x = nondet();
    if ( x < 0 )
        return 0;
    short &y = *reinterpret_cast< short * >( &x );
    y = 0;
    ++y;
    assert( y == 1 );
    assert( x == 0 ); /* ERROR */
}

