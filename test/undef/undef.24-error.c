/* TAGS: min c */
#include <stdlib.h>

int main() {
    int *mem = malloc( sizeof( int ) );
    if ( mem && *mem ) /* ERROR */
        *mem = 42;
    free( mem );
    return 0;
}
