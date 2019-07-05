/* TAGS: c min */
#include <string.h>

int main()
{
    char *mem = malloc( 4 );
    if ( mem )
    {
        memcpy( mem, mem + 1, 2 ); /* ERROR */
        free( mem );
    }
    return 0;
}
