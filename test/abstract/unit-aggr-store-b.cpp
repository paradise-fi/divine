/* TAGS: star min */
/* VERIFY_OPTS: --lamp unit -o nofail:malloc */

#include <sys/lamp.h>

#include <cassert>

int main() {
    auto val = __lamp_any_i8();
    auto idx = __lamp_any_i64();
    auto aggr = static_cast< char * >( __lamp_any_array() );
    aggr[idx] = val;
}

