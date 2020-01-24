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
unsigned sum( unsigned a, unsigned b );

unsigned sum_impl( unsigned a, unsigned b ) {
    return sum( a - 1, b + 1 );
}

unsigned sum( unsigned a , unsigned b ) {
    if ( b == 0 )
        return a;
    if ( a == 0 )
        return b;
    return sum_impl( a, b );
}


int main() {
    int b = __lamp_any_i32();
    int s = sum( 4, b );
    assert( s == 4 + b );
    return 0;
}
