/* TAGS: min */
#include <stdlib.h>

int main()
{
    void *m = malloc( 4 );
    free( m );
    free( m ); /* ERROR */
}
