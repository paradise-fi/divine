/* TAGS: star min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <cassert>

char * store_ten( char * aggr ) {
    auto idx = __unit_val_i64();
    aggr[idx] = 10;
    return aggr;
}

int main() {
    auto aggr = static_cast< char * >( __unit_aggregate() );
    auto res = store_ten( aggr );
}
