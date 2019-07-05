/* TAGS: c min */
#include <stdlib.h>

int main()
{
    char *mem = malloc( 4 );
    if ( mem )
        free( mem + 2 ); /* ERROR */
    return 0;
}
