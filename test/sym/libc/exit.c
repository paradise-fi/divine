/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <rst/domains.h>
#include <stdlib.h>

int main()
{
    int x = __sym_val_i32();
    if ( x == 0 )
        exit( x );
}

