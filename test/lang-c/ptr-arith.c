/* TAGS: min c */
#include <assert.h>
#include <stdlib.h>

int main()
{
    char *p = malloc( 4 );
    assert( (p + 1) - p == 1 );
    assert( p - (p + 1) == -1 );
    free( p );
    return 0;
}
