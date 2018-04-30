/* TAGS: sym min c++ */
/* VERIFY_OPTS: --symbolic */
#include <rst/domains.h>

#include <cstdint>
#include <cassert>

int main() {
    uint64_t array[ 4 ];
    for ( int i = 0; i < 4; ++i )
        array[ i ] = __sym_val_i64();
    if ( array[ 4 ] ) /* ERROR */
        return 0;
    return 1;
}
