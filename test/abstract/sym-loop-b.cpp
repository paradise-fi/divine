/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>
#include <cstdint>

int main() {
    _SYM uint16_t x;
    uint32_t y = x;

    _SYM int mod;
    for ( int j = mod; j % 8 != 0; ++j )
        y++;

    assert( ( mod % 8 == 0 ) || ( y > 0 ) );
}
