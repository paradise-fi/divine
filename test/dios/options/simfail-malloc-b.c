/* TAGS: min c */
// VERIFY_OPTS: -o simfail:malloc
#include <stdlib.h>

int main() {
    int *a = malloc( sizeof( int ) );
    *a = 42; /* ERROR */
    free( a );
}
