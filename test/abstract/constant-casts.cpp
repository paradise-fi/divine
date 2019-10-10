/* TAGS: abstract min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>
#include <limits>
#include <cassert>

int main() {
    auto a = __constant_lift_i32( std::numeric_limits< uint32_t >::max() );
    auto trunced = static_cast< uint8_t >( a );
    assert( trunced == std::numeric_limits< uint8_t >::max() );

    auto b = __constant_lift_i8( std::numeric_limits< uint8_t >::max() );
    auto zext = static_cast< uint32_t >( b );
    assert( zext == std::numeric_limits< uint8_t >::max() );

    int8_t min = -1;
    int8_t c = __constant_lift_i8( min );
    auto sext = static_cast< int32_t >( c );
    assert( sext == static_cast< int32_t >( min ) );
}
