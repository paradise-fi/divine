/* TAGS: min c */
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

int main()
{
    int *m = malloc( sizeof( int ) );
    if ( m == NULL )
        return 0;
    errno = 0;
    int *m2 = realloc( m, 2 * sizeof( int ) );
    if ( m2 == 0 ) {
        assert( errno == ENOMEM );
        m[0] = 42; /* should still work */
        free( m );
    }
    else
        m2[1] = 42;
    free( m2 );
    return 0;
}
