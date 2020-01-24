/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>


int plus( int a, int b ) { return a + b; }

int main() {
    int a = __lamp_any_i32();
    int b = __lamp_any_i32();
    assert( plus( a, 10 ) == a + 10 );
    assert( plus( 10, b ) == 10 + b );
    assert( plus( a, b ) == a + b );
}
