/* TAGS: min c++ */
#include "demo.h"

void foo( int *array ) {
    for ( int i = 0; i <= 4; ++i ) {
        printf( "writing at index %d\n", i );
        array[i] = 42; /* ERROR */
    }
}

int main() {
    int x[4];
    foo( x );
    assert( x[3] == 42 );
}
