/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */
/* CC_OPTS: */

#include <sys/lamp.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
    unsigned size = __lamp_any_i32();
    void *ptr = malloc( size );

    if ( size != 0 )
        assert( ptr != NULL );

    free( ptr );
}
