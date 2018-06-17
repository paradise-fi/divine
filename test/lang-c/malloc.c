/* TAGS: min c */
#include <stdlib.h>

int main()
{
    int *m = malloc( sizeof( int ) );
    *m = 32; /* ERROR */
    free( m );
    return 0;
}
