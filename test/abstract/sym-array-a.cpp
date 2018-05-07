/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>

#include <cstdint>
#include <cassert>

int main() {
    uint64_t array[ 4 ] = { 1, 2, 3, 4 };
    array[ 0 ] = __sym_val_i64();

    if ( array[ 0 ] > array[ 3 ] ) {
        assert( array[ 0 ] - array[ 3 ] > 0 );
    }
}
