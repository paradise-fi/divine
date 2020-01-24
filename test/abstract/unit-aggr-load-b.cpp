/* TAGS: star min */
/* VERIFY_OPTS: --lamp unit -o nofail:malloc */

#include <sys/lamp.h>

#include <cassert>

int main() {
    auto aggr = static_cast< char * >( __lamp_any_array() );
    auto idx = __lamp_any_i64();
    auto c = aggr[idx];
}
