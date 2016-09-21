#include <stdlib.h>

int main() {
    int *mem = calloc( 1, sizeof( int ) );
    if ( mem && *mem == 0 )
        *mem = 42;
    free( mem );
    return 0;
}
