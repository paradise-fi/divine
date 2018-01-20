/* TAGS: min c */
#include <stdlib.h>

int main() {
    int *a = malloc( sizeof( int ) );
    *a = 42; /* ERROR */
    free( a );
}
