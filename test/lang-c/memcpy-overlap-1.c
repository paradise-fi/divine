/* TAGS: min c */
#include <string.h>

int main()
{
    char *mem = malloc( 4 );
    if ( mem )
    {
        memcpy( mem + 1, mem, 2 ); /* ERROR */
        free( mem );
    }
    return 0;
}
