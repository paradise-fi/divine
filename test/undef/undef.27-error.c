/* TAGS: min c */
#include <stdlib.h>
#include <stdlib.h>

int main() {
    int x;
    int *mem = malloc( sizeof( int ) );
    if ( mem ) {
        *mem = 42;
        if ( !*mem ) // OK
            exit( 1 );
        memcpy( mem, &x, sizeof( int ));
        if ( *mem ) /* ERROR */
            exit( 1 );
    }
    free( mem );
    return 0;
}
