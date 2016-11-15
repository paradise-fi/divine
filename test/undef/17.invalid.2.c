#include <stdlib.h>
#include <string.h>

int main() {
    int x;
    int y = 42;
    memmove( &y, &x, sizeof( int ) );
    if ( y == 0 ) /* ERROR */
        exit( 1 );
    return 0;
}
