/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */

#include <rst/domains.h>
#include <alloca.h>
#include <assert.h>

int main()
{
    unsigned size = __sym_val_i32();
    void *ptr = alloca( size );

    if ( size != 0 )
        assert( ptr != NULL );

    return 0;
}
