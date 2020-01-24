/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <sys/lamp.h>

#include <cstdlib>
#include <cassert>

int main() {
    int *a = static_cast< int* >( malloc( sizeof(int) ) );
    int v = __lamp_any_i32();
    *a = v;
    assert( v == *a );
    free( a );
}


