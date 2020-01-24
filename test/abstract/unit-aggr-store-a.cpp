/* TAGS: star min */
/* VERIFY_OPTS: --lamp unit -o nofail:malloc */

#include <sys/lamp.h>

#include <cassert>

int main() {
    auto val = __lamp_any_i8();
    auto aggr = static_cast< char * >( __lamp_any_array() );
    aggr[5] = val;
}
