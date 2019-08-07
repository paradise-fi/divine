/* TAGS: sym c todo */
/* VERIFY_OPTS: --symbolic */

#include <rst/domains.h>
#include <stdio.h>
#include <assert.h>

#include "common.h"

int main()
{
    char str[5];
    for( unsigned i = 0; i < 4; i++ )
        str[i] = lower();
    str[4] = '\0';

    int ret = printf( "%s", str );
    assert( ret == 4 );

    return 0;
}
