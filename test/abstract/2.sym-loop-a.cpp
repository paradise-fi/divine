/* VERIFY_OPTS: --symbolic */

#include <assert.h>
#include <cstdint>

#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int main() {
    __sym uint16_t x;
    uint32_t y = x;
    for ( int i = 0; i < 10; ++i )
        y++;
    assert( y > 9 );
}
