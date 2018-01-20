/* TAGS: min c */
#include <stdlib.h>

int main() {
    int x = 42;
    int *mem = malloc( sizeof( int ) );
    if ( mem ) {
        memcpy( mem, &x, sizeof( int ));
        if ( !*mem )
            exit( 1 );
    }
    free( mem );
    return 0;
}
