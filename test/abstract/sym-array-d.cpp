/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>

#include <cstdint>
#include <cassert>

int main() {
    uint64_t array[ 4 ] = { 0 };
    uint64_t x = __sym_val_i64();
    array[ 2 ] = x;
    assert( array[ 2 ] == x );
}
