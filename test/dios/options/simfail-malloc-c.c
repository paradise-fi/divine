/* TAGS: min c */
// VERIFY_OPTS: -o nofail:malloc
#include <stdlib.h>

int main() {
    int *a = malloc( sizeof( int ) );
    *a = 42;
    free( a );
}
