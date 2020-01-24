/* TAGS: star min */
/* VERIFY_OPTS: */

// V: trivial  V_OPT: --lamp trivial
// V: unit     V_OPT: --lamp unit
// V: symbolic V_OPT: --symbolic TAGS: sym

#include <sys/lamp.h>
#include <assert.h>

int main()
{
    int v = __lamp_any_i32();
    assert( v ); /* ERROR */
}
