/* TAGS: min c */
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

int main()
{
    errno = 0;
    int *m = malloc( sizeof( int ) );
    assert( m != NULL || errno == ENOMEM );
    free( m );
    return 0;
}

