/* TAGS: min c */
#include <stdlib.h>

int main() {
    int x;
    int y = x == 42;
    if ( y ) /* ERROR */
        exit( 1 );
    return 0;
}
