/* TAGS: c min */
#include <stdlib.h>

int main()
{
    int *mem = malloc( 4 );
    if ( mem )
    {
        free( mem );
        mem[0] = 3; /* ERROR */
    }
    return 0;
}
