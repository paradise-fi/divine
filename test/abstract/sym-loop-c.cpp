/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>
#include <assert.h>
#include <cstdint>

int main() {
    int x = __sym_val_i32();
    x = x % 32;

    int sum = 0;

    for ( int i = 0; i < x; ++i ) {
        sum += i;
    }

    assert( sum <= 465 );
}
