/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */

#include <sys/lamp.h>
#include <stdlib.h>

int main()
{
    int x = __lamp_any_i32();
    if ( x == 0 )
        exit( x );
}

