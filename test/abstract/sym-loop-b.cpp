/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>
#include <cstdint>

int main() {
    uint16_t x = __sym_val_i16();
    uint32_t y = x;

    int mod = __sym_val_i32();
    for ( int j = mod; j % 8 != 0; ++j )
        y++;

    assert( ( mod % 8 == 0 ) || ( y > 0 ) );
}
