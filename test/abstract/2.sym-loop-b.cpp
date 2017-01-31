/* VERIFY_OPTS: --symbolic */

#include <assert.h>
#include <cstdint>

#define __sym __attribute__((__annotate__("lart.abstract.symbolic")))

int main() {
    __sym uint16_t x;
    uint32_t y = x;

    __sym int mod;
    for ( int j = mod; j % 8 != 0; ++j )
        y++;

    assert( ( mod % 8 == 0 ) || ( y > 0 ) );
}
