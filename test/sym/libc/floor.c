/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic -lm */

#include <sys/lamp.h>
#include <math.h>
#include <assert.h>

int main()
{
    int x = __lamp_any_i32();
    assert( floor( x ) == x );
}
