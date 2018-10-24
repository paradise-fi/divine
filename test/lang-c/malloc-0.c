/* TAGS: c */
#include <stdlib.h>

int main()
{
    // man malloc: If size is 0, then malloc() returns either NULL, or a unique
    // pointer value that can later be successfully passed to free().
    int *m = malloc( 0 );
    free( m );
    return 0;
}

