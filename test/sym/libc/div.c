/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */

#include <sys/lamp.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
    unsigned num = __lamp_any_i32();
    div_t ret = div( num, 2 );

    assert( ret.rem == 0 || ret.rem == 1 );

    if ( num > 2 && num < 100 )
        assert( ret.quot >= 1 && ret.quot < 50 );
}
