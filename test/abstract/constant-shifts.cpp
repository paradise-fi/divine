/* TAGS: abstract min */
/* VERIFY_OPTS: --lamp trivial -o nofail:malloc */

#include <sys/lamp.h>
#include <cassert>


int main() {
    uint32_t x1 = __lamp_lift_i32( 5 );
    assert( ( x1 >> 1 ) == 2 );

    int32_t x2 = __lamp_lift_i32( -5 );
    assert( ( x2 >> 1 ) == -3 );

    uint32_t x3 = __lamp_lift_i32( -5 );
    assert( ( x3 >> 1 ) == 0x7FFFFFFD );
}
