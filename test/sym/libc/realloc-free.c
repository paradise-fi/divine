/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */
/* CC_OPTS: */

#include <sys/lamp.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
    void *ptr = malloc( 20 );
    assert( ptr != NULL );

    unsigned size = __lamp_any_i32();
    void *ptr2 = realloc( ptr, size );

    if ( size != 0 )
        assert( ptr2 != NULL );

    free( ptr2 );
}
