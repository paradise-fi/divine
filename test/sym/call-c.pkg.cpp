/* TAGS: sym c++ */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>
#include <cassert>

int id( int a ) { return a; }

int plus( int a, int b ) { return id( a ) + id( b ); }

int main() {
    int input = __lamp_any_i32();
    assert( plus( input, 10 ) == input + 10 );
}
