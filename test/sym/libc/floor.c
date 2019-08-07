/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic -lm */

#include <rst/domains.h>
#include <math.h>
#include <assert.h>

int main()
{
    int x = __sym_val_i32();
    assert( floor( x ) == x );
}
