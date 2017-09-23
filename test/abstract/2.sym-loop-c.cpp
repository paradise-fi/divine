/* VERIFY_OPTS: --symbolic */
#include <abstract/domains.h>
#include <assert.h>
#include <cstdint>

int main() {
    _SYM int x;
    x = x % 32;

    int sum = 0;

    for ( int i = 0; i < x; ++i ) {
        sum += i;
    }

    assert( sum <= 465 );
}
