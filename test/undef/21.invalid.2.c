#include <stdlib.h>

int main() {
    int x;
    int y = x + 4;
    if ( y == 0 ) /* ERROR */
        exit( 1 );
    return 0;
}
