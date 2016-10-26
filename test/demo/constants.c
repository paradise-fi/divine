#include "demo.h"

const int x = 0;
const int y = 42;
int z = 10;

void foo( int *val ) {
    printf( "foo got val %d\n", *val );
    *val = 16; /* ERROR */
    printf( "val set to %d\n", *val );
}

int main() {
    foo( &z );
    foo( &y );
    foo( &x );
}
