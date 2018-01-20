/* TAGS: min c */
#include <stdlib.h>

int main() {
    int x;
    int y = x;
    if ( y == 0 ) /* ERROR */
        exit( 1 );
    return 0;
}
