/* TAGS: abstract min */
/* VERIFY_OPTS: --lamp trivial -o nofail:malloc */

#include <limits>
#include <cassert>
#include <sys/lamp.h>

int main() {
    auto a = __lamp_lift_i32( std::numeric_limits< uint32_t >::max() );
    auto trunced = static_cast< uint8_t >( a );
    assert( trunced == std::numeric_limits< uint8_t >::max() );

    auto b = __lamp_lift_i8( std::numeric_limits< uint8_t >::max() );
    auto zext = static_cast< uint32_t >( b );
    assert( zext == std::numeric_limits< uint8_t >::max() );

    int8_t min = -1;
    int8_t c = __lamp_lift_i8( min );
    auto sext = static_cast< int32_t >( c );
    assert( sext == static_cast< int32_t >( min ) );
}
