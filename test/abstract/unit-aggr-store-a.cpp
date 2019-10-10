/* TAGS: star min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <cassert>

int main() {
    auto val = __unit_val_i8();
    auto aggr = static_cast< char * >( __unit_aggregate() );
    aggr[5] = val;
}
