/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <sys/lamp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>

int main()
{
    assert( mkdir( "dir", 0755 ) == 0 );
    int perm = __lamp_any_i32();
    // F_OK = 0, X_OK = 1, W_OK = 2, R_OK = 4
    if( perm > 0 && perm < 8 )
        assert( access( "dir", perm ) == 0 );
}
