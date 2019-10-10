/* TAGS: star min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <cassert>

int main() {
    auto aggr = static_cast< char * >( __unit_aggregate() );
    auto idx = __unit_val_i64();
    auto c = aggr[idx];
    assert( c ); /* ERROR */
}
