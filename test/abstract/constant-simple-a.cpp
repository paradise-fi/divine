/* TAGS: abstract min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {
    auto a = __constant_lift_i64( 10 );
    auto b = __constant_lift_i64( 14 );
    auto c = a + b;
    assert( c == __constant_lift_i64( 24 ) );
}
