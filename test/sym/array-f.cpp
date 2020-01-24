/* TAGS: sym c++ min todo */
/* VERIFY_OPTS: --symbolic */
#include <sys/lamp.h>

#include <cstdint>
#include <cassert>

int main() {
    uint8_t array[ 4 ] = { 0 };
    uint8_t x = __lamp_any_i8();
    array[ 1 ] = x;

    uint32_t val = *reinterpret_cast< uint32_t * >( array );
    assert( ( val & 0xff ) == 0 );
    assert( ( ( val >> 8 ) & 0xff ) == x );
    assert( ( ( val >> 16 ) & 0xff ) == 0 );
    assert( ( ( val >> 24 ) & 0xff ) == 0 );
}