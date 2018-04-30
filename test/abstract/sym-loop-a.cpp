/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>
#include <cstdint>

int main() {
    uint16_t x = __sym_val_i16();

    uint32_t y = x;
    for ( int i = 0; i < 10; ++i )
        y++;
    assert( y > 9 );
}
