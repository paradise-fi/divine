/* TAGS: sym c++ float todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */
/* CC_OPTS: */

// V: v.O0 CC_OPT: -O0 TAGS: min
// V: v.O1 CC_OPT: -O1
// V: v.O2 CC_OPT: -O2
// V: v.Os CC_OPT: -Os
#include <rst/domains.h>

#include <cstdlib>
#include <cstdint>
#include <cassert>

int main() {
    float * a = static_cast< float * >( malloc( sizeof( float ) ) );
    float y = __sym_val_float32();
    *a = y;
    assert( *a == y );
    free( a );
}
