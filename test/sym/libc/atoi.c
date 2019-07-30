/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */

#include <rst/domains.h>
#include <stdlib.h>
#include <assert.h>

#define EXPLICIT
#include "common.h"

int main()
{
    char str[5];
    for( unsigned i = 0; i < 4; i++ )
        str[i] = digit();
    str[4] = '\0';

    int num = atoi( str );
    assert( num >= 0 && num <= 9999 );

    return 0;
}
