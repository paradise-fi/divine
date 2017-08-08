/* VERIFY_OPTS: --symbolic */

#include <assert.h>
#include <cstdint>

#define __sym __attribute__((__annotate__("lart.abstract.sym")))

int main() {
    __sym int x;
    x = x % 32;

    int sum = 0;

    for ( int i = 0; i < x; ++i ) {
        sum += i;
    }

    assert( sum <= 465 );
}
