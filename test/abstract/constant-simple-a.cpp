/* TAGS: abstract min */
/* VERIFY_OPTS: --lamp trivial -o nofail:malloc */

#include <assert.h>
#include <sys/lamp.h>

int main()
{
    auto a = __lamp_lift_i64( 10 );
    auto b = __lamp_lift_i64( 14 );
    auto c = a + b;
    assert( c == __lamp_lift_i64( 24 ) );
}
